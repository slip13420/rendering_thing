#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include "render/render_engine.h"
#include "render/path_tracer.h"
#include "ui/ui_manager.h"
#include "core/scene_manager.h"
#include "render/image_output.h"

class RenderIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up complete render system
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
        render_engine_->set_render_size(50, 50);
        render_engine_->set_samples_per_pixel(2);
        render_engine_->set_max_depth(3);
    }

    void TearDown() override {
        render_engine_->stop_render();
        ui_manager_->shutdown();
    }

    std::shared_ptr<RenderEngine> render_engine_;
    std::unique_ptr<UIManager> ui_manager_;
    std::shared_ptr<SceneManager> scene_manager_;
    std::shared_ptr<ImageOutput> image_output_;
};

// Test complete start/stop workflow
TEST_F(RenderIntegrationTest, CompleteStartStopWorkflow) {
    // Initial state
    EXPECT_EQ(render_engine_->get_render_state(), RenderState::IDLE);
    
    // Start render
    render_engine_->start_render();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Should be rendering
    EXPECT_EQ(render_engine_->get_render_state(), RenderState::RENDERING);
    
    // UI should reflect the state
    EXPECT_NO_THROW(ui_manager_->render());
    
    // Stop render
    render_engine_->stop_render();
    
    // Should be stopped
    EXPECT_EQ(render_engine_->get_render_state(), RenderState::STOPPED);
    
    // UI should reflect the new state
    EXPECT_NO_THROW(ui_manager_->render());
}

// Test render completion workflow
TEST_F(RenderIntegrationTest, CompleteRenderWorkflow) {
    bool state_changes_detected = false;
    RenderState final_state = RenderState::IDLE;
    
    render_engine_->set_state_change_callback([&](RenderState state) {
        state_changes_detected = true;
        final_state = state;
    });
    
    // Start render and wait for completion
    render_engine_->start_render();
    
    auto start_time = std::chrono::steady_clock::now();
    while (render_engine_->get_render_state() == RenderState::RENDERING) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        ui_manager_->render(); // Exercise UI during rendering
        
        // Timeout after 10 seconds
        if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(10)) {
            render_engine_->stop_render();
            FAIL() << "Render took too long";
        }
    }
    
    EXPECT_TRUE(state_changes_detected);
    EXPECT_TRUE(final_state == RenderState::COMPLETED || final_state == RenderState::STOPPED);
}

// Test rapid start/stop operations
TEST_F(RenderIntegrationTest, RapidStartStop) {
    for (int i = 0; i < 5; ++i) {
        render_engine_->start_render();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        render_engine_->stop_render();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        // Should end up in stopped state
        EXPECT_EQ(render_engine_->get_render_state(), RenderState::STOPPED);
    }
}

// Test PathTracer cancellation integration
TEST_F(RenderIntegrationTest, PathTracerCancellation) {
    // Use larger render for longer execution
    render_engine_->set_render_size(200, 200);
    render_engine_->set_samples_per_pixel(20);
    
    render_engine_->start_render();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    EXPECT_EQ(render_engine_->get_render_state(), RenderState::RENDERING);
    
    // Cancel should work quickly
    auto stop_start = std::chrono::steady_clock::now();
    render_engine_->stop_render();
    auto stop_end = std::chrono::steady_clock::now();
    
    EXPECT_EQ(render_engine_->get_render_state(), RenderState::STOPPED);
    
    // Stop should be responsive (under 1 second)
    auto stop_duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop_end - stop_start);
    EXPECT_LT(stop_duration.count(), 1000);
}

// Test UI responsiveness during rendering
TEST_F(RenderIntegrationTest, UIResponsiveness) {
    render_engine_->start_render();
    
    // UI operations should not block during rendering
    for (int i = 0; i < 10; ++i) {
        auto ui_start = std::chrono::steady_clock::now();
        
        ui_manager_->render();
        ui_manager_->update();
        
        auto ui_end = std::chrono::steady_clock::now();
        auto ui_duration = std::chrono::duration_cast<std::chrono::milliseconds>(ui_end - ui_start);
        
        // UI operations should be fast (under 50ms)
        EXPECT_LT(ui_duration.count(), 50);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    render_engine_->stop_render();
}

// Test component integration after render completion
TEST_F(RenderIntegrationTest, ComponentIntegrationAfterCompletion) {
    render_engine_->start_render();
    
    // Wait for completion or timeout
    auto start_time = std::chrono::steady_clock::now();
    while (render_engine_->get_render_state() == RenderState::RENDERING) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(5)) {
            render_engine_->stop_render();
            break;
        }
    }
    
    // After render, image output should be available
    if (render_engine_->get_render_state() == RenderState::COMPLETED) {
        EXPECT_NO_THROW(render_engine_->save_image("test_output.ppm"));
        EXPECT_NO_THROW(render_engine_->display_image());
    }
}