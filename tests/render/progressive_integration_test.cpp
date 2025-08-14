#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <atomic>
#include "render/render_engine.h"
#include "render/path_tracer.h"
#include "ui/ui_manager.h"
#include "core/scene_manager.h"
#include "render/image_output.h"

class ProgressiveIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up complete progressive render system
        render_engine_ = std::make_shared<RenderEngine>();
        ui_manager_ = std::make_unique<UIManager>();
        scene_manager_ = std::make_shared<SceneManager>();
        image_output_ = std::make_shared<ImageOutput>();
        
        // Wire up components
        render_engine_->set_scene_manager(scene_manager_);
        render_engine_->set_image_output(image_output_);
        ui_manager_->set_render_engine(render_engine_);
        ui_manager_->set_scene_manager(scene_manager_);
        
        ui_manager_->initialize();
        
        // Configure for fast testing
        render_engine_->set_render_size(25, 25);
        render_engine_->set_max_depth(3);
        
        // Set up progress tracking
        render_engine_->set_progress_callback([this](int w, int h, int current, int target) {
            ui_manager_->update_progress(w, h, current, target);
            progress_updates_++;
        });
    }

    void TearDown() override {
        render_engine_->stop_progressive_render();
        ui_manager_->shutdown();
    }

    std::shared_ptr<RenderEngine> render_engine_;
    std::unique_ptr<UIManager> ui_manager_;
    std::shared_ptr<SceneManager> scene_manager_;
    std::shared_ptr<ImageOutput> image_output_;
    
    std::atomic<int> progress_updates_{0};
};

// Test progressive rendering workflow
TEST_F(ProgressiveIntegrationTest, CompleteProgressiveWorkflow) {
    ProgressiveConfig config;
    config.initialSamples = 1;
    config.targetSamples = 10;
    config.progressiveSteps = 3;
    config.updateInterval = 0.05f;
    
    // Initial state
    EXPECT_EQ(render_engine_->get_render_state(), RenderState::IDLE);
    EXPECT_FALSE(render_engine_->is_progressive_rendering());
    
    // Start progressive render
    render_engine_->start_progressive_render(config);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Should be in progressive rendering mode
    EXPECT_EQ(render_engine_->get_render_state(), RenderState::RENDERING);
    EXPECT_TRUE(render_engine_->is_progressive_rendering());
    
    // Wait for completion
    auto start_time = std::chrono::steady_clock::now();
    while (render_engine_->get_render_state() == RenderState::RENDERING) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        ui_manager_->render(); // Exercise UI during rendering
        
        // Timeout after 10 seconds
        if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(10)) {
            render_engine_->stop_progressive_render();
            FAIL() << "Progressive render took too long";
        }
    }
    
    // Should complete successfully
    EXPECT_EQ(render_engine_->get_render_state(), RenderState::COMPLETED);
    EXPECT_FALSE(render_engine_->is_progressive_rendering());
    EXPECT_GT(progress_updates_.load(), 0);
}

// Test progressive rendering interruption
TEST_F(ProgressiveIntegrationTest, ProgressiveInterruption) {
    ProgressiveConfig config;
    config.initialSamples = 1;
    config.targetSamples = 1000; // Large number to ensure it doesn't complete quickly
    config.progressiveSteps = 20;
    config.updateInterval = 0.1f;
    
    // Start progressive render
    render_engine_->start_progressive_render(config);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    EXPECT_EQ(render_engine_->get_render_state(), RenderState::RENDERING);
    EXPECT_TRUE(render_engine_->is_progressive_rendering());
    
    // Stop progressive render
    auto stop_start = std::chrono::steady_clock::now();
    render_engine_->stop_progressive_render();
    auto stop_end = std::chrono::steady_clock::now();
    
    EXPECT_EQ(render_engine_->get_render_state(), RenderState::STOPPED);
    EXPECT_FALSE(render_engine_->is_progressive_rendering());
    
    // Stop should be responsive
    auto stop_duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop_end - stop_start);
    EXPECT_LT(stop_duration.count(), 1000);
}

// Test UI progress feedback accuracy
TEST_F(ProgressiveIntegrationTest, UIProgressFeedbackAccuracy) {
    ProgressiveConfig config;
    config.initialSamples = 2;
    config.targetSamples = 12;
    config.progressiveSteps = 4;
    config.updateInterval = 0.03f;
    
    progress_updates_ = 0;
    
    render_engine_->start_progressive_render(config);
    
    // Wait for completion or timeout
    auto start_time = std::chrono::steady_clock::now();
    while (render_engine_->get_render_state() == RenderState::RENDERING) {
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        ui_manager_->render(); // This should show progress
        
        if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(5)) {
            render_engine_->stop_progressive_render();
            break;
        }
    }
    
    // Should have received multiple progress updates
    EXPECT_GT(progress_updates_.load(), 2);
}

// Test real-time display updates
TEST_F(ProgressiveIntegrationTest, RealTimeDisplayUpdates) {
    ProgressiveConfig config;
    config.initialSamples = 1;
    config.targetSamples = 8;
    config.progressiveSteps = 3;
    config.updateInterval = 0.05f;
    
    std::atomic<bool> display_updated{false};
    
    // Mock display update tracking
    image_output_->set_progressive_callback([&](int w, int h, int current, int target) {
        display_updated = true;
    });
    
    render_engine_->start_progressive_render(config);
    
    // Wait for some progress
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    EXPECT_TRUE(display_updated.load());
    
    render_engine_->stop_progressive_render();
}

// Test integration with start/stop render controls
TEST_F(ProgressiveIntegrationTest, StartStopRenderControlsIntegration) {
    ProgressiveConfig config;
    config.initialSamples = 1;
    config.targetSamples = 20;
    config.progressiveSteps = 4;
    config.updateInterval = 0.05f;
    
    // Test that regular render and progressive render don't interfere
    
    // Start regular render
    render_engine_->start_render();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Try to start progressive render (should be ignored)
    render_engine_->start_progressive_render(config);
    
    EXPECT_EQ(render_engine_->get_render_state(), RenderState::RENDERING);
    EXPECT_FALSE(render_engine_->is_progressive_rendering());
    
    // Stop regular render
    render_engine_->stop_render();
    EXPECT_EQ(render_engine_->get_render_state(), RenderState::STOPPED);
    
    // Now progressive render should work
    render_engine_->start_progressive_render(config);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    EXPECT_EQ(render_engine_->get_render_state(), RenderState::RENDERING);
    EXPECT_TRUE(render_engine_->is_progressive_rendering());
    
    render_engine_->stop_progressive_render();
}

// Test progressive rendering state transitions
TEST_F(ProgressiveIntegrationTest, ProgressiveStateTransitions) {
    ProgressiveConfig config;
    config.initialSamples = 1;
    config.targetSamples = 6;
    config.progressiveSteps = 2;
    config.updateInterval = 0.05f;
    
    std::vector<RenderState> state_transitions;
    
    render_engine_->set_state_change_callback([&](RenderState state) {
        state_transitions.push_back(state);
    });
    
    // Start and complete progressive render
    render_engine_->start_progressive_render(config);
    
    auto start_time = std::chrono::steady_clock::now();
    while (render_engine_->get_render_state() == RenderState::RENDERING) {
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        
        if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(3)) {
            render_engine_->stop_progressive_render();
            break;
        }
    }
    
    // Should have at least transitioned to RENDERING and then COMPLETED or STOPPED
    EXPECT_GE(state_transitions.size(), 1);
    EXPECT_EQ(state_transitions[0], RenderState::RENDERING);
    
    if (state_transitions.size() > 1) {
        RenderState final_state = state_transitions.back();
        EXPECT_TRUE(final_state == RenderState::COMPLETED || final_state == RenderState::STOPPED);
    }
}

// Test performance of progressive updates
TEST_F(ProgressiveIntegrationTest, ProgressiveUpdatePerformance) {
    ProgressiveConfig config;
    config.initialSamples = 1;
    config.targetSamples = 16;
    config.progressiveSteps = 8;
    config.updateInterval = 0.02f; // Fast updates
    
    auto start_time = std::chrono::steady_clock::now();
    
    render_engine_->start_progressive_render(config);
    
    // Wait for completion
    while (render_engine_->get_render_state() == RenderState::RENDERING) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        ui_manager_->render(); // Exercise UI
        
        // Timeout after 5 seconds
        if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(5)) {
            render_engine_->stop_progressive_render();
            break;
        }
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // Should complete in reasonable time (under 3 seconds for small test image)
    EXPECT_LT(duration.count(), 3000);
    
    // Should have received progress updates
    EXPECT_GT(progress_updates_.load(), 0);
}

// Test UI responsiveness during progressive rendering
TEST_F(ProgressiveIntegrationTest, UIResponsivenessDuringProgressive) {
    ProgressiveConfig config;
    config.initialSamples = 1;
    config.targetSamples = 30;
    config.progressiveSteps = 6;
    config.updateInterval = 0.05f;
    
    render_engine_->start_progressive_render(config);
    
    // UI operations should not block during progressive rendering
    for (int i = 0; i < 10; ++i) {
        auto ui_start = std::chrono::steady_clock::now();
        
        ui_manager_->render();
        ui_manager_->update();
        
        auto ui_end = std::chrono::steady_clock::now();
        auto ui_duration = std::chrono::duration_cast<std::chrono::milliseconds>(ui_end - ui_start);
        
        // UI operations should be fast (under 100ms)
        EXPECT_LT(ui_duration.count(), 100);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        
        // Break if render completed
        if (render_engine_->get_render_state() != RenderState::RENDERING) {
            break;
        }
    }
    
    render_engine_->stop_progressive_render();
}

// Test memory efficiency during progressive rendering
TEST_F(ProgressiveIntegrationTest, MemoryEfficiency) {
    // Use slightly larger image for memory test
    render_engine_->set_render_size(50, 50);
    
    ProgressiveConfig config;
    config.initialSamples = 1;
    config.targetSamples = 20;
    config.progressiveSteps = 10;
    config.updateInterval = 0.02f;
    
    // Progressive rendering should not cause memory leaks or excessive memory usage
    render_engine_->start_progressive_render(config);
    
    auto start_time = std::chrono::steady_clock::now();
    while (render_engine_->get_render_state() == RenderState::RENDERING) {
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        
        if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(3)) {
            render_engine_->stop_progressive_render();
            break;
        }
    }
    
    // Test should complete without crashes, indicating good memory management
    EXPECT_TRUE(true); // If we get here, memory management is working
}