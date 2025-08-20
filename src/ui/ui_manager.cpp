#include "ui/ui_manager.h"
#include "ui/ui_input.h"
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
    
    // Connect UI manager to input for primitive management (after initialization)
    // Note: We'll set this later to avoid shared_from_this() issues during construction
    
    std::cout << "UIManager initialized with camera controls and primitive management" << std::endl;
    initialized_ = true;
}

void UIManager::setup_primitive_management() {
    if (ui_input_) {
        ui_input_->set_ui_manager(shared_from_this());
        std::cout << "Primitive management connected to UI input" << std::endl;
    }
}

void UIManager::update() {
    if (ui_input_) {
        ui_input_->processEvents();
    }
}

void UIManager::render() {
    // Process any pending progressive display updates (must happen on main thread)
    if (image_output_) {
        image_output_->process_pending_progressive_updates();
    }
    
    // Display render controls
    render_status_display();
    render_start_button();
    render_stop_button();
    render_save_button();
    render_progressive_controls();
    render_primitive_controls();
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
        return;
    }
    
    bool enabled = (current_render_state_ == RenderState::IDLE || 
                    current_render_state_ == RenderState::COMPLETED ||
                    current_render_state_ == RenderState::STOPPED ||
                    current_render_state_ == RenderState::ERROR);
    
    // Start button logic without verbose output
}

void UIManager::render_stop_button() {
    if (!render_engine_) {
        return;
    }
    
    bool enabled = (current_render_state_ == RenderState::RENDERING);
    
    // Stop button logic without verbose output
}

void UIManager::render_status_display() {
    // Status display without verbose output
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
    
    // Progressive controls without verbose output
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
        return;
    }
    
    bool enabled = is_save_enabled();
    
    // Save button logic without verbose output
    
    // Save button behavior without verbose output
}

bool UIManager::is_save_enabled() const {
    // Allow saving if render completed OR if it was stopped (partial render still has image data)
    return ((current_render_state_ == RenderState::COMPLETED) || 
            (current_render_state_ == RenderState::STOPPED)) && 
           image_output_;
}

void UIManager::trigger_save_dialog() {
    if (!is_save_enabled()) {
        std::cout << "Save is not currently available" << std::endl;
        
        // Provide specific feedback about why save is not available
        if (!image_output_) {
            std::cout << "Reason: Image output not available" << std::endl;
        } else {
            std::cout << "Reason: No completed render to save" << std::endl;
            std::cout << "Current render state: ";
            switch (current_render_state_) {
                case RenderState::IDLE: std::cout << "IDLE (no render started)"; break;
                case RenderState::RENDERING: std::cout << "RENDERING (in progress)"; break;
                case RenderState::COMPLETED: std::cout << "COMPLETED"; break;
                case RenderState::STOPPED: std::cout << "STOPPED"; break;
                case RenderState::ERROR: std::cout << "ERROR"; break;
            }
            std::cout << std::endl;
            std::cout << "Try pressing 'V' after a render completes (G or M key)" << std::endl;
        }
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

// Primitive management methods
void UIManager::render_primitive_controls() {
    static bool show_primitive_menu = false;
    
    // Simple console-based controls for primitive management
    // In a real application, this would be replaced with GUI controls
}

void UIManager::trigger_add_primitive_menu() {
    show_add_primitive_menu();
}

void UIManager::show_primitive_list() {
    if (!scene_manager_) {
        std::cout << "No scene manager available" << std::endl;
        return;
    }
    
    std::cout << "\n=== Current Scene Objects ===" << std::endl;
    
    auto objects = scene_manager_->get_objects();
    if (objects.empty()) {
        std::cout << "No objects in scene" << std::endl;
    } else {
        std::cout << "Scene contains " << objects.size() << " objects:" << std::endl;
        for (size_t i = 0; i < objects.size(); ++i) {
            auto obj = objects[i];
            Vector3 pos = obj->position();
            Color col = obj->color();
            std::cout << "  " << i+1 << ". Object at (" 
                      << pos.x << ", " << pos.y << ", " << pos.z 
                      << ") Color(" << col.r << ", " << col.g << ", " << col.b << ")" << std::endl;
        }
    }
    std::cout << "=============================" << std::endl;
}

void UIManager::trigger_remove_primitive_menu() {
    show_remove_primitive_menu();
}

void UIManager::show_add_primitive_menu() {
    if (!scene_manager_) {
        std::cout << "No scene manager available" << std::endl;
        return;
    }
    
    std::cout << "\n=== Add Primitive ===" << std::endl;
    std::cout << "1. Sphere" << std::endl;
    std::cout << "2. Cube" << std::endl;
    std::cout << "3. Torus" << std::endl;
    std::cout << "4. Pyramid" << std::endl;
    std::cout << "0. Cancel" << std::endl;
    std::cout << "Enter choice (0-4): ";
    
    std::string choice;
    std::getline(std::cin, choice);
    
    if (choice.empty() || choice == "0") {
        std::cout << "Add primitive cancelled" << std::endl;
        return;
    }
    
    try {
        int type_choice = std::stoi(choice);
        if (type_choice >= 1 && type_choice <= 4) {
            add_primitive_by_type(type_choice);
        } else {
            std::cout << "Invalid choice. Please enter 1-4." << std::endl;
        }
    } catch (const std::exception&) {
        std::cout << "Invalid input. Please enter a number." << std::endl;
    }
    
    std::cout << "====================" << std::endl;
}

void UIManager::show_remove_primitive_menu() {
    if (!scene_manager_) {
        std::cout << "No scene manager available" << std::endl;
        return;
    }
    
    std::cout << "\n=== Remove Primitive ===" << std::endl;
    
    // First show the current objects
    show_primitive_list();
    
    auto objects = scene_manager_->get_objects();
    if (objects.empty()) {
        std::cout << "No objects to remove" << std::endl;
        return;
    }
    
    std::cout << "\nEnter object number to remove (1-" << objects.size() << "), 0 to cancel: ";
    
    std::string choice;
    std::getline(std::cin, choice);
    
    if (choice.empty() || choice == "0") {
        std::cout << "Remove primitive cancelled" << std::endl;
        return;
    }
    
    try {
        int obj_index = std::stoi(choice) - 1; // Convert to 0-based index
        if (obj_index >= 0 && obj_index < static_cast<int>(objects.size())) {
            auto obj_to_remove = objects[obj_index];
            scene_manager_->remove_object(obj_to_remove);
            std::cout << "Object " << (obj_index + 1) << " removed successfully!" << std::endl;
        } else {
            std::cout << "Invalid object number. Please enter 1-" << objects.size() << std::endl;
        }
    } catch (const std::exception&) {
        std::cout << "Invalid input. Please enter a number." << std::endl;
    }
    
    std::cout << "=======================" << std::endl;
}

void UIManager::add_primitive_by_type(int type_choice) {
    if (!scene_manager_) {
        return;
    }
    
    // Get position input
    std::cout << "Enter position (x y z, default: 0 0 -1): ";
    std::string pos_input;
    std::getline(std::cin, pos_input);
    
    Vector3 position(0, 0, -1); // default position
    if (!pos_input.empty()) {
        std::istringstream iss(pos_input);
        float x, y, z;
        if (iss >> x >> y >> z) {
            position = Vector3(x, y, z);
        }
    }
    
    // Get color input
    std::cout << "Enter color (r g b, range 0-1, default: 0.7 0.3 0.3): ";
    std::string color_input;
    std::getline(std::cin, color_input);
    
    Color color(0.7f, 0.3f, 0.3f); // default color
    if (!color_input.empty()) {
        std::istringstream iss(color_input);
        float r, g, b;
        if (iss >> r >> g >> b) {
            color = Color(
                std::clamp(r, 0.0f, 1.0f),
                std::clamp(g, 0.0f, 1.0f),
                std::clamp(b, 0.0f, 1.0f)
            );
        }
    }
    
    Material material(color, 0.5f, 0.0f, 0.0f); // default material
    
    PrimitiveType prim_type;
    switch (type_choice) {
        case 1: prim_type = static_cast<PrimitiveType>(1); break; // SPHERE
        case 2: prim_type = static_cast<PrimitiveType>(2); break; // CUBE  
        case 3: prim_type = static_cast<PrimitiveType>(3); break; // TORUS
        case 4: prim_type = static_cast<PrimitiveType>(4); break; // PYRAMID
        default:
            std::cout << "Invalid primitive type" << std::endl;
            return;
    }
    
    PrimitiveID id = scene_manager_->addPrimitive(prim_type, position, color, material);
    if (id != 0) { // INVALID_PRIMITIVE_ID is 0
        std::cout << "Added " << get_primitive_type_name(type_choice) 
                  << " with ID " << id << " at position (" 
                  << position.x << ", " << position.y << ", " << position.z << ")" << std::endl;
    } else {
        std::cout << "Failed to add " << get_primitive_type_name(type_choice) << std::endl;
    }
}

void UIManager::remove_primitive_by_id() {
    // This method would be used for ID-based removal
    // Currently using index-based removal in show_remove_primitive_menu
}

std::string UIManager::get_primitive_type_name(int type) const {
    switch (type) {
        case 1: return "Sphere";
        case 2: return "Cube";  
        case 3: return "Torus";
        case 4: return "Pyramid";
        default: return "Unknown";
    }
}