#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include "render/render_engine.h"
#include "core/scene_manager.h"
#include "render/image_output.h"

class RenderEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        render_engine_ = std::make_unique<RenderEngine>();
        scene_manager_ = std::make_shared<SceneManager>();
        image_output_ = std::make_shared<ImageOutput>();
        
        render_engine_->set_scene_manager(scene_manager_);
        render_engine_->set_image_output(image_output_);
    }

    void TearDown() override {
        render_engine_->stop_render();
        render_engine_.reset();
    }

    std::unique_ptr<RenderEngine> render_engine_;
    std::shared_ptr<SceneManager> scene_manager_;
    std::shared_ptr<ImageOutput> image_output_;
};

// Test render state management
TEST_F(RenderEngineTest, InitialState) {
    EXPECT_EQ(render_engine_->get_render_state(), RenderState::IDLE);
    EXPECT_FALSE(render_engine_->is_rendering());
}

TEST_F(RenderEngineTest, StateTransitions) {
    // Test IDLE -> RENDERING transition
    render_engine_->start_render();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(render_engine_->get_render_state(), RenderState::RENDERING);
    EXPECT_TRUE(render_engine_->is_rendering());
    
    // Test RENDERING -> STOPPED transition
    render_engine_->stop_render();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(render_engine_->get_render_state(), RenderState::STOPPED);
    EXPECT_FALSE(render_engine_->is_rendering());
}

TEST_F(RenderEngineTest, StateChangeCallback) {
    RenderState captured_state = RenderState::IDLE;
    bool callback_called = false;
    
    render_engine_->set_state_change_callback([&](RenderState state) {
        captured_state = state;
        callback_called = true;
    });
    
    render_engine_->start_render();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    EXPECT_TRUE(callback_called);
    EXPECT_EQ(captured_state, RenderState::RENDERING);
}

TEST_F(RenderEngineTest, MultipleStartRender) {
    render_engine_->start_render();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Second start should be ignored
    render_engine_->start_render();
    
    EXPECT_EQ(render_engine_->get_render_state(), RenderState::RENDERING);
    
    render_engine_->stop_render();
}

TEST_F(RenderEngineTest, StopWhenNotRendering) {
    EXPECT_EQ(render_engine_->get_render_state(), RenderState::IDLE);
    
    // Stop when not rendering should be safe
    render_engine_->stop_render();
    
    EXPECT_EQ(render_engine_->get_render_state(), RenderState::IDLE);
}

// Test threading and cancellation
TEST_F(RenderEngineTest, BackgroundRendering) {
    render_engine_->set_render_size(100, 100); // Small size for faster test
    render_engine_->set_samples_per_pixel(5);  // Low sample count
    
    render_engine_->start_render();
    
    // Main thread should not be blocked
    auto start = std::chrono::steady_clock::now();
    while (render_engine_->get_render_state() == RenderState::RENDERING) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        // Timeout after 5 seconds
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed > std::chrono::seconds(5)) {
            render_engine_->stop_render();
            FAIL() << "Render took too long, possible threading issue";
        }
    }
    
    // Should complete or be stopped
    RenderState final_state = render_engine_->get_render_state();
    EXPECT_TRUE(final_state == RenderState::COMPLETED || final_state == RenderState::STOPPED);
}

TEST_F(RenderEngineTest, RenderCancellation) {
    render_engine_->set_render_size(500, 500); // Larger size for longer render
    render_engine_->set_samples_per_pixel(50); // High sample count
    
    render_engine_->start_render();
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Let it start
    
    EXPECT_EQ(render_engine_->get_render_state(), RenderState::RENDERING);
    
    // Cancel the render
    render_engine_->stop_render();
    
    EXPECT_EQ(render_engine_->get_render_state(), RenderState::STOPPED);
}

// Test state persistence
TEST_F(RenderEngineTest, StatePersistence) {
    // Test save/restore cycle
    render_engine_->save_render_state();
    render_engine_->restore_render_state();
    
    EXPECT_EQ(render_engine_->get_render_state(), RenderState::IDLE);
}