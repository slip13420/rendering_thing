#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include "render/image_output.h"

class ImageOutputProgressiveTest : public ::testing::Test {
protected:
    void SetUp() override {
        image_output_ = std::make_unique<ImageOutput>();
    }

    void TearDown() override {
        image_output_.reset();
    }

    std::unique_ptr<ImageOutput> image_output_;
    
    // Helper to create test image data
    std::vector<Color> create_test_image(int width, int height, float brightness = 0.5f) {
        std::vector<Color> data(width * height);
        for (int i = 0; i < width * height; ++i) {
            data[i] = Color(brightness, brightness * 0.8f, brightness * 0.6f);
        }
        return data;
    }
};

// Test progressive display update functionality
TEST_F(ImageOutputProgressiveTest, ProgressiveDisplayUpdate) {
    int width = 100, height = 100;
    int target_samples = 50;
    
    bool callback_called = false;
    image_output_->set_progressive_callback([&](int w, int h, int current, int target) {
        callback_called = true;
        EXPECT_EQ(w, width);
        EXPECT_EQ(h, height);
        EXPECT_LE(current, target);
    });
    
    auto test_image = create_test_image(width, height, 0.3f);
    
    EXPECT_NO_THROW(image_output_->update_progressive_display(test_image, width, height, 10, target_samples));
    EXPECT_TRUE(callback_called);
}

// Test progressive update with varying sample counts
TEST_F(ImageOutputProgressiveTest, VaryingSampleCounts) {
    int width = 50, height = 50;
    auto test_image = create_test_image(width, height);
    
    std::vector<int> sample_counts = {1, 5, 10, 25, 50, 100};
    int target_samples = 100;
    
    for (int current_samples : sample_counts) {
        EXPECT_NO_THROW(image_output_->update_progressive_display(
            test_image, width, height, current_samples, target_samples));
    }
}

// Test performance of progressive updates
TEST_F(ImageOutputProgressiveTest, UpdatePerformance) {
    int width = 200, height = 200;
    auto test_image = create_test_image(width, height);
    int target_samples = 100;
    
    // Time multiple updates
    auto start_time = std::chrono::steady_clock::now();
    
    for (int i = 1; i <= 20; ++i) {
        image_output_->update_progressive_display(test_image, width, height, i * 5, target_samples);
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // Updates should be reasonably fast (under 1 second for 20 updates)
    EXPECT_LT(duration.count(), 1000);
}

// Test callback mechanism
TEST_F(ImageOutputProgressiveTest, CallbackMechanism) {
    int callback_count = 0;
    int last_current_samples = 0;
    int last_target_samples = 0;
    
    image_output_->set_progressive_callback([&](int w, int h, int current, int target) {
        callback_count++;
        last_current_samples = current;
        last_target_samples = target;
    });
    
    int width = 30, height = 30;
    int target_samples = 20;
    auto test_image = create_test_image(width, height);
    
    // Multiple progressive updates
    for (int i = 1; i <= 5; ++i) {
        image_output_->update_progressive_display(test_image, width, height, i * 4, target_samples);
    }
    
    EXPECT_GT(callback_count, 0);
    EXPECT_EQ(last_current_samples, 20);
    EXPECT_EQ(last_target_samples, target_samples);
}

// Test image data persistence
TEST_F(ImageOutputProgressiveTest, ImageDataPersistence) {
    int width = 40, height = 30;
    auto test_image1 = create_test_image(width, height, 0.2f);
    auto test_image2 = create_test_image(width, height, 0.8f);
    
    // First update
    image_output_->update_progressive_display(test_image1, width, height, 10, 50);
    
    // Second update with different data
    image_output_->update_progressive_display(test_image2, width, height, 25, 50);
    
    // Should not crash and should handle the data change
    EXPECT_NO_THROW(image_output_->display_to_screen());
}

// Test update throttling behavior
TEST_F(ImageOutputProgressiveTest, UpdateThrottling) {
    int width = 100, height = 100;
    auto test_image = create_test_image(width, height);
    int target_samples = 100;
    
    // Rapid updates should be throttled internally
    auto start_time = std::chrono::steady_clock::now();
    
    for (int i = 1; i <= 50; ++i) {
        image_output_->update_progressive_display(test_image, width, height, i * 2, target_samples);
        // Very small delay
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // Should handle rapid updates without significant performance degradation
    // Allow some reasonable time but not excessive
    EXPECT_LT(duration.count(), 2000); // Under 2 seconds for 50 rapid updates
}

// Test edge cases
TEST_F(ImageOutputProgressiveTest, EdgeCases) {
    int width = 10, height = 10;
    auto test_image = create_test_image(width, height);
    
    // Edge case: current_samples equals target_samples
    EXPECT_NO_THROW(image_output_->update_progressive_display(test_image, width, height, 50, 50));
    
    // Edge case: single sample
    EXPECT_NO_THROW(image_output_->update_progressive_display(test_image, width, height, 1, 1));
    
    // Edge case: very small image
    auto small_image = create_test_image(1, 1);
    EXPECT_NO_THROW(image_output_->update_progressive_display(small_image, 1, 1, 5, 10));
}

// Test memory efficiency with large images
TEST_F(ImageOutputProgressiveTest, LargeImageHandling) {
    int width = 500, height = 500; // Large image
    auto test_image = create_test_image(width, height);
    
    // Should handle large images without issues
    EXPECT_NO_THROW(image_output_->update_progressive_display(test_image, width, height, 10, 100));
    EXPECT_NO_THROW(image_output_->update_progressive_display(test_image, width, height, 50, 100));
    EXPECT_NO_THROW(image_output_->update_progressive_display(test_image, width, height, 100, 100));
}

// Test progressive quality indication
TEST_F(ImageOutputProgressiveTest, QualityProgression) {
    int width = 20, height = 20;
    int target_samples = 50;
    
    struct ProgressData {
        int current_samples;
        float progress_percentage;
    };
    
    std::vector<ProgressData> progress_records;
    
    image_output_->set_progressive_callback([&](int w, int h, int current, int target) {
        float percentage = (100.0f * current) / target;
        progress_records.push_back({current, percentage});
    });
    
    auto test_image = create_test_image(width, height);
    
    // Simulate progressive quality improvement
    std::vector<int> sample_progression = {1, 5, 15, 30, 50};
    for (int samples : sample_progression) {
        image_output_->update_progressive_display(test_image, width, height, samples, target_samples);
    }
    
    EXPECT_EQ(progress_records.size(), sample_progression.size());
    
    // Verify progression
    for (size_t i = 1; i < progress_records.size(); ++i) {
        EXPECT_GE(progress_records[i].current_samples, progress_records[i-1].current_samples);
        EXPECT_GE(progress_records[i].progress_percentage, progress_records[i-1].progress_percentage);
    }
    
    // Final progress should be 100%
    EXPECT_FLOAT_EQ(progress_records.back().progress_percentage, 100.0f);
}