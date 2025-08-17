#pragma once

#include <vector>
#include <string>
#include <memory>
#include <chrono>

// Forward declarations
class PathTracer;
class GPUPerformanceMonitor;
class GPUHardwareOptimizer;
class HybridModeSelector;

class GPUBenchmarkSuite {
public:
    struct BenchmarkResult {
        std::string scenarioName;
        int imageWidth = 0;
        int imageHeight = 0;
        int samplesPerPixel = 0;
        int primitiveCount = 0;
        double cpuTime = 0.0;           // CPU execution time (ms)
        double gpuTime = 0.0;           // GPU execution time (ms)
        double speedupRatio = 0.0;      // GPU vs CPU speedup
        double memoryTransferTime = 0.0; // Memory transfer overhead (ms)
        double memoryUsage = 0.0;       // GPU memory usage (MB)
        bool meetsPerformanceTarget = false;
        std::string errorMessage;
        std::chrono::system_clock::time_point timestamp;
    };

    struct BenchmarkConfiguration {
        bool enableCPUComparison = true;
        bool enableMemoryProfiling = true;
        bool enableRegressionDetection = true;
        int warmupIterations = 2;
        int benchmarkIterations = 5;
        double targetSpeedupMinimum = 5.0;
        double memoryOverheadMaximum = 5.0; // Max 5% memory transfer overhead
    };

    GPUBenchmarkSuite();
    ~GPUBenchmarkSuite() = default;

    // Main benchmark execution
    std::vector<BenchmarkResult> runFullBenchmarkSuite();
    BenchmarkResult runSingleBenchmark(const std::string& scenario);
    bool validatePerformanceTargets(const std::vector<BenchmarkResult>& results);

    // Standardized test scenarios
    BenchmarkResult benchmarkSimpleScene();
    BenchmarkResult benchmarkComplexScene();
    BenchmarkResult benchmarkHighSampleCount();
    BenchmarkResult benchmarkLargePrimitiveCount();
    BenchmarkResult benchmarkLargeResolution();
    BenchmarkResult benchmarkProgressiveRendering();
    
    // Scaling validation
    std::vector<BenchmarkResult> benchmarkScalingValidation();
    BenchmarkResult benchmarkMemoryScaling();
    BenchmarkResult benchmarkComputeScaling();
    
    // Regression testing
    bool runRegressionTests();
    std::vector<BenchmarkResult> getBaselineResults() const { return baselineResults_; }
    void setBaselineResults(const std::vector<BenchmarkResult>& baseline);
    bool detectPerformanceRegression(const BenchmarkResult& current, const BenchmarkResult& baseline) const;
    
    // Configuration
    void setConfiguration(const BenchmarkConfiguration& config) { config_ = config; }
    BenchmarkConfiguration getConfiguration() const { return config_; }
    void setPathTracer(std::shared_ptr<PathTracer> pathTracer) { pathTracer_ = pathTracer; }
    void setPerformanceMonitor(std::shared_ptr<GPUPerformanceMonitor> monitor) { performanceMonitor_ = monitor; }
    void setHardwareOptimizer(std::shared_ptr<GPUHardwareOptimizer> optimizer) { hardwareOptimizer_ = optimizer; }
    void setHybridModeSelector(std::shared_ptr<HybridModeSelector> selector) { hybridModeSelector_ = selector; }
    
    // Reporting
    void generateBenchmarkReport(const std::vector<BenchmarkResult>& results) const;
    void exportResultsToCSV(const std::vector<BenchmarkResult>& results, const std::string& filename) const;
    void logBenchmarkSummary(const std::vector<BenchmarkResult>& results) const;
    
    // Custom benchmark execution
    BenchmarkResult runBenchmarkScenario(const std::string& name, int width, int height, 
                                         int samples, int primitives = 100);

private:
    BenchmarkConfiguration config_;
    std::shared_ptr<PathTracer> pathTracer_;
    std::shared_ptr<GPUPerformanceMonitor> performanceMonitor_;
    std::shared_ptr<GPUHardwareOptimizer> hardwareOptimizer_;
    std::shared_ptr<HybridModeSelector> hybridModeSelector_;
    
    std::vector<BenchmarkResult> baselineResults_;
    std::vector<BenchmarkResult> benchmarkHistory_;
    
    // Internal benchmark execution (now moved to public)
    void setupBenchmarkScene(int primitiveCount);
    void validateBenchmarkAccuracy(const BenchmarkResult& result);
    
    // Performance measurement
    double measureCPUPerformance(int width, int height, int samples);
    double measureGPUPerformance(int width, int height, int samples);
    double measureMemoryTransferOverhead(int width, int height);
    
    // Analysis and validation
    bool isResultValid(const BenchmarkResult& result) const;
    void calculateStatistics(std::vector<BenchmarkResult>& results) const;
    double calculateAverageSpeedup(const std::vector<BenchmarkResult>& results) const;
    bool allTargetsMet(const std::vector<BenchmarkResult>& results) const;
    
    // Utility methods
    std::string formatDuration(double milliseconds) const;
    std::string formatMemory(double megabytes) const;
    void logBenchmarkResult(const BenchmarkResult& result) const;
};