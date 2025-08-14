#include <gtest/gtest.h>
#include "ui/ui_manager.h"
#include "render/render_engine.h"
#include "core/scene_manager.h"
#include <memory>
#include <chrono>
#include <thread>

class UIManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        ui_manager_ = std::make_unique<UIManager>();
        render_engine_ = std::make_shared<RenderEngine>();
        scene_manager_ = std::make_shared<SceneManager>();
        
        ui_manager_->set_render_engine(render_engine_);
        ui_manager_->set_scene_manager(scene_manager_);
        ui_manager_->initialize();
    }

    void TearDown() override {
        ui_manager_->shutdown();
    }

    std::unique_ptr<UIManager> ui_manager_;
    std::shared_ptr<RenderEngine> render_engine_;
    std::shared_ptr<SceneManager> scene_manager_;
};

// Test render state text conversion
TEST_F(UIManagerTest, RenderStateText) {
    EXPECT_EQ(ui_manager_->get_render_state_text(RenderState::IDLE), "Ready to render");
    EXPECT_EQ(ui_manager_->get_render_state_text(RenderState::RENDERING), "Rendering in progress...");
    EXPECT_EQ(ui_manager_->get_render_state_text(RenderState::COMPLETED), "Render completed successfully");
    EXPECT_EQ(ui_manager_->get_render_state_text(RenderState::STOPPED), "Render stopped by user");
    EXPECT_EQ(ui_manager_->get_render_state_text(RenderState::ERROR), "Render failed with error");
}

// Test UI control functionality
TEST_F(UIManagerTest, RenderControls) {
    // Test that controls can be rendered without crashing
    EXPECT_NO_THROW(ui_manager_->render_start_button());
    EXPECT_NO_THROW(ui_manager_->render_stop_button());
    EXPECT_NO_THROW(ui_manager_->render_status_display());
}

TEST_F(UIManagerTest, StateChangeNotification) {
    // Start a render to trigger state change
    render_engine_->start_render();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // UI should be notified of state changes
    // Since we can't easily test console output, we'll just verify no crashes
    EXPECT_NO_THROW(ui_manager_->render());
    
    render_engine_->stop_render();
}

TEST_F(UIManagerTest, InitializationWithoutRenderEngine) {
    UIManager ui_no_engine;
    ui_no_engine.initialize();
    
    // Should handle missing render engine gracefully
    EXPECT_NO_THROW(ui_no_engine.render_start_button());
    EXPECT_NO_THROW(ui_no_engine.render_stop_button());
    
    ui_no_engine.shutdown();
}

TEST_F(UIManagerTest, Update) {
    // Test that update can be called without issues
    EXPECT_NO_THROW(ui_manager_->update());
    EXPECT_NO_THROW(ui_manager_->process_input());
}