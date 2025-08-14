#pragma once

#include "core/common.h"
#include <string>
#include <vector>
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
    
    // Image data management
    void set_image_data(const std::vector<Color>& data, int width, int height);
    void clear();
    
    // File operations
    void save_to_file(const std::string& filename);
    
    // Display operations
    void display_to_screen();
    void initialize_display(int width, int height);
    void update_camera_preview(const Vector3& camera_pos, const Vector3& camera_target);
    
    // Progressive rendering support
    void update_progressive_display(const std::vector<Color>& data, int width, int height, int current_samples, int target_samples);
    void set_progressive_callback(ProgressUpdateCallback callback) { progress_callback_ = callback; }
    
    // Window management
    bool is_window_open() const;
    void close_window();
    
    // Legacy method names for compatibility
    void saveToFile(const std::string& filename) { save_to_file(filename); }
    void displayToScreen() { display_to_screen(); }
    
private:
    bool create_window(const std::string& title, int width, int height);
    void update_window();
    void update_texture();
    void save_as_ppm(const std::string& filename);
    
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
};