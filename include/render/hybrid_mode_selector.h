#pragma once

#include <memory>
#include <vector>
#include <deque>
#include <chrono>

// Forward declarations
class GPUPerformanceMonitor;
class GPUHardwareOptimizer;

class HybridModeSelector {
public:
    enum class SelectionMode {
        ALWAYS_GPU,
        ALWAYS_CPU,
        PERFORMANCE_BASED,
        ADAPTIVE
    };

    struct SceneCharacteristics {
        int width = 0;
        int height = 0;
        int samples = 0;
        int primitiveCount = 0;
        double complexity = 1.0;
        bool hasComplexMaterials = false;
        bool hasVolumetricLighting = false;
    };

    struct PerformanceModel {
        double gpuBaseTime = 0.001;      // Base GPU overhead (ms)
        double gpuPixelFactor = 0.00001; // Time per pixel (ms)
        double gpuSampleFactor = 0.0001; // Time per sample (ms)
        double cpuBaseTime = 0.0005;     // Base CPU overhead (ms)
        double cpuPixelFactor = 0.0001;  // Time per pixel (ms)
        double cpuSampleFactor = 0.001;  // Time per sample (ms)
        double memoryTransferCost = 0.01; // Memory transfer overhead
        double gpuSetupCost = 0.5;       // GPU initialization cost (ms)
    };

    HybridModeSelector();
    ~HybridModeSelector() = default;

    // Main decision methods
    bool shouldUseGPU(int width, int height, int samples, int primitiveCount = 0) const;
    bool shouldUseGPU(const SceneCharacteristics& scene) const;
    
    // Performance prediction
    double predictGPUTime(int width, int height, int samples) const;
    double predictCPUTime(int width, int height, int samples) const;
    double predictGPUTime(const SceneCharacteristics& scene) const;
    double predictCPUTime(const SceneCharacteristics& scene) const;
    double getExpectedSpeedup(int width, int height, int samples) const;
    double getExpectedSpeedup(const SceneCharacteristics& scene) const;

    // Performance model management
    void updatePerformanceModel(const SceneCharacteristics& scene, 
                               double actualGPUTime, double actualCPUTime);
    void calibratePerformanceModel();
    
    // Configuration
    void setSelectionMode(SelectionMode mode) { mode_ = mode; }
    SelectionMode getSelectionMode() const { return mode_; }
    void setPerformanceThreshold(double threshold) { performanceThreshold_ = threshold; }
    void setMemoryThreshold(size_t threshold) { memoryThreshold_ = threshold; }
    
    // GPU availability and validation
    bool isGPUAvailable() const;
    bool hasAdequateGPUMemory(const SceneCharacteristics& scene) const;
    
    // Adaptive thresholds
    void enableAdaptiveThresholds(bool enable) { adaptiveThresholds_ = enable; }
    void updateAdaptiveThresholds();
    
    // Scene complexity analysis
    double calculateSceneComplexity(const SceneCharacteristics& scene) const;
    void updateComplexityModel(const SceneCharacteristics& scene, double actualComplexity);
    
    // Performance monitoring integration
    void setPerformanceMonitor(std::shared_ptr<GPUPerformanceMonitor> monitor);
    void setHardwareOptimizer(std::shared_ptr<GPUHardwareOptimizer> optimizer);

private:
    SelectionMode mode_;
    double performanceThreshold_;    // Minimum speedup required for GPU
    size_t memoryThreshold_;        // Maximum GPU memory usage (bytes)
    bool adaptiveThresholds_;
    
    PerformanceModel performanceModel_;
    
    // Performance monitoring integration
    std::shared_ptr<GPUPerformanceMonitor> performanceMonitor_;
    std::shared_ptr<GPUHardwareOptimizer> hardwareOptimizer_;
    
    // Historical performance data for adaptive learning
    struct PerformanceRecord {
        SceneCharacteristics scene;
        double actualGPUTime;
        double actualCPUTime;
        double predictedGPUTime;
        double predictedCPUTime;
        std::chrono::system_clock::time_point timestamp;
    };
    std::deque<PerformanceRecord> performanceHistory_;
    static const size_t MAX_HISTORY_SIZE = 50;
    
    // Adaptive threshold tracking
    struct ThresholdData {
        double averageSpeedup;
        double successRate;
        int totalDecisions;
        int correctDecisions;
    };
    ThresholdData thresholdData_;
    
    // Internal methods
    double calculateMemoryRequirement(const SceneCharacteristics& scene) const;
    double adjustForComplexity(double baseTime, const SceneCharacteristics& scene) const;
    void updatePerformanceHistory(const PerformanceRecord& record);
    void analyzePerformanceHistory();
    bool shouldUseGPUPerformanceBased(const SceneCharacteristics& scene) const;
    bool shouldUseGPUAdaptive(const SceneCharacteristics& scene) const;
    void updateModelAccuracy();
};