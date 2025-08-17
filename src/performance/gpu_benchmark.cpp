#include "performance/gpu_benchmark.h"
#include "render/path_tracer.h"
#include "render/gpu_performance.h"
#include "render/gpu_hardware_optimizer.h"
#include "render/hybrid_mode_selector.h"
#include "core/scene_manager.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <cmath>

GPUBenchmarkSuite::GPUBenchmarkSuite() {
    // Initialize with default configuration
    config_.enableCPUComparison = true;
    config_.enableMemoryProfiling = true;
    config_.enableRegressionDetection = true;
    config_.warmupIterations = 2;
    config_.benchmarkIterations = 5;
    config_.targetSpeedupMinimum = 5.0;
    config_.memoryOverheadMaximum = 5.0;
}

std::vector<GPUBenchmarkSuite::BenchmarkResult> GPUBenchmarkSuite::runFullBenchmarkSuite() {
    std::vector<BenchmarkResult> results;
    
    std::cout << "=== Starting Full GPU Benchmark Suite ===" << std::endl;
    
    if (!pathTracer_) {
        std::cerr << "Error: PathTracer not configured for benchmarking" << std::endl;
        return results;
    }
    
    // Run all standard benchmark scenarios
    std::vector<std::string> scenarios = {
        "SimpleScene",
        "ComplexScene", 
        "HighSampleCount",
        "LargePrimitiveCount",
        "LargeResolution",
        "ProgressiveRendering"
    };
    
    for (const auto& scenario : scenarios) {
        try {
            std::cout << "Running benchmark: " << scenario << std::endl;
            BenchmarkResult result = runSingleBenchmark(scenario);
            
            if (isResultValid(result)) {
                results.push_back(result);
                logBenchmarkResult(result);
            } else {
                std::cout << "Warning: Invalid result for " << scenario << " - " << result.errorMessage << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error running benchmark " << scenario << ": " << e.what() << std::endl;
        }
    }
    
    // Run scaling validation tests
    auto scalingResults = benchmarkScalingValidation();
    results.insert(results.end(), scalingResults.begin(), scalingResults.end());
    
    // Calculate overall statistics
    calculateStatistics(results);
    
    // Generate comprehensive report
    generateBenchmarkReport(results);
    
    // Store results for regression testing
    benchmarkHistory_.insert(benchmarkHistory_.end(), results.begin(), results.end());
    
    std::cout << "=== Benchmark Suite Complete ===" << std::endl;
    std::cout << "Total scenarios: " << results.size() << std::endl;
    std::cout << "Performance targets met: " << (validatePerformanceTargets(results) ? "YES" : "NO") << std::endl;
    
    return results;
}

GPUBenchmarkSuite::BenchmarkResult GPUBenchmarkSuite::runSingleBenchmark(const std::string& scenario) {
    if (scenario == "SimpleScene") {
        return benchmarkSimpleScene();
    } else if (scenario == "ComplexScene") {
        return benchmarkComplexScene();
    } else if (scenario == "HighSampleCount") {
        return benchmarkHighSampleCount();
    } else if (scenario == "LargePrimitiveCount") {
        return benchmarkLargePrimitiveCount();
    } else if (scenario == "LargeResolution") {
        return benchmarkLargeResolution();
    } else if (scenario == "ProgressiveRendering") {
        return benchmarkProgressiveRendering();
    } else {
        BenchmarkResult result;
        result.scenarioName = scenario;
        result.errorMessage = "Unknown benchmark scenario";
        return result;
    }
}

GPUBenchmarkSuite::BenchmarkResult GPUBenchmarkSuite::benchmarkSimpleScene() {
    return runBenchmarkScenario("SimpleScene", 512, 512, 10, 50);
}

GPUBenchmarkSuite::BenchmarkResult GPUBenchmarkSuite::benchmarkComplexScene() {
    return runBenchmarkScenario("ComplexScene", 512, 512, 10, 500);
}

GPUBenchmarkSuite::BenchmarkResult GPUBenchmarkSuite::benchmarkHighSampleCount() {
    return runBenchmarkScenario("HighSampleCount", 256, 256, 100, 100);
}

GPUBenchmarkSuite::BenchmarkResult GPUBenchmarkSuite::benchmarkLargePrimitiveCount() {
    return runBenchmarkScenario("LargePrimitiveCount", 512, 512, 10, 2000);
}

GPUBenchmarkSuite::BenchmarkResult GPUBenchmarkSuite::benchmarkLargeResolution() {
    return runBenchmarkScenario("LargeResolution", 1024, 1024, 10, 100);
}

GPUBenchmarkSuite::BenchmarkResult GPUBenchmarkSuite::benchmarkProgressiveRendering() {
    // Special case for progressive rendering benchmark
    BenchmarkResult result;
    result.scenarioName = "ProgressiveRendering";
    result.imageWidth = 512;
    result.imageHeight = 512;
    result.samplesPerPixel = 50;
    result.primitiveCount = 100;
    result.timestamp = std::chrono::system_clock::now();
    
    setupBenchmarkScene(result.primitiveCount);
    
    try {
        // Measure progressive GPU performance
        if (performanceMonitor_) {
            performanceMonitor_->reset();
            performanceMonitor_->startGPUTiming();
        }
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // Simulate progressive rendering with multiple steps
        for (int step = 1; step <= 5; ++step) {
            int stepSamples = result.samplesPerPixel / 5;
#ifdef USE_GPU
            if (pathTracer_->isGPUAvailable()) {
                pathTracer_->trace_gpu(result.imageWidth, result.imageHeight);
            }
#endif
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        result.gpuTime = duration.count() / 1000.0; // Convert to milliseconds
        
        if (performanceMonitor_) {
            performanceMonitor_->endGPUTiming();
            auto metrics = performanceMonitor_->getMetrics();
            result.memoryTransferTime = metrics.gpuMemoryTransferTime;
            result.memoryUsage = metrics.gpuMemoryUsed / (1024.0 * 1024.0); // Convert to MB
        }
        
        // Measure CPU comparison if enabled
        if (config_.enableCPUComparison) {
            result.cpuTime = measureCPUPerformance(result.imageWidth, result.imageHeight, result.samplesPerPixel);
        }
        
        // Calculate speedup
        if (result.cpuTime > 0 && result.gpuTime > 0) {
            result.speedupRatio = result.cpuTime / result.gpuTime;
        }
        
        result.meetsPerformanceTarget = result.speedupRatio >= config_.targetSpeedupMinimum;
        
        validateBenchmarkAccuracy(result);
        
    } catch (const std::exception& e) {
        result.errorMessage = e.what();
    }
    
    return result;
}

GPUBenchmarkSuite::BenchmarkResult GPUBenchmarkSuite::runBenchmarkScenario(const std::string& name, 
                                                                           int width, int height, 
                                                                           int samples, int primitives) {
    BenchmarkResult result;
    result.scenarioName = name;
    result.imageWidth = width;
    result.imageHeight = height;
    result.samplesPerPixel = samples;
    result.primitiveCount = primitives;
    result.timestamp = std::chrono::system_clock::now();
    
    setupBenchmarkScene(primitives);
    
    try {
        // Warmup iterations
        for (int i = 0; i < config_.warmupIterations; ++i) {
#ifdef USE_GPU
            if (pathTracer_->isGPUAvailable()) {
                pathTracer_->trace_gpu(width, height);
            }
#endif
        }
        
        // Benchmark iterations
        std::vector<double> gpuTimes;
        std::vector<double> cpuTimes;
        
        for (int i = 0; i < config_.benchmarkIterations; ++i) {
            // Measure GPU performance
            double gpuTime = measureGPUPerformance(width, height, samples);
            if (gpuTime > 0) {
                gpuTimes.push_back(gpuTime);
            }
            
            // Measure CPU performance if enabled
            if (config_.enableCPUComparison) {
                double cpuTime = measureCPUPerformance(width, height, samples);
                if (cpuTime > 0) {
                    cpuTimes.push_back(cpuTime);
                }
            }
        }
        
        // Calculate averages
        if (!gpuTimes.empty()) {
            result.gpuTime = std::accumulate(gpuTimes.begin(), gpuTimes.end(), 0.0) / gpuTimes.size();
        }
        
        if (!cpuTimes.empty()) {
            result.cpuTime = std::accumulate(cpuTimes.begin(), cpuTimes.end(), 0.0) / cpuTimes.size();
        }
        
        // Measure memory transfer overhead
        if (config_.enableMemoryProfiling) {
            result.memoryTransferTime = measureMemoryTransferOverhead(width, height);
        }
        
        // Calculate speedup
        if (result.cpuTime > 0 && result.gpuTime > 0) {
            result.speedupRatio = result.cpuTime / result.gpuTime;
        }
        
        // Check performance targets
        result.meetsPerformanceTarget = result.speedupRatio >= config_.targetSpeedupMinimum &&
                                       (result.memoryTransferTime / result.gpuTime * 100.0) <= config_.memoryOverheadMaximum;
        
        validateBenchmarkAccuracy(result);
        
    } catch (const std::exception& e) {
        result.errorMessage = e.what();
    }
    
    return result;
}

double GPUBenchmarkSuite::measureGPUPerformance(int width, int height, int samples) {
    if (!pathTracer_) return 0.0;
    
    if (performanceMonitor_) {
        performanceMonitor_->reset();
        performanceMonitor_->startGPUTiming();
    }
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
#ifdef USE_GPU
    if (pathTracer_->isGPUAvailable()) {
        pathTracer_->set_samples_per_pixel(samples);
        pathTracer_->trace_gpu(width, height);
    } else {
        return 0.0; // GPU not available
    }
#else
    return 0.0; // GPU not compiled in
#endif
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    
    if (performanceMonitor_) {
        performanceMonitor_->endGPUTiming();
    }
    
    return duration.count() / 1000.0; // Convert to milliseconds
}

double GPUBenchmarkSuite::measureCPUPerformance(int width, int height, int samples) {
    if (!pathTracer_) return 0.0;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    pathTracer_->set_samples_per_pixel(samples);
    pathTracer_->trace(width, height);
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    
    return duration.count() / 1000.0; // Convert to milliseconds
}

double GPUBenchmarkSuite::measureMemoryTransferOverhead(int width, int height) {
    if (!performanceMonitor_) return 0.0;
    
    auto metrics = performanceMonitor_->getMetrics();
    return metrics.gpuMemoryTransferTime;
}

std::vector<GPUBenchmarkSuite::BenchmarkResult> GPUBenchmarkSuite::benchmarkScalingValidation() {
    std::vector<BenchmarkResult> results;
    
    std::cout << "Running scaling validation benchmarks..." << std::endl;
    
    // Test different resolutions
    std::vector<std::pair<int, int>> resolutions = {
        {256, 256}, {512, 512}, {768, 768}, {1024, 1024}
    };
    
    for (const auto& res : resolutions) {
        auto result = runBenchmarkScenario("Scaling_" + std::to_string(res.first) + "x" + std::to_string(res.second),
                                          res.first, res.second, 10, 100);
        if (isResultValid(result)) {
            results.push_back(result);
        }
    }
    
    // Test different sample counts
    std::vector<int> sampleCounts = {1, 5, 10, 25, 50};
    
    for (int samples : sampleCounts) {
        auto result = runBenchmarkScenario("Samples_" + std::to_string(samples),
                                          512, 512, samples, 100);
        if (isResultValid(result)) {
            results.push_back(result);
        }
    }
    
    return results;
}

bool GPUBenchmarkSuite::validatePerformanceTargets(const std::vector<BenchmarkResult>& results) {
    if (results.empty()) return false;
    
    double averageSpeedup = calculateAverageSpeedup(results);
    bool allTargetsMet = std::all_of(results.begin(), results.end(),
                                    [](const BenchmarkResult& r) { return r.meetsPerformanceTarget; });
    
    std::cout << "Performance validation:" << std::endl;
    std::cout << "  Average speedup: " << std::fixed << std::setprecision(2) << averageSpeedup << "x" << std::endl;
    std::cout << "  Target speedup: " << config_.targetSpeedupMinimum << "x" << std::endl;
    std::cout << "  All targets met: " << (allTargetsMet ? "YES" : "NO") << std::endl;
    
    return averageSpeedup >= config_.targetSpeedupMinimum && allTargetsMet;
}

void GPUBenchmarkSuite::generateBenchmarkReport(const std::vector<BenchmarkResult>& results) const {
    std::cout << "\n=== GPU Benchmark Report ===" << std::endl;
    std::cout << std::left << std::setw(20) << "Scenario" 
              << std::setw(12) << "Resolution"
              << std::setw(10) << "Samples"
              << std::setw(12) << "GPU (ms)"
              << std::setw(12) << "CPU (ms)"
              << std::setw(10) << "Speedup"
              << std::setw(12) << "Target Met" << std::endl;
    std::cout << std::string(88, '-') << std::endl;
    
    for (const auto& result : results) {
        std::cout << std::left << std::setw(20) << result.scenarioName
                  << std::setw(12) << (std::to_string(result.imageWidth) + "x" + std::to_string(result.imageHeight))
                  << std::setw(10) << result.samplesPerPixel
                  << std::setw(12) << std::fixed << std::setprecision(2) << result.gpuTime
                  << std::setw(12) << std::fixed << std::setprecision(2) << result.cpuTime
                  << std::setw(10) << std::fixed << std::setprecision(1) << result.speedupRatio << "x"
                  << std::setw(12) << (result.meetsPerformanceTarget ? "YES" : "NO") << std::endl;
    }
    
    std::cout << std::string(88, '-') << std::endl;
    
    double avgSpeedup = calculateAverageSpeedup(results);
    int passCount = std::count_if(results.begin(), results.end(),
                                 [](const BenchmarkResult& r) { return r.meetsPerformanceTarget; });
    
    std::cout << "Summary: " << passCount << "/" << results.size() << " scenarios passed, "
              << "Average speedup: " << std::fixed << std::setprecision(2) << avgSpeedup << "x" << std::endl;
    std::cout << "=========================" << std::endl;
}

double GPUBenchmarkSuite::calculateAverageSpeedup(const std::vector<BenchmarkResult>& results) const {
    if (results.empty()) return 0.0;
    
    double totalSpeedup = 0.0;
    int validResults = 0;
    
    for (const auto& result : results) {
        if (result.speedupRatio > 0) {
            totalSpeedup += result.speedupRatio;
            validResults++;
        }
    }
    
    return validResults > 0 ? totalSpeedup / validResults : 0.0;
}

bool GPUBenchmarkSuite::isResultValid(const BenchmarkResult& result) const {
    return result.errorMessage.empty() && 
           result.gpuTime > 0 && 
           result.speedupRatio > 0;
}

void GPUBenchmarkSuite::setupBenchmarkScene(int primitiveCount) {
    // This would set up a scene with the specified number of primitives
    // For now, we'll just ensure the path tracer is ready
    if (pathTracer_) {
        pathTracer_->reset_stop_request();
    }
}

void GPUBenchmarkSuite::validateBenchmarkAccuracy(const BenchmarkResult& result) {
    // Validate that the benchmark result is reasonable
    if (result.speedupRatio > 100.0) {
        const_cast<BenchmarkResult&>(result).errorMessage = "Suspiciously high speedup - possible measurement error";
    }
    
    if (result.gpuTime < 0.1) {
        const_cast<BenchmarkResult&>(result).errorMessage = "GPU time too short - may be inaccurate";
    }
}

void GPUBenchmarkSuite::logBenchmarkResult(const BenchmarkResult& result) const {
    std::cout << "  " << result.scenarioName << ": " 
              << result.speedupRatio << "x speedup"
              << " (GPU: " << result.gpuTime << "ms, CPU: " << result.cpuTime << "ms)"
              << " - " << (result.meetsPerformanceTarget ? "PASS" : "FAIL") << std::endl;
}

void GPUBenchmarkSuite::calculateStatistics(std::vector<BenchmarkResult>& results) const {
    // Add any statistical analysis needed
    for (auto& result : results) {
        if (result.gpuTime > 0 && result.memoryTransferTime > 0) {
            double overhead = (result.memoryTransferTime / result.gpuTime) * 100.0;
            // Store overhead in a field if needed
        }
    }
}