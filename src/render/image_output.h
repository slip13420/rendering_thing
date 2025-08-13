#pragma once

#include "core/common.h"
#include <vector>
#include <string>

class ImageOutput {
public:
    ImageOutput();
    ~ImageOutput();
    
    void set_image_data(const std::vector<Color>& data, int width, int height);
    void save_to_file(const std::string& filename);
    void display_to_screen();
    void clear();
    
    // Get image properties
    int width() const { return width_; }
    int height() const { return height_; }
    const std::vector<Color>& get_data() const { return image_data_; }
    
private:
    std::vector<Color> image_data_;
    int width_;
    int height_;
    
    void save_as_ppm(const std::string& filename);
};