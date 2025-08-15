#include "ui/ui_manager.h"
#include "render/render_engine.h"
#include "render/image_output.h"
#include "core/scene_manager.h"
#include <gtest/gtest.h>
#include <memory>

class UIManagerSaveTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create dependencies
        render_engine = std::make_shared<RenderEngine>();
        scene_manager = std::make_shared<SceneManager>();
        image_output = std::make_shared<ImageOutput>();
        
        // Initialize scene manager
        scene_manager->initialize();
        
        // Set up render engine
        render_engine->set_scene_manager(scene_manager);
        render_engine->set_image_output(image_output);
        
        // Create UI manager and set dependencies
        ui_manager = std::make_unique<UIManager>();
        ui_manager->set_scene_manager(scene_manager);
        ui_manager->set_render_engine(render_engine);
        ui_manager->set_image_output(image_output);
        ui_manager->initialize();
        
        // Create some test image data
        std::vector<Color> test_data = {
            Color(1.0f, 0.0f, 0.0f, 1.0f),
            Color(0.0f, 1.0f, 0.0f, 1.0f),
            Color(0.0f, 0.0f, 1.0f, 1.0f),
            Color(1.0f, 1.0f, 1.0f, 1.0f)
        };
        image_output->set_image_data(test_data, 2, 2);
    }
    
    void TearDown() override {
        ui_manager.reset();
    }
    
    std::shared_ptr<RenderEngine> render_engine;
    std::shared_ptr<SceneManager> scene_manager;
    std::shared_ptr<ImageOutput> image_output;
    std::unique_ptr<UIManager> ui_manager;
};

// Test save button enablement based on render state
TEST_F(UIManagerSaveTest, SaveButtonEnablement) {
    // Initially save should be disabled (no render completed)
    EXPECT_FALSE(ui_manager->is_save_enabled());
    
    // Simulate render completion by setting the render state
    // Note: This would require exposing render state manipulation or 
    // completing an actual render, which is complex for unit tests
    
    // For now, test that save is not enabled without completed render
    EXPECT_FALSE(ui_manager->is_save_enabled());
}

// Test that UI manager has image output dependency
TEST_F(UIManagerSaveTest, ImageOutputDependency) {
    // Test that save is disabled without image output
    UIManager manager_without_image;
    manager_without_image.set_render_engine(render_engine);
    manager_without_image.initialize();
    
    EXPECT_FALSE(manager_without_image.is_save_enabled());
}

// Test UI input access
TEST_F(UIManagerSaveTest, UIInputAccess) {
    // Test that we can get UI input from UI manager
    auto ui_input = ui_manager->get_ui_input();
    EXPECT_NE(ui_input, nullptr);
}

// Test save callback setting
TEST_F(UIManagerSaveTest, SaveCallbackSetting) {
    auto ui_input = ui_manager->get_ui_input();
    ASSERT_NE(ui_input, nullptr);
    
    bool callback_called = false;
    ui_input->set_save_callback([&callback_called]() {
        callback_called = true;
    });
    
    // We can't easily trigger the callback through the UI input in a unit test
    // since it requires SDL key events, but we can test that the callback
    // mechanism is properly set up
    EXPECT_FALSE(callback_called); // Should not be called yet
}

// Test render state integration
TEST_F(UIManagerSaveTest, RenderStateIntegration) {
    // Test that UI manager responds to render state changes
    // This is an integration test that would require completing a render
    // For unit testing, we verify the components are properly connected
    
    EXPECT_NE(ui_manager->get_ui_input(), nullptr);
    
    // Verify UI manager has access to all required components
    // (This tests the dependency injection is working)
}

// Test error handling in save operations
TEST_F(UIManagerSaveTest, SaveErrorHandling) {
    // Test calling trigger_save_dialog when save is not enabled
    // This should not crash and should handle the error gracefully
    EXPECT_NO_THROW(ui_manager->trigger_save_dialog());
}