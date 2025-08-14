#include "image_output.h"
#include <fstream>
#include <iostream>

#ifdef USE_SDL
#include <SDL.h>
#endif

ImageOutput::ImageOutput() : width_(0), height_(0), window_open_(false) {
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

void ImageOutput::update_camera_preview(const Vector3& camera_pos, const Vector3& camera_target) {
    if (image_data_.empty()) return;
    
    // Simple rasterized preview - render basic scene geometry
    Vector3 camera_dir = (camera_target - camera_pos).normalized();
    Vector3 camera_right = camera_dir.cross(Vector3(0, 1, 0)).normalized();
    Vector3 camera_up = camera_right.cross(camera_dir).normalized();
    
    // Clear with sky color
    Color sky_color(0.3f, 0.5f, 0.8f, 1.0f);
    Color ground_color(0.2f, 0.3f, 0.1f, 1.0f);
    
    for (int y = 0; y < height_; y++) {
        for (int x = 0; x < width_; x++) {
            // Convert screen coordinates to camera ray direction
            float u = (2.0f * x / width_) - 1.0f;
            float v = 1.0f - (2.0f * y / height_);
            
            Vector3 ray_dir = (camera_dir + camera_right * u + camera_up * v).normalized();
            
            Color pixel_color = sky_color;
            
            // Simple ground plane at y = 0
            if (ray_dir.y < 0 && camera_pos.y > 0) {
                float t = -camera_pos.y / ray_dir.y;
                if (t > 0) {
                    Vector3 hit_point = camera_pos + ray_dir * t;
                    
                    // Checkerboard pattern on ground
                    int checker_x = static_cast<int>(hit_point.x + 1000) / 2;
                    int checker_z = static_cast<int>(hit_point.z + 1000) / 2;
                    bool checker = (checker_x + checker_z) % 2 == 0;
                    
                    pixel_color = checker ? Color(0.3f, 0.4f, 0.2f) : Color(0.1f, 0.2f, 0.05f);
                }
            }
            
            // Simple sphere at origin with radius 1
            Vector3 sphere_center(0, 1, 0);
            float sphere_radius = 1.0f;
            Vector3 to_sphere = camera_pos - sphere_center;
            
            float a = ray_dir.dot(ray_dir);
            float b = 2.0f * ray_dir.dot(to_sphere);
            float c = to_sphere.dot(to_sphere) - sphere_radius * sphere_radius;
            
            float discriminant = b * b - 4 * a * c;
            if (discriminant >= 0) {
                float t = (-b - std::sqrt(discriminant)) / (2 * a);
                if (t > 0.1f) { // Avoid rendering behind camera
                    Vector3 hit_point = camera_pos + ray_dir * t;
                    Vector3 normal = (hit_point - sphere_center).normalized();
                    
                    // Simple lighting
                    Vector3 light_dir(-0.3f, -1.0f, -0.3f);
                    light_dir = light_dir.normalized();
                    float light_intensity = std::max(0.0f, -normal.dot(light_dir));
                    
                    pixel_color = Color(0.8f, 0.3f, 0.3f) * (0.3f + 0.7f * light_intensity);
                }
            }
            
            image_data_[y * width_ + x] = pixel_color.clamped();
        }
    }
    
    // Update display
#ifdef USE_SDL
    if (window_open_) {
        update_window();
    }
#endif
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