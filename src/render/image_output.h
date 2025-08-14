#pragma once

#include "core/common.h"
#include <vector>
#include <string>
#include <functional>

#ifdef USE_SDL
struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;
#endif

// Callback for progressive rendering updates
using ProgressUpdateCallback = std::function<void(int, int, int, int)>;

class ImageOutput {
public:
    ImageOutput();
    ~ImageOutput();
    
    void set_image_data(const std::vector<Color>& data, int width, int height);
    void save_to_file(const std::string& filename);
    void display_to_screen();
    void clear();
    void initialize_display(int width, int height);
    void update_camera_preview(const Vector3& camera_pos, const Vector3& camera_target);
    
    // Progressive rendering support
    void update_progressive_display(const std::vector<Color>& data, int width, int height, int current_samples, int target_samples);
    void set_progressive_callback(ProgressUpdateCallback callback) { progress_callback_ = callback; }
    
    // SDL window management
    bool create_window(const std::string& title, int width, int height);
    void update_window();
    void close_window();
    bool is_window_open() const;
    
    // Get image properties
    int width() const { return width_; }
    int height() const { return height_; }
    const std::vector<Color>& get_data() const { return image_data_; }
    
private:
    std::vector<Color> image_data_;
    int width_;
    int height_;
    bool window_open_;
    
    ProgressUpdateCallback progress_callback_;
    
#ifdef USE_SDL
    SDL_Window* window_;
    SDL_Renderer* renderer_;
    SDL_Texture* texture_;
#endif
    
    void save_as_ppm(const std::string& filename);
    void update_texture();
};