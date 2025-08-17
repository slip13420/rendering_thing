#include "render/hybrid_mode_selector.h"
#include "render/gpu_performance.h"
#include "render/gpu_hardware_optimizer.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <numeric>

HybridModeSelector::HybridModeSelector()
    : mode_(SelectionMode::ADAPTIVE)
    , performanceThreshold_(2.0) // Require 2x speedup minimum
    , memoryThreshold_(2ULL * 1024 * 1024 * 1024) // 2GB limit
    , adaptiveThresholds_(true)
{
    // Initialize performance model with reasonable defaults
    performanceModel_.gpuBaseTime = 0.001;
    performanceModel_.gpuPixelFactor = 0.00001;
    performanceModel_.gpuSampleFactor = 0.0001;
    performanceModel_.cpuBaseTime = 0.0005;
    performanceModel_.cpuPixelFactor = 0.0001;
    performanceModel_.cpuSampleFactor = 0.001;
    performanceModel_.memoryTransferCost = 0.01;
    performanceModel_.gpuSetupCost = 0.5;
    
    // Initialize threshold tracking
    thresholdData_.averageSpeedup = 1.0;
    thresholdData_.successRate = 0.5;
    thresholdData_.totalDecisions = 0;
    thresholdData_.correctDecisions = 0;
}

bool HybridModeSelector::shouldUseGPU(int width, int height, int samples, int primitiveCount) const {
    SceneCharacteristics scene;
    scene.width = width;
    scene.height = height;
    scene.samples = samples;
    scene.primitiveCount = primitiveCount;
    scene.complexity = calculateSceneComplexity(scene);
    
    return shouldUseGPU(scene);
}

bool HybridModeSelector::shouldUseGPU(const SceneCharacteristics& scene) const {
    switch (mode_) {
        case SelectionMode::ALWAYS_GPU:
            return isGPUAvailable() && hasAdequateGPUMemory(scene);
            
        case SelectionMode::ALWAYS_CPU:
            return false;
            
        case SelectionMode::PERFORMANCE_BASED:
            return shouldUseGPUPerformanceBased(scene);
            
        case SelectionMode::ADAPTIVE:
            return shouldUseGPUAdaptive(scene);
    }
    
    return false;
}

bool HybridModeSelector::shouldUseGPUPerformanceBased(const SceneCharacteristics& scene) const {
    if (!isGPUAvailable() || !hasAdequateGPUMemory(scene)) {
        return false;
    }
    
    double expectedSpeedup = getExpectedSpeedup(scene);
    return expectedSpeedup >= performanceThreshold_;
}

bool HybridModeSelector::shouldUseGPUAdaptive(const SceneCharacteristics& scene) const {
    if (!isGPUAvailable() || !hasAdequateGPUMemory(scene)) {
        return false;
    }
    
    // Use adaptive threshold based on historical performance
    double adaptiveThreshold = performanceThreshold_;
    
    if (adaptiveThresholds_ && thresholdData_.totalDecisions > 10) {
        // Adjust threshold based on success rate
        if (thresholdData_.successRate > 0.8) {
            adaptiveThreshold *= 0.9; // Lower threshold if we're doing well
        } else if (thresholdData_.successRate < 0.6) {
            adaptiveThreshold *= 1.1; // Raise threshold if we're making bad decisions
        }
    }
    
    double expectedSpeedup = getExpectedSpeedup(scene);
    
    // Also consider scene complexity
    double complexityFactor = 1.0 + (scene.complexity - 1.0) * 0.2;
    double adjustedThreshold = adaptiveThreshold / complexityFactor;
    
    return expectedSpeedup >= adjustedThreshold;
}

double HybridModeSelector::predictGPUTime(int width, int height, int samples) const {
    SceneCharacteristics scene;
    scene.width = width;
    scene.height = height;
    scene.samples = samples;
    scene.complexity = 1.0;
    
    return predictGPUTime(scene);
}

double HybridModeSelector::predictCPUTime(int width, int height, int samples) const {
    SceneCharacteristics scene;
    scene.width = width;
    scene.height = height;
    scene.samples = samples;
    scene.complexity = 1.0;
    
    return predictCPUTime(scene);
}

double HybridModeSelector::predictGPUTime(const SceneCharacteristics& scene) const {
    double pixels = static_cast<double>(scene.width * scene.height);
    double samples = static_cast<double>(scene.samples);
    
    double baseTime = performanceModel_.gpuBaseTime;
    double computeTime = pixels * performanceModel_.gpuPixelFactor + 
                        pixels * samples * performanceModel_.gpuSampleFactor;
    double transferTime = performanceModel_.memoryTransferCost * pixels;
    double setupTime = performanceModel_.gpuSetupCost;
    
    double totalTime = baseTime + computeTime + transferTime + setupTime;
    
    // Adjust for scene complexity
    return adjustForComplexity(totalTime, scene);
}

double HybridModeSelector::predictCPUTime(const SceneCharacteristics& scene) const {
    double pixels = static_cast<double>(scene.width * scene.height);
    double samples = static_cast<double>(scene.samples);
    
    double baseTime = performanceModel_.cpuBaseTime;
    double computeTime = pixels * performanceModel_.cpuPixelFactor + 
                        pixels * samples * performanceModel_.cpuSampleFactor;
    
    double totalTime = baseTime + computeTime;
    
    // Adjust for scene complexity
    return adjustForComplexity(totalTime, scene);
}

double HybridModeSelector::getExpectedSpeedup(int width, int height, int samples) const {
    double cpuTime = predictCPUTime(width, height, samples);
    double gpuTime = predictGPUTime(width, height, samples);
    
    if (gpuTime > 0) {
        return cpuTime / gpuTime;
    }
    return 0.0;
}

double HybridModeSelector::getExpectedSpeedup(const SceneCharacteristics& scene) const {
    double cpuTime = predictCPUTime(scene);
    double gpuTime = predictGPUTime(scene);
    
    if (gpuTime > 0) {
        return cpuTime / gpuTime;
    }
    return 0.0;
}

void HybridModeSelector::updatePerformanceModel(const SceneCharacteristics& scene, 
                                               double actualGPUTime, double actualCPUTime) {
    // Create performance record
    PerformanceRecord record;
    record.scene = scene;
    record.actualGPUTime = actualGPUTime;
    record.actualCPUTime = actualCPUTime;
    record.predictedGPUTime = predictGPUTime(scene);
    record.predictedCPUTime = predictCPUTime(scene);
    record.timestamp = std::chrono::system_clock::now();
    
    updatePerformanceHistory(record);
    
    // Update model parameters based on actual vs predicted performance
    double pixels = static_cast<double>(scene.width * scene.height);
    double samples = static_cast<double>(scene.samples);
    
    // Simple adaptive learning - adjust factors based on prediction error
    if (actualGPUTime > 0 && record.predictedGPUTime > 0) {
        double gpuError = actualGPUTime / record.predictedGPUTime;
        if (gpuError > 1.2 || gpuError < 0.8) {
            // Significant error - adjust model
            performanceModel_.gpuPixelFactor *= (gpuError * 0.1 + 0.9);
            performanceModel_.gpuSampleFactor *= (gpuError * 0.1 + 0.9);
        }
    }
    
    if (actualCPUTime > 0 && record.predictedCPUTime > 0) {
        double cpuError = actualCPUTime / record.predictedCPUTime;
        if (cpuError > 1.2 || cpuError < 0.8) {
            // Significant error - adjust model
            performanceModel_.cpuPixelFactor *= (cpuError * 0.1 + 0.9);
            performanceModel_.cpuSampleFactor *= (cpuError * 0.1 + 0.9);
        }
    }
    
    // Update threshold tracking
    thresholdData_.totalDecisions++;
    double actualSpeedup = (actualGPUTime > 0 && actualCPUTime > 0) ? 
                          actualCPUTime / actualGPUTime : 0.0;
    
    bool correctDecision = (actualSpeedup >= performanceThreshold_) == 
                          shouldUseGPU(scene);
    if (correctDecision) {
        thresholdData_.correctDecisions++;
    }
    
    thresholdData_.successRate = static_cast<double>(thresholdData_.correctDecisions) / 
                                thresholdData_.totalDecisions;
    thresholdData_.averageSpeedup = (thresholdData_.averageSpeedup * 0.9) + (actualSpeedup * 0.1);
    
    std::cout << "Updated performance model - GPU: " << actualGPUTime 
              << "ms, CPU: " << actualCPUTime << "ms, Speedup: " << actualSpeedup 
              << "x, Success rate: " << thresholdData_.successRate << std::endl;
}

void HybridModeSelector::calibratePerformanceModel() {
    if (performanceHistory_.size() < 5) {
        std::cout << "Insufficient data for performance model calibration" << std::endl;
        return;
    }
    
    analyzePerformanceHistory();
    updateModelAccuracy();
    
    std::cout << "Performance model calibrated with " << performanceHistory_.size() 
              << " data points" << std::endl;
}

double HybridModeSelector::calculateSceneComplexity(const SceneCharacteristics& scene) const {
    double baseComplexity = 1.0;
    
    // Factor in primitive count
    if (scene.primitiveCount > 100) {
        baseComplexity += (scene.primitiveCount - 100) * 0.001;
    }
    
    // Factor in complex materials
    if (scene.hasComplexMaterials) {
        baseComplexity *= 1.5;
    }
    
    // Factor in volumetric lighting
    if (scene.hasVolumetricLighting) {
        baseComplexity *= 2.0;
    }
    
    // Clamp complexity to reasonable bounds
    return std::max(0.5, std::min(baseComplexity, 5.0));
}

double HybridModeSelector::adjustForComplexity(double baseTime, const SceneCharacteristics& scene) const {
    return baseTime * scene.complexity;
}

bool HybridModeSelector::isGPUAvailable() const {
    // Check with hardware optimizer if available
    if (hardwareOptimizer_) {
        return hardwareOptimizer_->getHardwareProfile().maxComputeUnits > 0;
    }
    
    // Default check - this would normally query OpenGL capabilities
#ifdef USE_GPU
    return true;
#else
    return false;
#endif
}

bool HybridModeSelector::hasAdequateGPUMemory(const SceneCharacteristics& scene) const {
    size_t requiredMemory = static_cast<size_t>(calculateMemoryRequirement(scene));
    return requiredMemory <= memoryThreshold_;
}

double HybridModeSelector::calculateMemoryRequirement(const SceneCharacteristics& scene) const {
    // Estimate memory requirement
    size_t pixels = static_cast<size_t>(scene.width * scene.height);
    size_t bytesPerPixel = 16; // RGBA32F = 16 bytes per pixel
    size_t imageMemory = pixels * bytesPerPixel;
    
    // Add buffers for scene data, random numbers, etc.
    size_t sceneMemory = scene.primitiveCount * 64; // Estimate 64 bytes per primitive
    size_t randomMemory = pixels * 4; // Random seed buffer
    
    return static_cast<double>(imageMemory + sceneMemory + randomMemory);
}

void HybridModeSelector::updatePerformanceHistory(const PerformanceRecord& record) {
    performanceHistory_.push_back(record);
    
    // Maintain maximum history size
    if (performanceHistory_.size() > MAX_HISTORY_SIZE) {
        performanceHistory_.pop_front();
    }
}

void HybridModeSelector::analyzePerformanceHistory() {
    if (performanceHistory_.size() < 3) return;
    
    // Calculate average prediction accuracy
    double totalGPUError = 0.0;
    double totalCPUError = 0.0;
    int validSamples = 0;
    
    for (const auto& record : performanceHistory_) {
        if (record.actualGPUTime > 0 && record.predictedGPUTime > 0) {
            double gpuError = std::abs(record.actualGPUTime - record.predictedGPUTime) / 
                             record.actualGPUTime;
            totalGPUError += gpuError;
        }
        
        if (record.actualCPUTime > 0 && record.predictedCPUTime > 0) {
            double cpuError = std::abs(record.actualCPUTime - record.predictedCPUTime) / 
                             record.actualCPUTime;
            totalCPUError += cpuError;
            validSamples++;
        }
    }
    
    if (validSamples > 0) {
        double avgGPUError = totalGPUError / validSamples;
        double avgCPUError = totalCPUError / validSamples;
        
        std::cout << "Performance model accuracy - GPU error: " << (avgGPUError * 100.0) 
                  << "%, CPU error: " << (avgCPUError * 100.0) << "%" << std::endl;
    }
}

void HybridModeSelector::updateModelAccuracy() {
    // Analyze recent performance and adjust model confidence
    if (performanceHistory_.size() < 10) return;
    
    // Look at last 10 decisions
    auto recentStart = performanceHistory_.end() - 10;
    int correctDecisions = 0;
    
    for (auto it = recentStart; it != performanceHistory_.end(); ++it) {
        double actualSpeedup = (it->actualGPUTime > 0 && it->actualCPUTime > 0) ?
                              it->actualCPUTime / it->actualGPUTime : 0.0;
        
        bool wouldChooseGPU = shouldUseGPU(it->scene);
        bool shouldHaveChosenGPU = actualSpeedup >= performanceThreshold_;
        
        if (wouldChooseGPU == shouldHaveChosenGPU) {
            correctDecisions++;
        }
    }
    
    double recentAccuracy = correctDecisions / 10.0;
    
    // Adjust threshold based on recent accuracy
    if (adaptiveThresholds_) {
        if (recentAccuracy > 0.8) {
            performanceThreshold_ *= 0.95; // Lower threshold slightly
        } else if (recentAccuracy < 0.6) {
            performanceThreshold_ *= 1.05; // Raise threshold slightly
        }
        
        // Keep threshold in reasonable bounds
        performanceThreshold_ = std::max(1.5, std::min(performanceThreshold_, 5.0));
    }
}

void HybridModeSelector::updateAdaptiveThresholds() {
    if (!adaptiveThresholds_ || thresholdData_.totalDecisions < 20) {
        return;
    }
    
    // Adjust performance threshold based on success rate
    if (thresholdData_.successRate > 0.85) {
        performanceThreshold_ *= 0.98; // Gradually lower threshold
    } else if (thresholdData_.successRate < 0.65) {
        performanceThreshold_ *= 1.02; // Gradually raise threshold
    }
    
    // Keep threshold in reasonable bounds
    performanceThreshold_ = std::max(1.2, std::min(performanceThreshold_, 10.0));
    
    std::cout << "Adaptive threshold updated to " << performanceThreshold_ 
              << " (success rate: " << thresholdData_.successRate << ")" << std::endl;
}

void HybridModeSelector::setPerformanceMonitor(std::shared_ptr<GPUPerformanceMonitor> monitor) {
    performanceMonitor_ = monitor;
}

void HybridModeSelector::setHardwareOptimizer(std::shared_ptr<GPUHardwareOptimizer> optimizer) {
    hardwareOptimizer_ = optimizer;
}