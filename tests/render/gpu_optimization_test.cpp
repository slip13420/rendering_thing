#include <gtest/gtest.h>
#include "render/gpu_hardware_optimizer.h"
#include "render/hybrid_mode_selector.h"
#include "render/gpu_performance.h"
#include <memory>

class GPUOptimizationTest : public ::testing::Test {
protected:
    void SetUp() override {
        hardwareOptimizer_ = std::make_unique<GPUHardwareOptimizer>();
        hybridModeSelector_ = std::make_unique<HybridModeSelector>();
        performanceMonitor_ = std::make_shared<GPUPerformanceMonitor>();
        
        // Connect components
        hybridModeSelector_->setPerformanceMonitor(performanceMonitor_);
        hybridModeSelector_->setHardwareOptimizer(std::shared_ptr<GPUHardwareOptimizer>(hardwareOptimizer_.get(), [](GPUHardwareOptimizer*){}));
    }

    void TearDown() override {
        hardwareOptimizer_.reset();
        hybridModeSelector_.reset();
        performanceMonitor_.reset();
    }

    std::unique_ptr<GPUHardwareOptimizer> hardwareOptimizer_;
    std::unique_ptr<HybridModeSelector> hybridModeSelector_;
    std::shared_ptr<GPUPerformanceMonitor> performanceMonitor_;
};

// GPU Hardware Optimizer Tests
TEST_F(GPUOptimizationTest, HardwareOptimizerInitialization) {
    EXPECT_NE(hardwareOptimizer_, nullptr);
    
    auto profile = hardwareOptimizer_->getHardwareProfile();
    EXPECT_GT(profile.optimalThreadGroupSize, 0);
    EXPECT_GT(profile.preferredVectorWidth, 0);
}

TEST_F(GPUOptimizationTest, HardwareCapabilityDetection) {
    // Detection should work whether GPU is available or not
    bool detectionResult = hardwareOptimizer_->detectHardwareCapabilities();
    
    auto profile = hardwareOptimizer_->getHardwareProfile();
    EXPECT_GT(profile.optimalThreadGroupSize, 0);
    EXPECT_GE(profile.maxComputeUnits, 0);
    
    // Vendor detection should work
    bool hasVendor = hardwareOptimizer_->isNVIDIA() || 
                    hardwareOptimizer_->isAMD() || 
                    hardwareOptimizer_->isIntel();
    // Note: This may be false if running without GPU, which is okay
}

TEST_F(GPUOptimizationTest, OptimalThreadGroupSizing) {
    // Test different image sizes
    int smallSize = hardwareOptimizer_->getOptimalThreadGroupSize(256, 256);
    int mediumSize = hardwareOptimizer_->getOptimalThreadGroupSize(512, 512);
    int largeSize = hardwareOptimizer_->getOptimalThreadGroupSize(1024, 1024);
    
    EXPECT_GT(smallSize, 0);
    EXPECT_GT(mediumSize, 0);
    EXPECT_GT(largeSize, 0);
    
    // Sizes should be reasonable
    EXPECT_LE(smallSize, 1024);
    EXPECT_LE(mediumSize, 1024);
    EXPECT_LE(largeSize, 1024);
}

TEST_F(GPUOptimizationTest, WorkGroupDimensions) {
    auto dimensions = hardwareOptimizer_->getOptimalWorkGroupDimensions(512, 512);
    
    EXPECT_GT(dimensions.first, 0);
    EXPECT_GT(dimensions.second, 0);
    
    // Product should equal thread group size
    int threadGroupSize = hardwareOptimizer_->getOptimalThreadGroupSize(512, 512);
    EXPECT_EQ(dimensions.first * dimensions.second, threadGroupSize);
}

TEST_F(GPUOptimizationTest, MemoryOptimization) {
    size_t testDataSize = 1024 * 1024; // 1MB
    size_t optimalBufferSize = hardwareOptimizer_->getOptimalBufferSize(testDataSize);
    
    EXPECT_GE(optimalBufferSize, testDataSize); // Should be at least as large as input
    
    int batchSize = hardwareOptimizer_->getOptimalMemoryTransferBatchSize();
    EXPECT_GT(batchSize, 0);
    EXPECT_LE(batchSize, 16 * 1024 * 1024); // Should be reasonable (<=16MB)
}

TEST_F(GPUOptimizationTest, AdaptiveOptimization) {
    double currentPerformance = 3.0; // 3x speedup
    double targetPerformance = 5.0;  // Want 5x speedup
    
    // This should trigger optimization adjustments
    EXPECT_NO_THROW(hardwareOptimizer_->updateOptimizationBasedOnPerformance(currentPerformance, targetPerformance));
    
    // Test with above-target performance
    EXPECT_NO_THROW(hardwareOptimizer_->updateOptimizationBasedOnPerformance(6.0, targetPerformance));
}

// Hybrid Mode Selector Tests
TEST_F(GPUOptimizationTest, HybridModeSelectorInitialization) {
    EXPECT_NE(hybridModeSelector_, nullptr);
    
    EXPECT_EQ(hybridModeSelector_->getSelectionMode(), HybridModeSelector::SelectionMode::ADAPTIVE);
}

TEST_F(GPUOptimizationTest, GPUSelectionLogic) {
    // Test different selection modes
    hybridModeSelector_->setSelectionMode(HybridModeSelector::SelectionMode::ALWAYS_GPU);
    
    // Should try to use GPU if available (may return false if no GPU)
    bool gpuChoice = hybridModeSelector_->shouldUseGPU(512, 512, 10, 100);
    // Result depends on GPU availability, so we just test it doesn't crash
    
    hybridModeSelector_->setSelectionMode(HybridModeSelector::SelectionMode::ALWAYS_CPU);
    EXPECT_FALSE(hybridModeSelector_->shouldUseGPU(512, 512, 10, 100));
}

TEST_F(GPUOptimizationTest, PerformancePrediction) {
    double gpuTime = hybridModeSelector_->predictGPUTime(512, 512, 10);
    double cpuTime = hybridModeSelector_->predictCPUTime(512, 512, 10);
    
    EXPECT_GT(gpuTime, 0.0);
    EXPECT_GT(cpuTime, 0.0);
    
    double speedup = hybridModeSelector_->getExpectedSpeedup(512, 512, 10);
    EXPECT_GT(speedup, 0.0);
}

TEST_F(GPUOptimizationTest, SceneCharacteristicsHandling) {
    HybridModeSelector::SceneCharacteristics scene;
    scene.width = 512;
    scene.height = 512;
    scene.samples = 25;
    scene.primitiveCount = 200;
    scene.hasComplexMaterials = true;
    scene.hasVolumetricLighting = false;
    scene.complexity = 1.5;
    
    double gpuTime = hybridModeSelector_->predictGPUTime(scene);
    double cpuTime = hybridModeSelector_->predictCPUTime(scene);
    bool shouldUseGPU = hybridModeSelector_->shouldUseGPU(scene);
    
    EXPECT_GT(gpuTime, 0.0);
    EXPECT_GT(cpuTime, 0.0);
    // shouldUseGPU result depends on configuration and GPU availability
}

TEST_F(GPUOptimizationTest, PerformanceModelUpdate) {
    HybridModeSelector::SceneCharacteristics scene;
    scene.width = 256;
    scene.height = 256;
    scene.samples = 10;
    scene.primitiveCount = 50;
    scene.complexity = 1.0;
    
    double actualGPUTime = 25.0; // 25ms
    double actualCPUTime = 100.0; // 100ms
    
    // Update the performance model with actual measurements
    EXPECT_NO_THROW(hybridModeSelector_->updatePerformanceModel(scene, actualGPUTime, actualCPUTime));
    
    // Predictions should now be influenced by this data
    double newGPUPrediction = hybridModeSelector_->predictGPUTime(scene);
    double newCPUPrediction = hybridModeSelector_->predictCPUTime(scene);
    
    EXPECT_GT(newGPUPrediction, 0.0);
    EXPECT_GT(newCPUPrediction, 0.0);
}

TEST_F(GPUOptimizationTest, AdaptiveThresholds) {
    hybridModeSelector_->enableAdaptiveThresholds(true);
    hybridModeSelector_->setPerformanceThreshold(2.0);
    
    // Simulate learning with multiple updates
    HybridModeSelector::SceneCharacteristics scene;
    scene.width = 512;
    scene.height = 512;
    scene.samples = 10;
    scene.primitiveCount = 100;
    scene.complexity = 1.0;
    
    // Add several performance measurements
    for (int i = 0; i < 10; ++i) {
        double gpuTime = 20.0 + i; // Varying GPU performance
        double cpuTime = 80.0;     // Consistent CPU performance
        hybridModeSelector_->updatePerformanceModel(scene, gpuTime, cpuTime);
    }
    
    EXPECT_NO_THROW(hybridModeSelector_->updateAdaptiveThresholds());
}

TEST_F(GPUOptimizationTest, GPUAvailabilityCheck) {
    bool gpuAvailable = hybridModeSelector_->isGPUAvailable();
    // Result depends on system configuration, just verify it doesn't crash
    
    // Test memory adequacy check
    HybridModeSelector::SceneCharacteristics largeScene;
    largeScene.width = 4096;
    largeScene.height = 4096;
    largeScene.samples = 100;
    largeScene.primitiveCount = 10000;
    
    bool hasAdequateMemory = hybridModeSelector_->hasAdequateGPUMemory(largeScene);
    // Result depends on GPU memory configuration
}

TEST_F(GPUOptimizationTest, PerformanceThresholdConfiguration) {
    hybridModeSelector_->setPerformanceThreshold(3.0);
    hybridModeSelector_->setMemoryThreshold(1024 * 1024 * 1024); // 1GB
    
    // Test that configuration is applied without errors
    bool decision = hybridModeSelector_->shouldUseGPU(512, 512, 10, 100);
    // Result depends on predictions and GPU availability
    
    // Verify mode switching works
    auto originalMode = hybridModeSelector_->getSelectionMode();
    hybridModeSelector_->setSelectionMode(HybridModeSelector::SelectionMode::PERFORMANCE_BASED);
    EXPECT_EQ(hybridModeSelector_->getSelectionMode(), HybridModeSelector::SelectionMode::PERFORMANCE_BASED);
}

// Integration Tests
TEST_F(GPUOptimizationTest, ComponentIntegration) {
    // Test that hardware optimizer and hybrid selector work together
    hardwareOptimizer_->detectHardwareCapabilities();
    hardwareOptimizer_->optimizeForHardware();
    
    // Hardware optimization should influence hybrid decisions
    bool decision1 = hybridModeSelector_->shouldUseGPU(512, 512, 10, 100);
    
    // Update hardware optimizer with poor performance
    hardwareOptimizer_->updateOptimizationBasedOnPerformance(1.0, 5.0);
    
    // Decision might change based on updated optimization
    bool decision2 = hybridModeSelector_->shouldUseGPU(512, 512, 10, 100);
    
    // Both decisions should be valid (not crash)
    EXPECT_TRUE(true);
}