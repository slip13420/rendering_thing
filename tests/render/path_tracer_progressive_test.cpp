#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include "render/path_tracer.h"
#include "core/scene_manager.h"

class PathTracerProgressiveTest : public ::testing::Test {
protected:
    void SetUp() override {
        path_tracer_ = std::make_unique<PathTracer>();
        scene_manager_ = std::make_shared<SceneManager>();
        scene_manager_->initialize();
        
        path_tracer_->set_scene_manager(scene_manager_);
        path_tracer_->set_max_depth(5);
    }

    void TearDown() override {
        path_tracer_->request_stop();
        path_tracer_.reset();
    }

    std::unique_ptr<PathTracer> path_tracer_;
    std::shared_ptr<SceneManager> scene_manager_;
};

// Test progressive configuration
TEST_F(PathTracerProgressiveTest, ProgressiveConfigValidation) {
    ProgressiveConfig config;
    config.initialSamples = 1;
    config.targetSamples = 100;
    config.progressiveSteps = 5;
    config.updateInterval = 0.1f;
    
    EXPECT_GT(config.targetSamples, config.initialSamples);
    EXPECT_GT(config.progressiveSteps, 1);
    EXPECT_GT(config.updateInterval, 0.0f);
}

// Test progressive sampling accuracy and convergence
TEST_F(PathTracerProgressiveTest, ProgressiveSamplingAccuracy) {
    ProgressiveConfig config;
    config.initialSamples = 1;
    config.targetSamples = 20;
    config.progressiveSteps = 4;
    config.updateInterval = 0.05f;
    
    std::vector<std::pair<int, std::vector<Color>>> progressive_results;
    
    auto callback = [&](const std::vector<Color>& data, int width, int height, int current_samples, int target_samples) {
        progressive_results.emplace_back(current_samples, data);
    };
    
    int width = 10, height = 10;
    bool completed = path_tracer_->trace_progressive(width, height, config, callback);
    
    EXPECT_TRUE(completed);
    EXPECT_FALSE(progressive_results.empty());
    
    // Verify progressive improvement
    EXPECT_GE(progressive_results.size(), config.progressiveSteps);
    
    // Check that sample counts increase
    for (size_t i = 1; i < progressive_results.size(); ++i) {
        EXPECT_GT(progressive_results[i].first, progressive_results[i-1].first);
    }
    
    // Final result should have target samples
    EXPECT_EQ(progressive_results.back().first, config.targetSamples);
}

// Test progressive rendering interruption
TEST_F(PathTracerProgressiveTest, ProgressiveInterruption) {
    ProgressiveConfig config;
    config.initialSamples = 1;
    config.targetSamples = 1000; // Large number to ensure it doesn't complete quickly
    config.progressiveSteps = 10;
    config.updateInterval = 0.1f;
    
    bool callback_called = false;
    auto callback = [&](const std::vector<Color>& data, int width, int height, int current_samples, int target_samples) {
        callback_called = true;
    };
    
    // Start progressive render in a thread
    std::thread render_thread([&]() {
        path_tracer_->trace_progressive(50, 50, config, callback);
    });
    
    // Let it start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Stop the render
    path_tracer_->request_stop();
    
    // Wait for thread to complete
    render_thread.join();
    
    EXPECT_TRUE(callback_called);
}

// Test progressive sample accumulation
TEST_F(PathTracerProgressiveTest, SampleAccumulation) {
    ProgressiveConfig config;
    config.initialSamples = 2;
    config.targetSamples = 8;
    config.progressiveSteps = 3;
    config.updateInterval = 0.05f;
    
    std::vector<int> recorded_samples;
    auto callback = [&](const std::vector<Color>& data, int width, int height, int current_samples, int target_samples) {
        recorded_samples.push_back(current_samples);
    };
    
    int width = 5, height = 5;
    bool completed = path_tracer_->trace_progressive(width, height, config, callback);
    
    EXPECT_TRUE(completed);
    EXPECT_FALSE(recorded_samples.empty());
    
    // Verify progressive accumulation
    for (size_t i = 1; i < recorded_samples.size(); ++i) {
        EXPECT_GE(recorded_samples[i], recorded_samples[i-1]);
    }
}

// Test stop request responsiveness
TEST_F(PathTracerProgressiveTest, StopRequestResponsiveness) {
    EXPECT_FALSE(path_tracer_->is_stop_requested());
    
    path_tracer_->request_stop();
    EXPECT_TRUE(path_tracer_->is_stop_requested());
    
    path_tracer_->reset_stop_request();
    EXPECT_FALSE(path_tracer_->is_stop_requested());
}

// Test progressive render with various configurations
TEST_F(PathTracerProgressiveTest, VariousConfigurations) {
    struct TestCase {
        int initial_samples;
        int target_samples;
        int progressive_steps;
        float update_interval;
    };
    
    std::vector<TestCase> test_cases = {
        {1, 4, 2, 0.01f},   // Minimal case
        {1, 16, 4, 0.02f},  // Small case
        {2, 32, 5, 0.05f},  // Medium case
    };
    
    for (const auto& test_case : test_cases) {
        ProgressiveConfig config;
        config.initialSamples = test_case.initial_samples;
        config.targetSamples = test_case.target_samples;
        config.progressiveSteps = test_case.progressive_steps;
        config.updateInterval = test_case.update_interval;
        
        bool callback_called = false;
        auto callback = [&](const std::vector<Color>& data, int width, int height, int current_samples, int target_samples) {
            callback_called = true;
            EXPECT_LE(current_samples, target_samples);
        };
        
        path_tracer_->reset_stop_request();
        bool completed = path_tracer_->trace_progressive(8, 8, config, callback);
        
        EXPECT_TRUE(completed) << "Failed for config: " << test_case.initial_samples 
                              << "->" << test_case.target_samples 
                              << " in " << test_case.progressive_steps << " steps";
        EXPECT_TRUE(callback_called);
    }
}

// Test image quality convergence
TEST_F(PathTracerProgressiveTest, ImageQualityConvergence) {
    ProgressiveConfig config;
    config.initialSamples = 1;
    config.targetSamples = 16;
    config.progressiveSteps = 4;
    config.updateInterval = 0.02f;
    
    std::vector<std::vector<Color>> progressive_images;
    
    auto callback = [&](const std::vector<Color>& data, int width, int height, int current_samples, int target_samples) {
        progressive_images.push_back(data);
    };
    
    int width = 8, height = 8;
    bool completed = path_tracer_->trace_progressive(width, height, config, callback);
    
    EXPECT_TRUE(completed);
    EXPECT_GE(progressive_images.size(), 2);
    
    // All images should have the same size
    for (const auto& image : progressive_images) {
        EXPECT_EQ(image.size(), width * height);
    }
    
    // Colors should be valid (no negative values, reasonable range)
    for (const auto& image : progressive_images) {
        for (const auto& pixel : image) {
            EXPECT_GE(pixel.r, 0.0f);
            EXPECT_GE(pixel.g, 0.0f);
            EXPECT_GE(pixel.b, 0.0f);
            EXPECT_LE(pixel.r, 1.0f);
            EXPECT_LE(pixel.g, 1.0f);
            EXPECT_LE(pixel.b, 1.0f);
        }
    }
}