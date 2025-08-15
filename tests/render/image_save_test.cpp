#include "render/image_output.h"
#include "core/common.h"
#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <vector>

class ImageSaveTest : public ::testing::Test {
protected:
    void SetUp() override {
        image_output = std::make_unique<ImageOutput>();
        
        // Create test image data - a simple gradient
        width = 4;
        height = 4;
        test_image_data.reserve(width * height);
        
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float r = static_cast<float>(x) / (width - 1);
                float g = static_cast<float>(y) / (height - 1);
                float b = 0.5f;
                test_image_data.emplace_back(r, g, b, 1.0f);
            }
        }
        
        image_output->set_image_data(test_image_data, width, height);
    }
    
    void TearDown() override {
        // Clean up test files
        std::vector<std::string> test_files = {
            "test_output.ppm",
            "test_output.png", 
            "test_output.jpg",
            "invalid_test<>.ppm",
            "test_metadata.ppm"
        };
        
        for (const auto& file : test_files) {
            if (std::filesystem::exists(file)) {
                std::filesystem::remove(file);
            }
        }
    }
    
    std::unique_ptr<ImageOutput> image_output;
    std::vector<Color> test_image_data;
    int width, height;
};

// Test PPM file format saving
TEST_F(ImageSaveTest, SaveAsPPM) {
    const std::string filename = "test_output.ppm";
    
    // Save the image
    EXPECT_NO_THROW(image_output->save_to_file(filename));
    
    // Verify file exists
    EXPECT_TRUE(std::filesystem::exists(filename));
    
    // Verify file content
    std::ifstream file(filename);
    ASSERT_TRUE(file.is_open());
    
    std::string line;
    
    // Check header
    std::getline(file, line);
    EXPECT_EQ(line, "P3");
    
    // Skip comments (metadata)
    while (std::getline(file, line) && line[0] == '#') {
        // Continue reading comments
    }
    
    // Check dimensions
    EXPECT_EQ(line, "4 4");
    
    // Check max value
    std::getline(file, line);
    EXPECT_EQ(line, "255");
    
    // Check pixel data (at least first pixel)
    int r, g, b;
    file >> r >> g >> b;
    EXPECT_EQ(r, 0);    // First pixel should be black (0,0,0.5) -> (0,0,127)
    EXPECT_EQ(g, 0);
    EXPECT_EQ(b, 127);  // 0.5 * 255 = 127.5 -> 127
    
    file.close();
}

// Test format detection from file extension
TEST_F(ImageSaveTest, FormatDetection) {
    // Test PNG extension (should fall back to PPM with message)
    EXPECT_NO_THROW(image_output->save_to_file("test_output.png"));
    EXPECT_TRUE(std::filesystem::exists("test_output.ppm")); // Falls back to PPM
    
    // Test JPEG extension (should fall back to PPM with message)
    EXPECT_NO_THROW(image_output->save_to_file("test_output.jpg"));
    EXPECT_TRUE(std::filesystem::exists("test_output.ppm")); // Falls back to PPM
}

// Test save with format specification
TEST_F(ImageSaveTest, SaveWithFormat) {
    // Test PNG format (should fall back to PPM)
    EXPECT_TRUE(image_output->save_with_format("test_output.png", ImageFormat::PNG));
    
    // Test JPEG format (should fall back to PPM) 
    EXPECT_TRUE(image_output->save_with_format("test_output.jpg", ImageFormat::JPEG, 85));
    
    // Test PPM format (should work normally)
    EXPECT_TRUE(image_output->save_with_format("test_output.ppm", ImageFormat::PPM));
    EXPECT_TRUE(std::filesystem::exists("test_output.ppm"));
}

// Test error handling - invalid filename
TEST_F(ImageSaveTest, InvalidFilename) {
    // Test empty filename
    EXPECT_THROW(image_output->save_with_format("", ImageFormat::PPM), std::runtime_error);
    
    // Test filename with invalid characters
    EXPECT_THROW(image_output->save_with_format("invalid_test<>.ppm", ImageFormat::PPM), std::runtime_error);
}

// Test error handling - no image data
TEST_F(ImageSaveTest, NoImageData) {
    ImageOutput empty_output;
    
    // Try to save without setting image data
    EXPECT_FALSE(empty_output.save_with_format("test_empty.ppm", ImageFormat::PPM));
}

// Test metadata inclusion
TEST_F(ImageSaveTest, MetadataInclusion) {
    const std::string filename = "test_metadata.ppm";
    
    // Save with metadata
    EXPECT_NO_THROW(image_output->save_to_file(filename));
    
    // Read file and check for metadata comments
    std::ifstream file(filename);
    ASSERT_TRUE(file.is_open());
    
    std::string line;
    bool found_renderer_info = false;
    bool found_resolution_info = false;
    
    while (std::getline(file, line)) {
        if (line.find("Path Tracer Renderer") != std::string::npos) {
            found_renderer_info = true;
        }
        if (line.find("Resolution: 4x4") != std::string::npos) {
            found_resolution_info = true;
        }
        if (line[0] != '#' && line != "P3") {
            break; // Stop at actual image data
        }
    }
    
    EXPECT_TRUE(found_renderer_info);
    EXPECT_TRUE(found_resolution_info);
    
    file.close();
}

// Test save options structure
TEST_F(ImageSaveTest, SaveOptions) {
    SaveOptions options;
    options.format = ImageFormat::PPM;
    options.jpeg_quality = 95;
    options.include_metadata = true;
    options.default_filename = "test_options.ppm";
    
    EXPECT_NO_THROW(image_output->save_to_file("test_options.ppm", options));
    EXPECT_TRUE(std::filesystem::exists("test_options.ppm"));
}

// Test color clamping
TEST_F(ImageSaveTest, ColorClamping) {
    // Create image with out-of-range colors
    std::vector<Color> extreme_colors = {
        Color(-0.5f, 1.5f, 0.5f, 1.0f),  // Negative red, over-range green
        Color(0.0f, 0.0f, 2.0f, 1.0f),   // Over-range blue
        Color(1.0f, 1.0f, 1.0f, 1.0f),   // Normal color
        Color(-1.0f, -1.0f, -1.0f, 1.0f) // All negative
    };
    
    image_output->set_image_data(extreme_colors, 2, 2);
    
    const std::string filename = "test_clamping.ppm";
    EXPECT_NO_THROW(image_output->save_to_file(filename));
    
    // Verify file was created successfully
    EXPECT_TRUE(std::filesystem::exists(filename));
    
    // Clean up
    std::filesystem::remove(filename);
}