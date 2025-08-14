#include "image_output.h"
#include <fstream>
#include <iostream>
#include <chrono>

#ifdef USE_SDL
#include <SDL.h>
#endif

ImageOutput::ImageOutput() : width_(0), height_(0), window_open_(false), progress_callback_(nullptr) {
#ifdef USE_SDL
    window_ = nullptr;
    renderer_ = nullptr;
    texture_ = nullptr;
#endif
}

ImageOutput::~ImageOutput() {
    close_window();
}

void ImageOutput::set_image_data(const std::vector<Color>& data, int width, int height) {
    image_data_ = data;
    
    // Check if dimensions changed and recreate texture if needed
    if (width_ != width || height_ != height) {
        width_ = width;
        height_ = height;
        
        #ifdef USE_SDL
        // Recreate texture with new dimensions
        if (texture_) {
            SDL_DestroyTexture(texture_);
            texture_ = nullptr;
        }
        
        if (renderer_ && window_open_) {
            texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, width, height);
            if (!texture_) {
                std::cerr << "Failed to recreate texture: " << SDL_GetError() << std::endl;
            } else {
                std::cout << "Recreated texture with dimensions: " << width << "x" << height << std::endl;
            }
        }
        #endif
    }
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
    
#ifdef USE_SDL
    if (!window_open_) {
        if (!create_window("Path Tracer - Camera View", width_, height_)) {
            std::cout << "Failed to create SDL window, falling back to console output" << std::endl;
            std::cout << "Image rendered: " << width_ << "x" << height_ 
                      << " (" << image_data_.size() << " pixels)" << std::endl;
            return;
        }
    }
    update_window();
    std::cout << "Image displayed in SDL window: " << width_ << "x" << height_ << std::endl;
#else
    std::cout << "Image rendered: " << width_ << "x" << height_ 
              << " (" << image_data_.size() << " pixels)" << std::endl;
    std::cout << "Image data available for display" << std::endl;
#endif
}

void ImageOutput::clear() {
    image_data_.clear();
    width_ = 0;
    height_ = 0;
}

void ImageOutput::initialize_display(int width, int height) {
    // Create a placeholder image - start with black
    width_ = width;
    height_ = height;
    image_data_.resize(width * height);
    
    // Fill with black initially
    Color black(0.0f, 0.0f, 0.0f, 1.0f);
    for (auto& pixel : image_data_) {
        pixel = black;
    }
    
    // Create window if using SDL
#ifdef USE_SDL
    if (!window_open_) {
        create_window("Path Tracer - Camera View", width, height);
    }
    #endif
    
    std::cout << "Display initialized: " << width << "x" << height << std::endl;
}

void ImageOutput::update_camera_preview(const Vector3& /* camera_pos */, const Vector3& /* camera_target */) {
    // This method is now deprecated - camera preview is handled by progressive rendering
    // Keeping the method for compatibility but it does nothing
    // Real-time updates are now handled through progressive rendering system
}

void ImageOutput::update_progressive_display(const std::vector<Color>& data, int width, int height, int current_samples, int target_samples) {
    // Update image data with progressive result
    set_image_data(data, width, height);
    
    // Throttled display updates to avoid excessive UI overhead
    static auto last_display_update = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_display_update).count();
    
    // Limit display updates to maximum 10 FPS (100ms) for performance
    const int MIN_UPDATE_INTERVAL_MS = 100;
    bool should_update_display = (elapsed_ms >= MIN_UPDATE_INTERVAL_MS);
    
    // Always update on final sample or significant progress milestones
    float progress_percent = float(current_samples) / target_samples;
    static float last_reported_progress = 0.0f;
    bool significant_progress = (progress_percent - last_reported_progress) >= 0.05f; // 5% increments
    
    if (should_update_display || current_samples == target_samples || significant_progress) {
        // Update display in real-time
#ifdef USE_SDL
        if (window_open_) {
            update_window();
            if (current_samples == target_samples || significant_progress) {
                std::cout << "Progressive update: " << current_samples << "/" << target_samples 
                          << " samples (" << int(100 * progress_percent) << "%)" << std::endl;
            }
        }
#else
        if (current_samples == target_samples || significant_progress) {
            std::cout << "Progressive render update: " << current_samples << "/" << target_samples 
                      << " samples (" << int(100 * progress_percent) << "%)" << std::endl;
        }
#endif
        
        last_display_update = now;
        if (significant_progress) {
            last_reported_progress = progress_percent;
        }
    }
    
    // Always call progress callback for UI updates (they handle their own throttling)
    if (progress_callback_) {
        progress_callback_(width, height, current_samples, target_samples);
    }
}

bool ImageOutput::create_window(const std::string& title, int width, int height) {
#ifdef USE_SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Set OpenGL attributes for compatibility with Hyprland/Wayland
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    
    window_ = SDL_CreateWindow(title.c_str(),
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              width, height,
                              SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    
    if (!window_) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return false;
    }
    
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_) {
        std::cerr << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        SDL_Quit();
        return false;
    }
    
    texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, width, height);
    if (!texture_) {
        std::cerr << "SDL_CreateTexture Error: " << SDL_GetError() << std::endl;
        SDL_DestroyRenderer(renderer_);
        SDL_DestroyWindow(window_);
        renderer_ = nullptr;
        window_ = nullptr;
        SDL_Quit();
        return false;
    }
    
    window_open_ = true;
    std::cout << "Created SDL+OpenGL window: " << width << "x" << height << std::endl;
    return true;
#else
    return false;
#endif
}

void ImageOutput::update_window() {
#ifdef USE_SDL
    if (!window_open_ || !texture_ || image_data_.empty()) {
        return;
    }
    
    update_texture();
    
    // Get window size to ensure proper scaling
    int window_width, window_height;
    SDL_GetWindowSize(window_, &window_width, &window_height);
    
    SDL_RenderClear(renderer_);
    
    // Scale texture to fill entire window
    SDL_Rect dest_rect = {0, 0, window_width, window_height};
    SDL_RenderCopy(renderer_, texture_, nullptr, &dest_rect);
    
    SDL_RenderPresent(renderer_);
#endif
}

void ImageOutput::close_window() {
#ifdef USE_SDL
    if (texture_) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }
    if (renderer_) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    if (window_open_) {
        SDL_Quit();
        window_open_ = false;
    }
#endif
}

bool ImageOutput::is_window_open() const {
    return window_open_;
}

void ImageOutput::update_texture() {
#ifdef USE_SDL
    if (!texture_ || image_data_.empty()) {
        return;
    }
    
    void* pixels;
    int pitch;
    
    if (SDL_LockTexture(texture_, nullptr, &pixels, &pitch) != 0) {
        std::cerr << "SDL_LockTexture Error: " << SDL_GetError() << std::endl;
        return;
    }
    
    Uint8* pixel_bytes = static_cast<Uint8*>(pixels);
    
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            int index = y * width_ + x;
            if (index < static_cast<int>(image_data_.size())) {
                Color pixel = image_data_[index].clamped();
                int pixel_offset = y * pitch + x * 3;
                
                pixel_bytes[pixel_offset + 0] = static_cast<Uint8>(255 * pixel.r); // R
                pixel_bytes[pixel_offset + 1] = static_cast<Uint8>(255 * pixel.g); // G
                pixel_bytes[pixel_offset + 2] = static_cast<Uint8>(255 * pixel.b); // B
            }
        }
    }
    
    SDL_UnlockTexture(texture_);
#endif
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