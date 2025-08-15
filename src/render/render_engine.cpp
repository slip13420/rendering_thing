#include "render_engine.h"
#include "path_tracer.h"
#include "core/scene_manager.h"
#include "core/camera.h"
#include "image_output.h"
#include <iostream>

RenderEngine::RenderEngine() 
    : initialized_(false), render_width_(800), render_height_(600),
      render_state_(RenderState::IDLE), stop_requested_(false), progressive_mode_(false), manual_progressive_mode_(false) {
    initialize();
}

RenderEngine::~RenderEngine() {
    if (initialized_) {
        stop_render();
        shutdown();
    }
}

void RenderEngine::initialize() {
    // Create default components
    path_tracer_ = std::make_shared<PathTracer>();
    scene_manager_ = std::make_shared<SceneManager>();
    image_output_ = std::make_shared<ImageOutput>();
    
    // Initialize scene manager
    scene_manager_->initialize();
    
    // Configure path tracer with scene manager
    path_tracer_->set_scene_manager(scene_manager_);
    path_tracer_->set_max_depth(10);
    path_tracer_->set_samples_per_pixel(10);
    
    // Use camera from scene manager
    if (scene_manager_->get_camera()) {
        path_tracer_->set_camera(*scene_manager_->get_camera());
    }
    
    // Restore render state
    restore_render_state();
    
    // Initialize display with default size
    if (image_output_) {
        image_output_->initialize_display(render_width_, render_height_);
    }
    
    initialized_ = true;
    std::cout << "RenderEngine initialized with path tracing support" << std::endl;
}

void RenderEngine::render() {
    if (!initialized_) {
        std::cerr << "RenderEngine not initialized!" << std::endl;
        return;
    }
    
    std::cout << "Starting render (" << render_width_ << "x" << render_height_ << ")" << std::endl;
    
    // Execute path tracing
    path_tracer_->trace(render_width_, render_height_);
    
    // Get the rendered image data and pass it to image output
    const auto& image_data = path_tracer_->get_image_data();
    image_output_->set_image_data(image_data, render_width_, render_height_);
    
    std::cout << "Render completed successfully" << std::endl;
}

void RenderEngine::shutdown() {
    // Save render state before shutdown
    save_render_state();
    
    // Stop any ongoing render
    stop_render();
    
    if (scene_manager_) {
        scene_manager_->shutdown();
    }
    path_tracer_.reset();
    scene_manager_.reset();
    image_output_.reset();
    initialized_ = false;
    std::cout << "RenderEngine shutdown" << std::endl;
}

void RenderEngine::set_scene_manager(std::shared_ptr<SceneManager> scene_manager) {
    scene_manager_ = scene_manager;
    if (path_tracer_) {
        path_tracer_->set_scene_manager(scene_manager_);
    }
}

void RenderEngine::set_image_output(std::shared_ptr<ImageOutput> image_output) {
    image_output_ = image_output;
}

void RenderEngine::set_render_size(int width, int height) {
    render_width_ = width;
    render_height_ = height;
    
    // Update camera aspect ratio in scene manager's camera
    if (scene_manager_ && scene_manager_->get_camera()) {
        auto camera = scene_manager_->get_camera();
        camera->set_aspect_ratio(float(width) / float(height));
        if (path_tracer_) {
            path_tracer_->set_camera(*camera);
        }
    }
    
    // Note: Don't reinitialize display here as it can interfere with window sizing
}

void RenderEngine::set_max_depth(int depth) {
    if (path_tracer_) {
        path_tracer_->set_max_depth(depth);
    }
}

void RenderEngine::set_samples_per_pixel(int samples) {
    if (path_tracer_) {
        path_tracer_->set_samples_per_pixel(samples);
    }
}

void RenderEngine::set_camera_position(const Vector3& position, const Vector3& target, const Vector3& up) {
    if (scene_manager_) {
        // Update the camera in scene manager
        scene_manager_->set_camera_position(position);
        scene_manager_->set_camera_target(target);
        scene_manager_->set_camera_up(up);
        
        // Update path tracer with the modified camera
        if (path_tracer_ && scene_manager_->get_camera()) {
            path_tracer_->set_camera(*scene_manager_->get_camera());
        }
    }
}

void RenderEngine::save_image(const std::string& filename) {
    if (image_output_) {
        image_output_->save_to_file(filename);
    } else {
        std::cerr << "No image output component available" << std::endl;
    }
}

void RenderEngine::display_image() {
    if (image_output_) {
        image_output_->display_to_screen();
    } else {
        std::cerr << "No image output component available" << std::endl;
    }
}

void RenderEngine::update_camera_preview(const Vector3& camera_pos, const Vector3& camera_target) {
    if (!initialized_) return;
    
    // Throttle camera preview updates to avoid excessive rendering during rapid movement
    static auto last_preview_time = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_preview_time);
    
    // Only update preview every 100ms to maintain responsiveness without overwhelming
    if (elapsed.count() < 100 && is_progressive_rendering()) {
        return;
    }
    
    last_preview_time = now;
    
    // Don't interrupt manual progressive renders with camera preview
    if (manual_progressive_mode_) {
        std::cout << "Skipping camera preview - manual progressive render in progress" << std::endl;
        return;
    }
    
    // Stop any existing progressive render for camera preview (only non-manual renders)
    if (is_progressive_rendering()) {
        // Double-check manual mode before stopping (race condition protection)
        if (manual_progressive_mode_) {
            std::cout << "Skipping camera preview - manual progressive render detected" << std::endl;
            return;
        }
        
        stop_progressive_render();
        
        // Brief wait for cleanup
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    
    // Update camera position in the scene
    set_camera_position(camera_pos, camera_target);
    
    // Start a fast progressive render for real-time preview
    ProgressiveConfig preview_config;
    preview_config.initialSamples = 1;      // Start with 1 sample for immediate feedback
    preview_config.targetSamples = 16;      // Low target for responsive preview
    preview_config.progressiveSteps = 4;    // Few steps for speed
    preview_config.updateInterval = 0.08f;  // Very fast updates for real-time feel
    
    std::cout << "Starting camera preview progressive render..." << std::endl;
    
    // Force manual mode to false for camera preview, then start
    bool saved_manual_mode = manual_progressive_mode_;
    manual_progressive_mode_ = false;
    
    start_progressive_render(preview_config);
    
    // Restore previous manual mode if it was true (shouldn't happen, but safety)
    if (saved_manual_mode) {
        std::cout << "WARNING: Camera preview interrupted manual progressive render" << std::endl;
    }
}

void RenderEngine::start_render() {
    if (render_state_ == RenderState::RENDERING) {
        std::cerr << "Cannot start render: already in progress" << std::endl;
        return;
    }
    
    // Reset state to IDLE if in COMPLETED, STOPPED, or ERROR state
    if (render_state_ != RenderState::IDLE) {
        set_render_state(RenderState::IDLE);
    }
    
    if (!initialized_) {
        std::cerr << "RenderEngine not initialized!" << std::endl;
        set_render_state(RenderState::ERROR);
        return;
    }
    
    stop_requested_ = false;
    set_render_state(RenderState::RENDERING);
    
    if (render_thread_.joinable()) {
        render_thread_.join();
    }
    
    render_thread_ = std::thread(&RenderEngine::render_worker, this);
}

void RenderEngine::stop_render() {
    if (render_state_ != RenderState::RENDERING) {
        return;
    }
    
    stop_requested_ = true;
    
    // Signal PathTracer to stop
    if (path_tracer_) {
        path_tracer_->request_stop();
    }
    
    if (render_thread_.joinable()) {
        render_thread_.join();
    }
    
    set_render_state(RenderState::STOPPED);
}

void RenderEngine::start_progressive_render(const ProgressiveConfig& config) {
    if (render_state_ == RenderState::RENDERING) {
        std::cerr << "Cannot start progressive render: already in progress" << std::endl;
        return;
    }
    
    if (!initialized_) {
        std::cerr << "RenderEngine not initialized!" << std::endl;
        set_render_state(RenderState::ERROR);
        return;
    }
    
    stop_requested_ = false;
    progressive_mode_ = true;
    manual_progressive_mode_ = true;  // This is a manual progressive render
    set_render_state(RenderState::RENDERING);
    
    if (render_thread_.joinable()) {
        render_thread_.join();
    }
    
    render_thread_ = std::thread(&RenderEngine::progressive_render_worker, this, config);
    std::cout << "Progressive render started" << std::endl;
}

void RenderEngine::stop_progressive_render() {
    progressive_mode_ = false;
    stop_render(); // Reuse existing stop logic
}

bool RenderEngine::is_progressive_rendering() const {
    return progressive_mode_ && render_state_ == RenderState::RENDERING;
}

bool RenderEngine::is_rendering() const {
    return render_state_ == RenderState::RENDERING;
}

RenderState RenderEngine::get_render_state() const {
    return render_state_;
}

void RenderEngine::set_state_change_callback(std::function<void(RenderState)> callback) {
    state_change_callback_ = callback;
}

void RenderEngine::render_worker() {
    try {
        std::cout << "Starting render orchestration (" << render_width_ << "x" << render_height_ << ")" << std::endl;
        
        // Render pipeline initialization
        if (!validate_render_components()) {
            set_render_state(RenderState::ERROR);
            return;
        }
        
        // Coordinate scene data and camera setup
        synchronize_render_components();
        
        // Set up PathTracer cancellation
        path_tracer_->reset_stop_request();
        
        // Execute path tracing with interruption support
        std::cout << "Executing PathTracer with current scene configuration..." << std::endl;
        bool completed = path_tracer_->trace_interruptible(render_width_, render_height_);
        
        // Check if we were stopped during rendering
        if (stop_requested_ || !completed) {
            cleanup_partial_render();
            set_render_state(RenderState::STOPPED);
            return;
        }
        
        // Process render completion and result handling
        process_render_completion();
        
        set_render_state(RenderState::COMPLETED);
        std::cout << "Render orchestration completed successfully" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Render orchestration error: " << e.what() << std::endl;
        cleanup_partial_render();
        set_render_state(RenderState::ERROR);
    }
}

void RenderEngine::progressive_render_worker(const ProgressiveConfig& config) {
    try {
        std::cout << "Starting progressive render orchestration (" << render_width_ << "x" << render_height_ << ")" << std::endl;
        
        // Render pipeline initialization
        if (!validate_render_components()) {
            set_render_state(RenderState::ERROR);
            return;
        }
        
        // Coordinate scene data and camera setup
        synchronize_render_components();
        
        // Set up PathTracer cancellation
        path_tracer_->reset_stop_request();
        
        // Create progressive callback to update display in real-time
        auto progressive_callback = [this](const std::vector<Color>& data, int width, int height, int current_samples, int target_samples) {
            if (image_output_) {
                image_output_->update_progressive_display(data, width, height, current_samples, target_samples);
            }
            // Also update UI progress
            if (progress_callback_) {
                progress_callback_(width, height, current_samples, target_samples);
            }
        };
        
        // Execute progressive path tracing
        std::cout << "Executing progressive PathTracer with current scene configuration..." << std::endl;
        bool completed = path_tracer_->trace_progressive(render_width_, render_height_, config, progressive_callback);
        
        // Check if we were stopped during rendering
        if (stop_requested_ || !completed) {
            cleanup_partial_render();
            progressive_mode_ = false;
            manual_progressive_mode_ = false;
            set_render_state(RenderState::STOPPED);
            return;
        }
        
        // Process render completion and result handling
        process_render_completion();
        
        progressive_mode_ = false;
        manual_progressive_mode_ = false;
        set_render_state(RenderState::COMPLETED);
        std::cout << "Progressive render orchestration completed successfully" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Progressive render orchestration error: " << e.what() << std::endl;
        cleanup_partial_render();
        progressive_mode_ = false;
        manual_progressive_mode_ = false;
        set_render_state(RenderState::ERROR);
    }
}

void RenderEngine::set_render_state(RenderState state) {
    render_state_ = state;
    if (state_change_callback_) {
        state_change_callback_(state);
    }
}

void RenderEngine::save_render_state() {
    // For basic persistence, we'll reset to IDLE on restart
    // In a full implementation, this could save to file
    std::cout << "Render state saved (currently: " << static_cast<int>(render_state_.load()) << ")" << std::endl;
}

void RenderEngine::restore_render_state() {
    // On startup/recovery, always start in IDLE state
    set_render_state(RenderState::IDLE);
    stop_requested_ = false;
    std::cout << "Render state restored to IDLE" << std::endl;
}

bool RenderEngine::validate_render_components() {
    if (!path_tracer_) {
        std::cerr << "Render validation failed: No PathTracer available" << std::endl;
        return false;
    }
    
    if (!scene_manager_) {
        std::cerr << "Render validation failed: No SceneManager available" << std::endl;
        return false;
    }
    
    if (!image_output_) {
        std::cerr << "Render validation failed: No ImageOutput available" << std::endl;
        return false;
    }
    
    if (render_width_ <= 0 || render_height_ <= 0) {
        std::cerr << "Render validation failed: Invalid render dimensions" << std::endl;
        return false;
    }
    
    std::cout << "Render components validated successfully" << std::endl;
    return true;
}

void RenderEngine::synchronize_render_components() {
    // Ensure PathTracer has the latest scene data
    if (path_tracer_ && scene_manager_) {
        path_tracer_->set_scene_manager(scene_manager_);
        std::cout << "PathTracer synchronized with SceneManager" << std::endl;
    }
    
    // Ensure camera is synchronized
    if (scene_manager_ && scene_manager_->get_camera() && path_tracer_) {
        path_tracer_->set_camera(*scene_manager_->get_camera());
        std::cout << "Camera synchronized with PathTracer" << std::endl;
    }
    
    std::cout << "Render components synchronized" << std::endl;
}

void RenderEngine::process_render_completion() {
    // Get the rendered image data and pass it to image output
    const auto& image_data = path_tracer_->get_image_data();
    image_output_->set_image_data(image_data, render_width_, render_height_);
    
    std::cout << "Render output processed and connected to Image Output module" << std::endl;
}

void RenderEngine::cleanup_partial_render() {
    // Clean up any partial render state
    std::cout << "Cleaning up partial render state" << std::endl;
    
    // Preserve the current image data from PathTracer when stopped
    // This allows saving partial renders
    if (path_tracer_ && image_output_) {
        const auto& image_data = path_tracer_->get_image_data();
        if (!image_data.empty()) {
            image_output_->set_image_data(image_data, render_width_, render_height_);
            std::cout << "Partial render image data preserved for saving" << std::endl;
        }
    }
    
    // PathTracer automatically handles its own cleanup on cancellation
}