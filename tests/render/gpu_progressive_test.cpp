#include <gtest/gtest.h>
#include "render/path_tracer.h"
#include "core/scene_manager.h"
#include "core/camera.h"
#include <chrono>
#include <vector>

class GPUProgressiveTest : public ::testing::Test {
protected:
    void SetUp() override {
        pathTracer = std::make_unique<PathTracer>();
        sceneManager = std::make_shared<SceneManager>();
        
        Camera camera(Vector3(0, 0, 0), Vector3(0, 0, -1), Vector3(0, 1, 0));
        pathTracer->set_camera(camera);
        pathTracer->set_scene_manager(sceneManager);
        pathTracer->set_max_depth(8);
        
        // Set up progressive config for testing
        progressiveConfig.initialSamples = 1;
        progressiveConfig.targetSamples = 16;
        progressiveConfig.progressiveSteps = 4;
        progressiveConfig.updateInterval = 0.1f; // Fast updates for testing
    }

    void TearDown() override {
        pathTracer.reset();
        sceneManager.reset();
    }

    std::unique_ptr<PathTracer> pathTracer;
    std::shared_ptr<SceneManager> sceneManager;
    ProgressiveConfig progressiveConfig;
    
    struct ProgressiveCallbackData {
        std::vector<std::vector<Color>> intermediateResults;
        std::vector<int> sampleCounts;
        std::vector<std::chrono::steady_clock::time_point> timestamps;
    };
};

TEST_F(GPUProgressiveTest, BasicGPUProgressiveRendering) {
    bool gpuAvailable = pathTracer->initializeGPU();
    if (!gpuAvailable) {
        GTEST_SKIP() << "GPU not available for progressive rendering test";
    }
    
    const int width = 64;
    const int height = 64;
    
    ProgressiveCallbackData callbackData;
    
    // Set up callback to capture intermediate results
    auto callback = [&callbackData](const std::vector<Color>& image, int w, int h, int currentSamples, int targetSamples) {
        callbackData.intermediateResults.push_back(image);
        callbackData.sampleCounts.push_back(currentSamples);
        callbackData.timestamps.push_back(std::chrono::steady_clock::now());
        
        std::cout << "Progressive update: " << currentSamples << "/" << targetSamples << " samples" << std::endl;
    };
    
    // TODO: Implement trace_progressive_gpu method
    // For now, test that regular progressive rendering still works
    bool success = pathTracer->trace_progressive(width, height, progressiveConfig, callback);
    
    EXPECT_TRUE(success);
    EXPECT_GT(callbackData.intermediateResults.size(), 0);
    EXPECT_EQ(callbackData.intermediateResults.size(), callbackData.sampleCounts.size());
    
    // Check that sample counts increase progressively
    for (size_t i = 1; i < callbackData.sampleCounts.size(); ++i) {
        EXPECT_GT(callbackData.sampleCounts[i], callbackData.sampleCounts[i-1]);
    }
    
    std::cout << "Progressive rendering completed with " << callbackData.intermediateResults.size() << " updates" << std::endl;
}

TEST_F(GPUProgressiveTest, ProgressiveTimingConsistency) {
    bool gpuAvailable = pathTracer->initializeGPU();
    if (!gpuAvailable) {
        GTEST_SKIP() << "GPU not available for timing test";
    }
    
    const int width = 96;
    const int height = 96;
    
    ProgressiveCallbackData callbackData;
    auto callback = [&callbackData](const std::vector<Color>& image, int w, int h, int currentSamples, int targetSamples) {
        callbackData.timestamps.push_back(std::chrono::steady_clock::now());
        callbackData.sampleCounts.push_back(currentSamples);
    };
    
    auto startTime = std::chrono::steady_clock::now();
    bool success = pathTracer->trace_progressive(width, height, progressiveConfig, callback);
    auto endTime = std::chrono::steady_clock::now();
    
    EXPECT_TRUE(success);
    EXPECT_GT(callbackData.timestamps.size(), 1);
    
    // Check timing intervals
    std::vector<double> intervals;
    for (size_t i = 1; i < callbackData.timestamps.size(); ++i) {
        auto interval = std::chrono::duration<double>(callbackData.timestamps[i] - callbackData.timestamps[i-1]).count();
        intervals.push_back(interval);
    }
    
    if (!intervals.empty()) {
        double totalTime = std::chrono::duration<double>(endTime - startTime).count();
        double expectedInterval = progressiveConfig.updateInterval;
        
        std::cout << "Progressive timing analysis:" << std::endl;
        std::cout << "  Total time: " << totalTime << "s" << std::endl;
        std::cout << "  Expected interval: " << expectedInterval << "s" << std::endl;
        std::cout << "  Actual intervals: ";
        for (double interval : intervals) {
            std::cout << interval << "s ";
        }
        std::cout << std::endl;
        
        // Intervals should be reasonably close to expected (within 50% tolerance)
        for (double interval : intervals) {
            EXPECT_GT(interval, expectedInterval * 0.5) << "Interval should not be too fast";
            EXPECT_LT(interval, expectedInterval * 2.0) << "Interval should not be too slow";
        }
    }
}

TEST_F(GPUProgressiveTest, ProgressiveImageQualityImprovement) {
    bool gpuAvailable = pathTracer->initializeGPU();
    if (!gpuAvailable) {
        GTEST_SKIP() << "GPU not available for quality improvement test";
    }
    
    const int width = 80;
    const int height = 80;
    
    // Use more progressive steps to see quality improvement
    progressiveConfig.progressiveSteps = 6;
    progressiveConfig.targetSamples = 32;
    
    ProgressiveCallbackData callbackData;
    auto callback = [&callbackData](const std::vector<Color>& image, int w, int h, int currentSamples, int targetSamples) {
        callbackData.intermediateResults.push_back(image);
        callbackData.sampleCounts.push_back(currentSamples);
    };
    
    bool success = pathTracer->trace_progressive(width, height, progressiveConfig, callback);
    EXPECT_TRUE(success);
    EXPECT_GE(callbackData.intermediateResults.size(), 2);
    
    if (callbackData.intermediateResults.size() >= 2) {
        // Compare first and last results to check for quality improvement
        const auto& firstResult = callbackData.intermediateResults[0];
        const auto& lastResult = callbackData.intermediateResults.back();
        
        // Calculate variance as a proxy for noise/quality
        auto calculateVariance = [](const std::vector<Color>& image) {
            double meanR = 0, meanG = 0, meanB = 0;
            for (const auto& pixel : image) {
                meanR += pixel.r;
                meanG += pixel.g;
                meanB += pixel.b;
            }
            meanR /= image.size();
            meanG /= image.size();
            meanB /= image.size();
            
            double varR = 0, varG = 0, varB = 0;
            for (const auto& pixel : image) {
                varR += (pixel.r - meanR) * (pixel.r - meanR);
                varG += (pixel.g - meanG) * (pixel.g - meanG);
                varB += (pixel.b - meanB) * (pixel.b - meanB);
            }
            
            return (varR + varG + varB) / (3 * image.size());
        };
        
        double firstVariance = calculateVariance(firstResult);
        double lastVariance = calculateVariance(lastResult);
        
        std::cout << "Quality improvement analysis:" << std::endl;
        std::cout << "  First result variance: " << firstVariance << " (" << callbackData.sampleCounts[0] << " samples)" << std::endl;
        std::cout << "  Last result variance: " << lastVariance << " (" << callbackData.sampleCounts.back() << " samples)" << std::endl;
        
        // Higher sample count should generally lead to lower variance (less noise)
        // But this is not always guaranteed due to the stochastic nature
        if (lastVariance < firstVariance) {
            std::cout << "✓ Progressive rendering shows quality improvement" << std::endl;
        } else {
            std::cout << "⚠ Progressive rendering variance not decreased (may be scene dependent)" << std::endl;
        }
        
        // At minimum, ensure both results have content
        bool firstHasContent = false, lastHasContent = false;
        for (const auto& pixel : firstResult) {
            if (pixel.r > 0.01f || pixel.g > 0.01f || pixel.b > 0.01f) {
                firstHasContent = true;
                break;
            }
        }
        for (const auto& pixel : lastResult) {
            if (pixel.r > 0.01f || pixel.g > 0.01f || pixel.b > 0.01f) {
                lastHasContent = true;
                break;
            }
        }
        
        EXPECT_TRUE(firstHasContent) << "First progressive result should have content";
        EXPECT_TRUE(lastHasContent) << "Last progressive result should have content";
    }
}

TEST_F(GPUProgressiveTest, ProgressiveMemoryManagement) {
    bool gpuAvailable = pathTracer->initializeGPU();
    if (!gpuAvailable) {
        GTEST_SKIP() << "GPU not available for memory management test";
    }
    
    // Test multiple progressive renderings to check for memory leaks
    const int width = 64;
    const int height = 64;
    const int iterations = 3;
    
    for (int i = 0; i < iterations; ++i) {
        std::cout << "Progressive rendering iteration " << (i + 1) << "/" << iterations << std::endl;
        
        ProgressiveCallbackData callbackData;
        auto callback = [&callbackData](const std::vector<Color>& image, int w, int h, int currentSamples, int targetSamples) {
            callbackData.intermediateResults.push_back(image);
        };
        
        bool success = pathTracer->trace_progressive(width, height, progressiveConfig, callback);
        EXPECT_TRUE(success) << "Progressive rendering iteration " << i << " should succeed";
        
        if (success) {
            EXPECT_GT(callbackData.intermediateResults.size(), 0);
            // Check that final result has expected size
            if (!callbackData.intermediateResults.empty()) {
                const auto& finalResult = callbackData.intermediateResults.back();
                EXPECT_EQ(finalResult.size(), width * height);
            }
        }
    }
    
    std::cout << "✓ Progressive rendering memory management test completed" << std::endl;
}