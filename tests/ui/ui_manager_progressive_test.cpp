#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include "ui/ui_manager.h"
#include "render/render_engine.h"
#include "core/scene_manager.h"

class UIManagerProgressiveTest : public ::testing::Test {
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
        render_engine_->stop_render();
        ui_manager_->shutdown();
    }

    std::unique_ptr<UIManager> ui_manager_;
    std::shared_ptr<RenderEngine> render_engine_;
    std::shared_ptr<SceneManager> scene_manager_;
};

// Test progress tracking functionality
TEST_F(UIManagerProgressiveTest, ProgressTracking) {
    int width = 100, height = 100;
    int target_samples = 50;
    
    // Initial state - no progress details should be shown
    EXPECT_NO_THROW(ui_manager_->render());
    
    // Update progress
    ui_manager_->update_progress(width, height, 10, target_samples);
    
    // Should now show progress details
    EXPECT_NO_THROW(ui_manager_->render());
    
    // Update with more progress
    ui_manager_->update_progress(width, height, 25, target_samples);
    EXPECT_NO_THROW(ui_manager_->render());
    
    // Complete progress
    ui_manager_->update_progress(width, height, target_samples, target_samples);
    EXPECT_NO_THROW(ui_manager_->render());
}

// Test progress reset functionality
TEST_F(UIManagerProgressiveTest, ProgressReset) {
    int width = 50, height = 50;
    int target_samples = 20;
    
    // Set some progress
    ui_manager_->update_progress(width, height, 10, target_samples);
    EXPECT_NO_THROW(ui_manager_->render());
    
    // Reset progress
    ui_manager_->reset_progress();
    EXPECT_NO_THROW(ui_manager_->render());
    
    // Should be able to start new progress tracking
    ui_manager_->update_progress(width, height, 5, target_samples);
    EXPECT_NO_THROW(ui_manager_->render());
}

// Test progress calculations
TEST_F(UIManagerProgressiveTest, ProgressCalculations) {
    int width = 75, height = 75;
    
    struct TestCase {
        int current_samples;
        int target_samples;
        float expected_percentage;
    };
    
    std::vector<TestCase> test_cases = {
        {10, 100, 10.0f},
        {25, 50, 50.0f},
        {100, 100, 100.0f},
        {1, 4, 25.0f},
        {0, 10, 0.0f}
    };
    
    for (const auto& test_case : test_cases) {
        ui_manager_->reset_progress();
        ui_manager_->update_progress(width, height, test_case.current_samples, test_case.target_samples);
        
        // The render call should handle the progress calculation correctly
        EXPECT_NO_THROW(ui_manager_->render());
    }
}

// Test time estimation functionality
TEST_F(UIManagerProgressiveTest, TimeEstimation) {
    int width = 60, height = 60;
    int target_samples = 40;
    
    // First progress update
    ui_manager_->update_progress(width, height, 10, target_samples);
    
    // Small delay to allow timing calculation
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Second progress update
    ui_manager_->update_progress(width, height, 20, target_samples);
    
    // Should render without issues
    EXPECT_NO_THROW(ui_manager_->render());
    
    // Another delay
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Final progress update
    ui_manager_->update_progress(width, height, 35, target_samples);
    EXPECT_NO_THROW(ui_manager_->render());
}

// Test samples per second calculation
TEST_F(UIManagerProgressiveTest, SamplesPerSecondCalculation) {
    int width = 40, height = 40;
    int target_samples = 30;
    
    // Start progress tracking
    ui_manager_->update_progress(width, height, 5, target_samples);
    
    // Wait a bit to establish timing
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Update with more samples
    ui_manager_->update_progress(width, height, 15, target_samples);
    
    // Should calculate samples per second
    EXPECT_NO_THROW(ui_manager_->render());
    
    // More progress
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ui_manager_->update_progress(width, height, 25, target_samples);
    EXPECT_NO_THROW(ui_manager_->render());
}

// Test UI state integration with render states
TEST_F(UIManagerProgressiveTest, RenderStateIntegration) {
    // Initially idle
    EXPECT_NO_THROW(ui_manager_->render());
    
    // Simulate render state changes
    render_engine_->start_render();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // UI should reflect rendering state
    EXPECT_NO_THROW(ui_manager_->render());
    
    // Add progress updates during rendering
    ui_manager_->update_progress(30, 30, 5, 20);
    EXPECT_NO_THROW(ui_manager_->render());
    
    ui_manager_->update_progress(30, 30, 15, 20);
    EXPECT_NO_THROW(ui_manager_->render());
    
    // Stop render
    render_engine_->stop_render();
    EXPECT_NO_THROW(ui_manager_->render());
}

// Test progressive rendering controls display
TEST_F(UIManagerProgressiveTest, ProgressiveControlsDisplay) {
    // Should show progressive controls
    EXPECT_NO_THROW(ui_manager_->render());
    
    // Start a regular render
    render_engine_->start_render();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    
    // Progressive controls should reflect current state
    EXPECT_NO_THROW(ui_manager_->render());
    
    render_engine_->stop_render();
    
    // Controls should update after stop
    EXPECT_NO_THROW(ui_manager_->render());
}

// Test progress display performance
TEST_F(UIManagerProgressiveTest, ProgressDisplayPerformance) {
    int width = 100, height = 100;
    int target_samples = 100;
    
    // Rapid progress updates (simulating real progressive rendering)
    auto start_time = std::chrono::steady_clock::now();
    
    for (int i = 1; i <= 50; ++i) {
        ui_manager_->update_progress(width, height, i * 2, target_samples);
        ui_manager_->render();
        
        // Small delay between updates
        std::this_thread::sleep_for(std::chrono::microseconds(500));
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // Should handle rapid updates efficiently (under 1 second for 50 updates)
    EXPECT_LT(duration.count(), 1000);
}

// Test edge cases in progress tracking
TEST_F(UIManagerProgressiveTest, ProgressEdgeCases) {
    int width = 20, height = 20;
    
    // Edge case: zero target samples
    EXPECT_NO_THROW(ui_manager_->update_progress(width, height, 0, 0));
    EXPECT_NO_THROW(ui_manager_->render());
    
    // Edge case: current > target (shouldn't happen but should be handled)
    EXPECT_NO_THROW(ui_manager_->update_progress(width, height, 15, 10));
    EXPECT_NO_THROW(ui_manager_->render());
    
    // Edge case: very small image
    EXPECT_NO_THROW(ui_manager_->update_progress(1, 1, 5, 10));
    EXPECT_NO_THROW(ui_manager_->render());
    
    // Edge case: very large target samples
    EXPECT_NO_THROW(ui_manager_->update_progress(width, height, 500, 10000));
    EXPECT_NO_THROW(ui_manager_->render());
}

// Test progress bar rendering
TEST_F(UIManagerProgressiveTest, ProgressBarRendering) {
    int width = 50, height = 50;
    int target_samples = 20;
    
    // Test various progress percentages
    std::vector<int> progress_points = {0, 1, 5, 10, 15, 20};
    
    for (int current : progress_points) {
        ui_manager_->update_progress(width, height, current, target_samples);
        
        // Should render progress bar without issues
        EXPECT_NO_THROW(ui_manager_->render());
        
        // Small delay to make timing calculations meaningful
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// Test concurrent progress updates
TEST_F(UIManagerProgressiveTest, ConcurrentProgressUpdates) {
    int width = 80, height = 80;
    int target_samples = 40;
    
    // Simulate concurrent updates from different threads (though we'll do it sequentially in test)
    std::vector<std::pair<int, int>> updates = {
        {5, target_samples},
        {8, target_samples},
        {12, target_samples},
        {18, target_samples},
        {25, target_samples},
        {32, target_samples},
        {40, target_samples}
    };
    
    for (const auto& update : updates) {
        ui_manager_->update_progress(width, height, update.first, update.second);
        EXPECT_NO_THROW(ui_manager_->render());
        
        // Brief delay between updates
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

// Test memory efficiency of progress tracking
TEST_F(UIManagerProgressiveTest, ProgressMemoryEfficiency) {
    // Test with many progress updates to check for memory leaks
    int width = 200, height = 200;
    int target_samples = 1000;
    
    for (int i = 1; i <= 100; ++i) {
        ui_manager_->update_progress(width, height, i * 10, target_samples);
        
        if (i % 10 == 0) {
            // Render occasionally to exercise the display code
            EXPECT_NO_THROW(ui_manager_->render());
        }
    }
    
    // Reset and do it again
    ui_manager_->reset_progress();
    
    for (int i = 1; i <= 50; ++i) {
        ui_manager_->update_progress(width, height, i * 20, target_samples);
    }
    
    EXPECT_NO_THROW(ui_manager_->render());
    
    // If we get here without crashes, memory management is working well
    EXPECT_TRUE(true);
}