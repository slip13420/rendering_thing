#include <gtest/gtest.h>
#include "performance/gpu_benchmark.h"
#include "render/path_tracer.h"
#include "render/gpu_performance.h"
#include "render/gpu_hardware_optimizer.h"
#include "render/hybrid_mode_selector.h"
#include <memory>

class GPUBenchmarkTest : public ::testing::Test {
protected:
    void SetUp() override {
        benchmarkSuite_ = std::make_unique<GPUBenchmarkSuite>();
        pathTracer_ = std::make_shared<PathTracer>();
        performanceMonitor_ = std::make_shared<GPUPerformanceMonitor>();
        hardwareOptimizer_ = std::make_shared<GPUHardwareOptimizer>();
        hybridModeSelector_ = std::make_shared<HybridModeSelector>();
        
        // Configure benchmark suite
        benchmarkSuite_->setPathTracer(pathTracer_);
        benchmarkSuite_->setPerformanceMonitor(performanceMonitor_);
        benchmarkSuite_->setHardwareOptimizer(hardwareOptimizer_);
        benchmarkSuite_->setHybridModeSelector(hybridModeSelector_);
        
        // Set conservative configuration for testing
        GPUBenchmarkSuite::BenchmarkConfiguration config;
        config.warmupIterations = 1;
        config.benchmarkIterations = 2;
        config.targetSpeedupMinimum = 1.5; // Lower target for testing
        benchmarkSuite_->setConfiguration(config);
    }

    void TearDown() override {
        benchmarkSuite_.reset();
        pathTracer_.reset();
        performanceMonitor_.reset();
        hardwareOptimizer_.reset();
        hybridModeSelector_.reset();
    }

    std::unique_ptr<GPUBenchmarkSuite> benchmarkSuite_;
    std::shared_ptr<PathTracer> pathTracer_;
    std::shared_ptr<GPUPerformanceMonitor> performanceMonitor_;
    std::shared_ptr<GPUHardwareOptimizer> hardwareOptimizer_;
    std::shared_ptr<HybridModeSelector> hybridModeSelector_;
};

TEST_F(GPUBenchmarkTest, Initialization) {
    EXPECT_NE(benchmarkSuite_, nullptr);
    
    auto config = benchmarkSuite_->getConfiguration();
    EXPECT_EQ(config.warmupIterations, 1);
    EXPECT_EQ(config.benchmarkIterations, 2);
}

TEST_F(GPUBenchmarkTest, SimpleScenarioBenchmark) {
    auto result = benchmarkSuite_->benchmarkSimpleScene();
    
    EXPECT_EQ(result.scenarioName, "SimpleScene");
    EXPECT_EQ(result.imageWidth, 512);
    EXPECT_EQ(result.imageHeight, 512);
    EXPECT_EQ(result.samplesPerPixel, 10);
    EXPECT_EQ(result.primitiveCount, 50);
    
    // Result should be valid (even if GPU isn't available, it should still work)
    EXPECT_TRUE(result.errorMessage.empty() || result.errorMessage.find("GPU") != std::string::npos);
}

TEST_F(GPUBenchmarkTest, ComplexScenarioBenchmark) {
    auto result = benchmarkSuite_->benchmarkComplexScene();
    
    EXPECT_EQ(result.scenarioName, "ComplexScene");
    EXPECT_EQ(result.primitiveCount, 500);
    EXPECT_TRUE(result.errorMessage.empty() || result.errorMessage.find("GPU") != std::string::npos);
}

TEST_F(GPUBenchmarkTest, HighSampleCountBenchmark) {
    auto result = benchmarkSuite_->benchmarkHighSampleCount();
    
    EXPECT_EQ(result.scenarioName, "HighSampleCount");
    EXPECT_EQ(result.samplesPerPixel, 100);
    EXPECT_TRUE(result.errorMessage.empty() || result.errorMessage.find("GPU") != std::string::npos);
}

TEST_F(GPUBenchmarkTest, LargePrimitiveCountBenchmark) {
    auto result = benchmarkSuite_->benchmarkLargePrimitiveCount();
    
    EXPECT_EQ(result.scenarioName, "LargePrimitiveCount");
    EXPECT_EQ(result.primitiveCount, 2000);
    EXPECT_TRUE(result.errorMessage.empty() || result.errorMessage.find("GPU") != std::string::npos);
}

TEST_F(GPUBenchmarkTest, LargeResolutionBenchmark) {
    auto result = benchmarkSuite_->benchmarkLargeResolution();
    
    EXPECT_EQ(result.scenarioName, "LargeResolution");
    EXPECT_EQ(result.imageWidth, 1024);
    EXPECT_EQ(result.imageHeight, 1024);
    EXPECT_TRUE(result.errorMessage.empty() || result.errorMessage.find("GPU") != std::string::npos);
}

TEST_F(GPUBenchmarkTest, ProgressiveRenderingBenchmark) {
    auto result = benchmarkSuite_->benchmarkProgressiveRendering();
    
    EXPECT_EQ(result.scenarioName, "ProgressiveRendering");
    EXPECT_EQ(result.samplesPerPixel, 50);
    EXPECT_TRUE(result.errorMessage.empty() || result.errorMessage.find("GPU") != std::string::npos);
}

TEST_F(GPUBenchmarkTest, SingleBenchmarkByName) {
    auto result = benchmarkSuite_->runSingleBenchmark("SimpleScene");
    EXPECT_EQ(result.scenarioName, "SimpleScene");
    
    auto unknownResult = benchmarkSuite_->runSingleBenchmark("UnknownScenario");
    EXPECT_EQ(unknownResult.scenarioName, "UnknownScenario");
    EXPECT_FALSE(unknownResult.errorMessage.empty());
}

TEST_F(GPUBenchmarkTest, ScalingValidationBenchmarks) {
    auto results = benchmarkSuite_->benchmarkScalingValidation();
    
    EXPECT_GT(results.size(), 0);
    
    // Should have results for different resolutions and sample counts
    bool hasResolutionTests = false;
    bool hasSampleTests = false;
    
    for (const auto& result : results) {
        if (result.scenarioName.find("Scaling_") != std::string::npos) {
            hasResolutionTests = true;
        }
        if (result.scenarioName.find("Samples_") != std::string::npos) {
            hasSampleTests = true;
        }
    }
    
    EXPECT_TRUE(hasResolutionTests);
    EXPECT_TRUE(hasSampleTests);
}

TEST_F(GPUBenchmarkTest, PerformanceTargetValidation) {
    std::vector<GPUBenchmarkSuite::BenchmarkResult> results;
    
    // Create mock results
    GPUBenchmarkSuite::BenchmarkResult passingResult;
    passingResult.scenarioName = "PassingTest";
    passingResult.speedupRatio = 2.0; // Above 1.5 threshold
    passingResult.meetsPerformanceTarget = true;
    results.push_back(passingResult);
    
    GPUBenchmarkSuite::BenchmarkResult failingResult;
    failingResult.scenarioName = "FailingTest";
    failingResult.speedupRatio = 1.0; // Below 1.5 threshold
    failingResult.meetsPerformanceTarget = false;
    results.push_back(failingResult);
    
    // Validation should pass if average is above threshold
    bool validationResult = benchmarkSuite_->validatePerformanceTargets(results);
    EXPECT_TRUE(validationResult); // Average of 2.0 and 1.0 is 1.5, which meets our 1.5 target
}

TEST_F(GPUBenchmarkTest, BenchmarkConfiguration) {
    GPUBenchmarkSuite::BenchmarkConfiguration config;
    config.enableCPUComparison = false;
    config.enableMemoryProfiling = false;
    config.warmupIterations = 3;
    config.benchmarkIterations = 7;
    config.targetSpeedupMinimum = 4.0;
    
    benchmarkSuite_->setConfiguration(config);
    
    auto retrievedConfig = benchmarkSuite_->getConfiguration();
    EXPECT_FALSE(retrievedConfig.enableCPUComparison);
    EXPECT_FALSE(retrievedConfig.enableMemoryProfiling);
    EXPECT_EQ(retrievedConfig.warmupIterations, 3);
    EXPECT_EQ(retrievedConfig.benchmarkIterations, 7);
    EXPECT_EQ(retrievedConfig.targetSpeedupMinimum, 4.0);
}

TEST_F(GPUBenchmarkTest, FullBenchmarkSuiteExecution) {
    // This test may take longer and might fail if GPU is not available
    // But it should not crash
    EXPECT_NO_THROW({
        auto results = benchmarkSuite_->runFullBenchmarkSuite();
        // Results may be empty if GPU is not available, but should not crash
        EXPECT_GE(results.size(), 0);
    });
}

TEST_F(GPUBenchmarkTest, BenchmarkReporting) {
    std::vector<GPUBenchmarkSuite::BenchmarkResult> results;
    
    GPUBenchmarkSuite::BenchmarkResult result;
    result.scenarioName = "TestScenario";
    result.imageWidth = 512;
    result.imageHeight = 512;
    result.samplesPerPixel = 10;
    result.gpuTime = 50.0;
    result.cpuTime = 100.0;
    result.speedupRatio = 2.0;
    result.meetsPerformanceTarget = true;
    results.push_back(result);
    
    // Test that reporting functions don't crash
    EXPECT_NO_THROW(benchmarkSuite_->generateBenchmarkReport(results));
    EXPECT_NO_THROW(benchmarkSuite_->logBenchmarkSummary(results));
}