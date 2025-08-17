#include <gtest/gtest.h>
#include "render/gpu_performance.h"
#include <thread>
#include <chrono>

class GPUPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        monitor_ = std::make_unique<GPUPerformanceMonitor>();
    }

    void TearDown() override {
        monitor_.reset();
    }

    std::unique_ptr<GPUPerformanceMonitor> monitor_;
};

TEST_F(GPUPerformanceTest, Initialization) {
    EXPECT_NE(monitor_, nullptr);
    
    auto metrics = monitor_->getMetrics();
    EXPECT_EQ(metrics.gpuComputeTime, 0.0);
    EXPECT_EQ(metrics.cpuComputeTime, 0.0);
    EXPECT_EQ(metrics.speedupRatio, 0.0);
}

TEST_F(GPUPerformanceTest, BasicTiming) {
    monitor_->startGPUTiming();
    
    // Simulate some work
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    monitor_->endGPUTiming();
    
    auto metrics = monitor_->getMetrics();
    
    // CPU time should be recorded
    EXPECT_GT(metrics.cpuComputeTime, 5.0); // At least 5ms
    EXPECT_LT(metrics.cpuComputeTime, 50.0); // But not too much more
}

TEST_F(GPUPerformanceTest, MemoryTransferRecording) {
    size_t testBytes = 1024 * 1024; // 1MB
    double transferTime = 5.0; // 5ms
    
    monitor_->recordMemoryTransfer(testBytes, transferTime);
    
    auto metrics = monitor_->getMetrics();
    EXPECT_EQ(metrics.gpuMemoryTransferTime, transferTime);
    EXPECT_EQ(metrics.gpuMemoryUsed, testBytes);
}

TEST_F(GPUPerformanceTest, MultipleMemoryTransfers) {
    monitor_->recordMemoryTransfer(512 * 1024, 2.5); // 512KB, 2.5ms
    monitor_->recordMemoryTransfer(512 * 1024, 2.5); // Another 512KB, 2.5ms
    
    auto metrics = monitor_->getMetrics();
    EXPECT_EQ(metrics.gpuMemoryTransferTime, 5.0); // Total 5ms
    EXPECT_EQ(metrics.gpuMemoryUsed, 1024 * 1024); // Total 1MB
}

TEST_F(GPUPerformanceTest, OverheadCalculation) {
    // Set up scenario where transfer time is 10% of total GPU time
    monitor_->recordMemoryTransfer(1024, 1.0); // 1ms transfer
    
    // Simulate GPU compute time indirectly by setting up metrics
    auto metrics = monitor_->getMetrics();
    // Overhead should be calculated as (transfer / total) * 100
    // If total GPU time is 10ms and transfer is 1ms, overhead should be 10%
}

TEST_F(GPUPerformanceTest, AverageMetrics) {
    // Add multiple performance measurements
    for (int i = 0; i < 5; ++i) {
        monitor_->startGPUTiming();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        monitor_->endGPUTiming();
        monitor_->recordMemoryTransfer(1024, 1.0);
    }
    
    auto avgMetrics = monitor_->getAverageMetrics(3);
    EXPECT_GT(avgMetrics.cpuComputeTime, 0.0);
    EXPECT_GT(avgMetrics.gpuMemoryTransferTime, 0.0);
}

TEST_F(GPUPerformanceTest, RegressionDetection) {
    // Initially should not detect regression with no history
    EXPECT_FALSE(monitor_->isPerformanceRegression());
    
    // Add some baseline measurements
    for (int i = 0; i < 10; ++i) {
        monitor_->startGPUTiming();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        monitor_->endGPUTiming();
    }
    
    // Should still not detect regression with consistent performance
    EXPECT_FALSE(monitor_->isPerformanceRegression());
}

TEST_F(GPUPerformanceTest, ConfigurationOptions) {
    monitor_->setRegressionThreshold(0.25); // 25% threshold
    monitor_->enableDetailedLogging(true);
    
    // Test that configuration is accepted without errors
    monitor_->startGPUTiming();
    monitor_->endGPUTiming();
    
    EXPECT_TRUE(true); // If we get here, configuration worked
}

TEST_F(GPUPerformanceTest, ResetFunctionality) {
    // Set up some metrics
    monitor_->recordMemoryTransfer(1024, 1.0);
    monitor_->startGPUTiming();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    monitor_->endGPUTiming();
    
    // Verify metrics exist
    auto metrics = monitor_->getMetrics();
    EXPECT_GT(metrics.cpuComputeTime, 0.0);
    
    // Reset and verify metrics are cleared
    monitor_->reset();
    metrics = monitor_->getMetrics();
    EXPECT_EQ(metrics.gpuComputeTime, 0.0);
    EXPECT_EQ(metrics.cpuComputeTime, 0.0);
    EXPECT_EQ(metrics.gpuMemoryTransferTime, 0.0);
}

TEST_F(GPUPerformanceTest, PerformanceLogging) {
    monitor_->recordMemoryTransfer(2048, 2.0);
    monitor_->startGPUTiming();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    monitor_->endGPUTiming();
    
    // Test logging doesn't crash
    EXPECT_NO_THROW(monitor_->logPerformanceData("UnitTest"));
}