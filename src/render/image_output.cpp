#include "image_output.h"
#include <fstream>
#include <iostream>
#include <chrono>
#include <algorithm>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <stdexcept>

#ifdef USE_SDL
#include <SDL.h>
#endif

ImageOutput::ImageOutput() : width_(0), height_(0), window_open_(false), progress_callback_(nullptr) {
#ifdef USE_SDL
    window_ = nullptr;
    renderer_ = nullptr;
    texture_ = nullptr;
    gl_context_ = nullptr;
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
    
    ImageFormat format = determine_format_from_extension(filename);
    save_with_format(filename, format);
}

void ImageOutput::save_to_file(const std::string& filename, const SaveOptions& options) {
    if (image_data_.empty()) {
        std::cerr << "No image data to save" << std::endl;
        return;
    }
    
    save_with_format(filename, options.format, options.jpeg_quality);
}

bool ImageOutput::save_with_format(const std::string& filename, ImageFormat format, int jpeg_quality) {
    if (image_data_.empty()) {
        std::cerr << "No image data to save" << std::endl;
        return false;
    }
    
    try {
        switch (format) {
            case ImageFormat::PNG:
                save_as_png(filename);
                break;
            case ImageFormat::JPEG:
                save_as_jpeg(filename, jpeg_quality);
                break;
            case ImageFormat::PPM:
                save_as_ppm(filename);
                break;
            default:
                std::cerr << "Unsupported image format" << std::endl;
                return false;
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error saving image: " << e.what() << std::endl;
        return false;
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
        // Signal main thread that progressive update is ready
        progressive_update_pending_ = true;
        
        if (current_samples == target_samples || significant_progress) {
            std::cout << "Progressive update: " << current_samples << "/" << target_samples 
                      << " samples (" << int(100 * progress_percent) << "%)" << std::endl;
        }
        
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
    
    // Create OpenGL context for compute shaders
    gl_context_ = static_cast<void*>(SDL_GL_CreateContext(window_));
    if (!gl_context_) {
        std::cerr << "Failed to create OpenGL context: " << SDL_GetError() << std::endl;
        SDL_DestroyTexture(texture_);
        SDL_DestroyRenderer(renderer_);
        SDL_DestroyWindow(window_);
        texture_ = nullptr;
        renderer_ = nullptr;
        window_ = nullptr;
        SDL_Quit();
        return false;
    }
    
    // Make the context current
    if (SDL_GL_MakeCurrent(window_, static_cast<SDL_GLContext>(gl_context_)) != 0) {
        std::cerr << "Failed to make OpenGL context current: " << SDL_GetError() << std::endl;
        SDL_GL_DeleteContext(static_cast<SDL_GLContext>(gl_context_));
        SDL_DestroyTexture(texture_);
        SDL_DestroyRenderer(renderer_);
        SDL_DestroyWindow(window_);
        gl_context_ = nullptr;
        texture_ = nullptr;
        renderer_ = nullptr;
        window_ = nullptr;
        SDL_Quit();
        return false;
    }
    
    window_open_ = true;
    std::cout << "Created SDL+OpenGL window: " << width << "x" << height << std::endl;
    std::cout << "OpenGL context created and activated" << std::endl;
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

void ImageOutput::process_pending_progressive_updates() {
#ifdef USE_SDL
    // Check if there's a pending progressive update and we have a window
    if (progressive_update_pending_.load() && window_open_) {
        update_window();
        progressive_update_pending_ = false;
    }
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
    if (gl_context_) {
        SDL_GL_DeleteContext(static_cast<SDL_GLContext>(gl_context_));
        gl_context_ = nullptr;
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

bool ImageOutput::make_context_current() {
#ifdef USE_SDL
    if (window_ && gl_context_) {
        if (SDL_GL_MakeCurrent(window_, static_cast<SDL_GLContext>(gl_context_)) == 0) {
            return true;
        } else {
            std::cerr << "Failed to make OpenGL context current: " << SDL_GetError() << std::endl;
            return false;
        }
    }
    return false;
#else
    return false;
#endif
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

ImageFormat ImageOutput::determine_format_from_extension(const std::string& filename) {
    size_t dot_pos = filename.find_last_of('.');
    if (dot_pos == std::string::npos) {
        return ImageFormat::PNG; // Default to PNG
    }
    
    std::string extension = filename.substr(dot_pos + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    if (extension == "png") {
        return ImageFormat::PNG;
    } else if (extension == "jpg" || extension == "jpeg") {
        return ImageFormat::JPEG;
    } else if (extension == "ppm") {
        return ImageFormat::PPM;
    } else {
        return ImageFormat::PNG; // Default to PNG for unknown extensions
    }
}

std::vector<uint8_t> ImageOutput::convert_to_rgb24() const {
    std::vector<uint8_t> rgb_data;
    rgb_data.reserve(width_ * height_ * 3);
    
    for (const auto& pixel : image_data_) {
        Color clamped_pixel = pixel.clamped();
        rgb_data.push_back(static_cast<uint8_t>(255 * clamped_pixel.r));
        rgb_data.push_back(static_cast<uint8_t>(255 * clamped_pixel.g));
        rgb_data.push_back(static_cast<uint8_t>(255 * clamped_pixel.b));
    }
    
    return rgb_data;
}

void ImageOutput::apply_gamma_correction(std::vector<uint8_t>& data) const {
    const float gamma = 2.2f;
    const float inv_gamma = 1.0f / gamma;
    
    for (auto& byte : data) {
        float normalized = byte / 255.0f;
        float corrected = std::pow(normalized, inv_gamma);
        byte = static_cast<uint8_t>(std::clamp(corrected * 255.0f, 0.0f, 255.0f));
    }
}

void ImageOutput::save_as_png(const std::string& filename, bool include_metadata) {
    // Simple PNG implementation without external libraries
    std::vector<uint8_t> rgb_data = convert_to_rgb24();
    
    // For now, fall back to PPM with a message about PNG support
    std::cout << "PNG support requires libpng. Saving as PPM instead: " << filename << std::endl;
    std::string ppm_filename = filename;
    size_t dot_pos = ppm_filename.find_last_of('.');
    if (dot_pos != std::string::npos) {
        ppm_filename = ppm_filename.substr(0, dot_pos) + ".ppm";
    } else {
        ppm_filename += ".ppm";
    }
    save_as_ppm(ppm_filename);
}

void ImageOutput::save_as_jpeg(const std::string& filename, int quality, bool include_metadata) {
    // Simple JPEG implementation without external libraries
    std::vector<uint8_t> rgb_data = convert_to_rgb24();
    
    // For now, fall back to PPM with a message about JPEG support
    std::cout << "JPEG support requires libjpeg. Saving as PPM instead: " << filename << std::endl;
    std::string ppm_filename = filename;
    size_t dot_pos = ppm_filename.find_last_of('.');
    if (dot_pos != std::string::npos) {
        ppm_filename = ppm_filename.substr(0, dot_pos) + ".ppm";
    } else {
        ppm_filename += ".ppm";
    }
    save_as_ppm(ppm_filename);
}

void ImageOutput::save_as_ppm(const std::string& filename) {
    // Check if filename is valid
    if (filename.empty()) {
        throw std::runtime_error("Filename cannot be empty");
    }
    
    // Check for invalid characters in filename
    const std::string invalid_chars = "<>:\"|?*";
    if (filename.find_first_of(invalid_chars) != std::string::npos) {
        throw std::runtime_error("Filename contains invalid characters");
    }
    
    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file for writing: " + filename + 
                                  " (check permissions and disk space)");
    }
    
    // Get current time for metadata
    auto now = std::time(nullptr);
    auto local_time = *std::localtime(&now);
    
    // PPM header with metadata
    file << "P3\n";
    file << "# Path Tracer Renderer v1.0.0\n";
    file << "# Generated on " << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S") << "\n";
    file << "# Resolution: " << width_ << "x" << height_ << "\n";
    file << "# Pixel count: " << (width_ * height_) << "\n";
    file << width_ << " " << height_ << "\n";
    file << "255\n";
    
    // Write pixel data
    for (const auto& pixel : image_data_) {
        Color clamped_pixel = pixel.clamped();
        int r = static_cast<int>(255 * clamped_pixel.r);
        int g = static_cast<int>(255 * clamped_pixel.g);
        int b = static_cast<int>(255 * clamped_pixel.b);
        
        file << r << " " << g << " " << b << "\n";
        
        // Check for write errors
        if (file.fail()) {
            file.close();
            throw std::runtime_error("Error writing to file: " + filename + 
                                      " (possibly out of disk space)");
        }
    }
    
    // Check final state and close
    if (!file.good()) {
        file.close();
        throw std::runtime_error("File write completed with errors: " + filename);
    }
    
    file.close();
    std::cout << "Image saved successfully: " << filename << std::endl;
}