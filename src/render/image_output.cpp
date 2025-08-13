#include "image_output.h"
#include <fstream>
#include <iostream>

ImageOutput::ImageOutput() : width_(0), height_(0) {
    
}

ImageOutput::~ImageOutput() {
    
}

void ImageOutput::set_image_data(const std::vector<Color>& data, int width, int height) {
    image_data_ = data;
    width_ = width;
    height_ = height;
}

void ImageOutput::save_to_file(const std::string& filename) {
    if (image_data_.empty()) {
        std::cerr << "No image data to save" << std::endl;
        return;
    }
    
    // Determine format by file extension
    std::string extension = filename.substr(filename.find_last_of('.') + 1);
    
    if (extension == "ppm") {
        save_as_ppm(filename);
    } else {
        std::cout << "Defaulting to PPM format for file: " << filename << std::endl;
        save_as_ppm(filename);
    }
}

void ImageOutput::display_to_screen() {
    if (image_data_.empty()) {
        std::cout << "No image data to display" << std::endl;
        return;
    }
    
    std::cout << "Image rendered: " << width_ << "x" << height_ 
              << " (" << image_data_.size() << " pixels)" << std::endl;
    std::cout << "Image data available for display" << std::endl;
}

void ImageOutput::clear() {
    image_data_.clear();
    width_ = 0;
    height_ = 0;
}

void ImageOutput::save_as_ppm(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Could not open file for writing: " << filename << std::endl;
        return;
    }
    
    // PPM header
    file << "P3\n";
    file << width_ << " " << height_ << "\n";
    file << "255\n";
    
    // Write pixel data
    for (const auto& pixel : image_data_) {
        Color clamped_pixel = pixel.clamped();
        int r = static_cast<int>(255 * clamped_pixel.r);
        int g = static_cast<int>(255 * clamped_pixel.g);
        int b = static_cast<int>(255 * clamped_pixel.b);
        
        file << r << " " << g << " " << b << "\n";
    }
    
    file.close();
    std::cout << "Image saved to " << filename << std::endl;
}