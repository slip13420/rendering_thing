#include "ui_manager.h"
#include "ui_input.h"
#include "core/scene_manager.h"
#include "render/render_engine.h"
#include "render/image_output.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <algorithm>
#include <stdexcept>

UIManager::UIManager() : initialized_(false), current_render_state_(RenderState::IDLE), show_progress_details_(false) {
    
}

UIManager::~UIManager() {
    if (initialized_) {
        shutdown();
    }
}

void UIManager::initialize() {
    ui_input_ = std::make_shared<UIInput>();
    
    // Set up input dependencies
    if (scene_manager_) {
        ui_input_->set_scene_manager(scene_manager_);
    }
    if (render_engine_) {
        ui_input_->set_render_engine(render_engine_);
    }
    
    std::cout << "UIManager initialized with camera controls" << std::endl;
    initialized_ = true;
}

void UIManager::update() {
    if (ui_input_) {
        ui_input_->processEvents();
    }
}

void UIManager::render() {
    // Display render controls
    render_status_display();
    render_start_button();
    render_stop_button();
    render_save_button();
    render_progressive_controls();
    render_progress_display();
}

void UIManager::shutdown() {
    ui_input_.reset();
    std::cout << "UIManager shutdown" << std::endl;
    initialized_ = false;
}

void UIManager::set_scene_manager(std::shared_ptr<SceneManager> scene_manager) {
    scene_manager_ = scene_manager;
    if (ui_input_) {
        ui_input_->set_scene_manager(scene_manager);
    }
}

void UIManager::set_render_engine(std::shared_ptr<RenderEngine> render_engine) {
    render_engine_ = render_engine;
    if (ui_input_) {
        ui_input_->set_render_engine(render_engine);
    }
    
    // Set up state change callback
    if (render_engine_) {
        render_engine_->set_state_change_callback(
            [this](RenderState state) { on_render_state_changed(state); });
        current_render_state_ = render_engine_->get_render_state();
    }
}

void UIManager::set_image_output(std::shared_ptr<ImageOutput> image_output) {
    image_output_ = image_output;
    std::cout << "UIManager: Image output dependency set" << std::endl;
}

bool UIManager::should_quit() const {
    return ui_input_ ? ui_input_->shouldQuit() : false;
}

void UIManager::process_input() {
    if (ui_input_) {
        ui_input_->processEvents();
    }
}

void UIManager::render_start_button() {
    if (!render_engine_) {
        std::cout << "[Start Render Button: DISABLED - No render engine]" << std::endl;
        return;
    }
    
    bool enabled = (current_render_state_ == RenderState::IDLE || 
                    current_render_state_ == RenderState::COMPLETED ||
                    current_render_state_ == RenderState::STOPPED ||
                    current_render_state_ == RenderState::ERROR);
    
    std::cout << "[Start Render Button: " << (enabled ? "ENABLED" : "DISABLED") << "]" << std::endl;
    
    if (enabled) {
        std::cout << "  Click to start rendering" << std::endl;
    }
}

void UIManager::render_stop_button() {
    if (!render_engine_) {
        std::cout << "[Stop Render Button: DISABLED - No render engine]" << std::endl;
        return;
    }
    
    bool enabled = (current_render_state_ == RenderState::RENDERING);
    
    std::cout << "[Stop Render Button: " << (enabled ? "ENABLED" : "DISABLED") << "]" << std::endl;
    
    if (enabled) {
        std::cout << "  Click to stop rendering" << std::endl;
    }
}

void UIManager::render_status_display() {
    std::string status_text = get_render_state_text(current_render_state_);
    std::cout << "[Render Status: " << status_text << "]" << std::endl;
}

std::string UIManager::get_render_state_text(RenderState state) const {
    switch (state) {
        case RenderState::IDLE:      return "Ready to render";
        case RenderState::RENDERING: return "Rendering in progress...";
        case RenderState::COMPLETED: return "Render completed successfully";
        case RenderState::STOPPED:   return "Render stopped by user";
        case RenderState::ERROR:     return "Render failed with error";
        default:                     return "Unknown state";
    }
}

void UIManager::on_render_state_changed(RenderState state) {
    current_render_state_ = state;
    std::cout << "Render state changed to: " << get_render_state_text(state) << std::endl;
    
    // Reset progress when starting new render
    if (state == RenderState::RENDERING) {
        reset_progress();
    }
}

void UIManager::update_progress(int width, int height, int current_samples, int target_samples) {
    auto now = std::chrono::steady_clock::now();
    
    // Initialize timing on first update
    if (progress_data_.current_samples == 0) {
        progress_data_.start_time = now;
        progress_data_.last_update_time = now;
    }
    
    progress_data_.width = width;
    progress_data_.height = height;
    progress_data_.current_samples = current_samples;
    progress_data_.target_samples = target_samples;
    
    // Calculate progress percentage
    if (target_samples > 0) {
        progress_data_.progress_percentage = (100.0f * current_samples) / target_samples;
    }
    
    // Calculate samples per second
    auto elapsed = std::chrono::duration<float>(now - progress_data_.start_time).count();
    if (elapsed > 0.1f) { // Avoid division by very small numbers
        progress_data_.samples_per_second = current_samples / elapsed;
    }
    
    // Calculate estimated time remaining
    if (progress_data_.samples_per_second > 0) {
        int remaining_samples = target_samples - current_samples;
        progress_data_.estimated_time_remaining = remaining_samples / progress_data_.samples_per_second;
    }
    
    progress_data_.last_update_time = now;
    show_progress_details_ = true;
}

void UIManager::reset_progress() {
    progress_data_ = ProgressData{};
    show_progress_details_ = false;
}

void UIManager::render_progress_display() {
    if (!show_progress_details_ || current_render_state_ != RenderState::RENDERING) {
        return;
    }
    
    std::cout << "\n=== Progressive Rendering Status ===" << std::endl;
    std::cout << std::fixed << std::setprecision(1);
    
    // Progress bar
    int bar_width = 40;
    int filled = int(progress_data_.progress_percentage * bar_width / 100.0f);
    std::cout << "[";
    for (int i = 0; i < bar_width; ++i) {
        std::cout << (i < filled ? "█" : "░");
    }
    std::cout << "] " << progress_data_.progress_percentage << "%" << std::endl;
    
    // Sample counts
    std::cout << "Samples: " << progress_data_.current_samples 
              << " / " << progress_data_.target_samples << std::endl;
    
    // Resolution
    std::cout << "Resolution: " << progress_data_.width 
              << "x" << progress_data_.height << std::endl;
    
    // Performance metrics
    std::cout << "Speed: " << progress_data_.samples_per_second 
              << " samples/sec" << std::endl;
    
    // Time estimation
    std::cout << "ETA: " << format_time(progress_data_.estimated_time_remaining) << std::endl;
    
    std::cout << "====================================" << std::endl;
}

void UIManager::render_progressive_controls() {
    if (!render_engine_) return;
    
    bool is_progressive = render_engine_->is_progressive_rendering();
    
    std::cout << "[Progressive Rendering: " << (is_progressive ? "ACTIVE" : "INACTIVE") << "]" << std::endl;
    
    if (current_render_state_ == RenderState::IDLE || 
        current_render_state_ == RenderState::COMPLETED ||
        current_render_state_ == RenderState::STOPPED) {
        std::cout << "  Available: Start Progressive Render" << std::endl;
    }
}

std::string UIManager::format_time(float seconds) const {
    if (seconds < 60) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << seconds << "s";
        return oss.str();
    } else if (seconds < 3600) {
        int minutes = int(seconds / 60);
        int secs = int(seconds) % 60;
        std::ostringstream oss;
        oss << minutes << "m " << secs << "s";
        return oss.str();
    } else {
        int hours = int(seconds / 3600);
        int minutes = int(seconds / 60) % 60;
        std::ostringstream oss;
        oss << hours << "h " << minutes << "m";
        return oss.str();
    }
}

void UIManager::render_save_button() {
    if (!image_output_) {
        std::cout << "[Save Button: DISABLED - No image output]" << std::endl;
        return;
    }
    
    bool enabled = is_save_enabled();
    
    std::cout << "[Save Button: " << (enabled ? "ENABLED" : "DISABLED") << "]" << std::endl;
    
    if (enabled) {
        std::cout << "  Click to save rendered image" << std::endl;
    } else if (current_render_state_ == RenderState::RENDERING) {
        std::cout << "  Save will be available after render completes" << std::endl;
    }
}

bool UIManager::is_save_enabled() const {
    return (current_render_state_ == RenderState::COMPLETED) && image_output_;
}

void UIManager::trigger_save_dialog() {
    if (!is_save_enabled()) {
        std::cout << "Save is not currently available" << std::endl;
        return;
    }
    
    show_save_dialog();
}

void UIManager::show_save_dialog() {
    std::cout << "\n=== Save Rendered Image ===" << std::endl;
    
    // Get default filename
    std::string default_filename = get_default_filename();
    
    std::cout << "Enter filename (default: " << default_filename << "): ";
    std::string user_filename;
    std::getline(std::cin, user_filename);
    
    if (user_filename.empty()) {
        user_filename = default_filename;
    }
    
    // Validate filename
    if (user_filename.find_first_of("<>:\"|?*") != std::string::npos) {
        std::cout << "Error: Filename contains invalid characters (<>:\"|?*)" << std::endl;
        std::cout << "Please try again with a valid filename." << std::endl;
        return;
    }
    
    // Get format choice
    std::cout << "\nSelect image format:" << std::endl;
    std::cout << "1. PNG (lossless, recommended)" << std::endl;
    std::cout << "2. JPEG (lossy, smaller file)" << std::endl;
    std::cout << "3. PPM (uncompressed)" << std::endl;
    std::cout << "Enter choice (1-3, default: 1): ";
    
    std::string choice;
    std::getline(std::cin, choice);
    
    ImageFormat format = ImageFormat::PNG; // default
    int quality = 90; // default JPEG quality
    
    if (choice == "2") {
        format = ImageFormat::JPEG;
        std::cout << "Enter JPEG quality (1-100, default: 90): ";
        std::string quality_str;
        std::getline(std::cin, quality_str);
        if (!quality_str.empty()) {
            try {
                quality = std::stoi(quality_str);
                quality = std::max(1, std::min(100, quality));
            } catch (const std::exception&) {
                quality = 90;
            }
        }
    } else if (choice == "3") {
        format = ImageFormat::PPM;
    }
    
    // Ensure correct file extension
    std::string final_filename = user_filename;
    std::string expected_ext;
    
    switch (format) {
        case ImageFormat::PNG:
            expected_ext = ".png";
            break;
        case ImageFormat::JPEG:
            expected_ext = ".jpg";
            break;
        case ImageFormat::PPM:
            expected_ext = ".ppm";
            break;
    }
    
    // Add extension if not present
    size_t dot_pos = final_filename.find_last_of('.');
    if (dot_pos == std::string::npos || 
        final_filename.substr(dot_pos) != expected_ext) {
        final_filename += expected_ext;
    }
    
    // Perform save
    std::cout << "Saving image..." << std::endl;
    
    try {
        bool success = save_image_with_options(final_filename, format, quality);
        
        if (success) {
            std::cout << "SUCCESS: Image saved to " << final_filename << std::endl;
        } else {
            std::cout << "FAILED: Could not save image to " << final_filename << std::endl;
            std::cout << "Please check file permissions and available disk space." << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "ERROR: " << e.what() << std::endl;
        std::cout << "Save operation cancelled." << std::endl;
    }
    
    std::cout << "=========================" << std::endl;
}

bool UIManager::save_image_with_options(const std::string& filename, ImageFormat format, int quality) {
    if (!image_output_) {
        std::cerr << "No image output available for saving" << std::endl;
        return false;
    }
    
    return image_output_->save_with_format(filename, format, quality);
}

std::string UIManager::get_default_filename() const {
    // Generate filename with timestamp
    auto now = std::time(nullptr);
    auto local_time = *std::localtime(&now);
    
    std::ostringstream oss;
    oss << "render_" 
        << std::setfill('0') << std::setw(4) << (local_time.tm_year + 1900)
        << "-" << std::setw(2) << (local_time.tm_mon + 1)
        << "-" << std::setw(2) << local_time.tm_mday
        << "_" << std::setw(2) << local_time.tm_hour
        << "-" << std::setw(2) << local_time.tm_min
        << "-" << std::setw(2) << local_time.tm_sec;
    
    return oss.str();
}