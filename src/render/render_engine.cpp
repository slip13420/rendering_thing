#include "render_engine.h"
#include "render/path_tracer.h"
#include "core/scene_manager.h"
#include "core/camera.h"
#include "image_output.h"
#include <iostream>
#include <thread>
#include <chrono>

#ifdef USE_GPU
#include "gpu_compute.h"
#include "gpu_memory.h"
#include <SDL.h>
#endif

RenderEngine::RenderEngine() 
    : initialized_(false), render_width_(1920), render_height_(1080),
      render_state_(RenderState::IDLE), stop_requested_(false), progressive_mode_(false), manual_progressive_mode_(false),
      render_mode_(RenderMode::AUTO), gpu_initialized_(false), camera_moving_(false) {
    initialize();
}

RenderEngine::~RenderEngine() {
    if (initialized_) {
        stop_render();
        cleanup_gpu();
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
    path_tracer_->set_max_depth(5);  // Reduced depth for faster live operations
    path_tracer_->set_samples_per_pixel(1);  // Single sample for responsive live operations
    
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
    
    // Initialize GPU acceleration if available - AFTER display initialization
    // to ensure OpenGL context is available
    if (initialize_gpu()) {
        std::cout << "GPU acceleration initialized successfully" << std::endl;
    } else {
        std::cout << "GPU acceleration unavailable, falling back to CPU" << std::endl;
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
    
    // Execute path tracing with GPU/CPU hybrid mode
#ifdef USE_GPU
    if (gpu_initialized_ && path_tracer_->isGPUAvailable()) {
        path_tracer_->trace_hybrid(render_width_, render_height_, PathTracer::RenderMode::HYBRID_AUTO);
    } else {
        path_tracer_->trace(render_width_, render_height_);
    }
#else
    path_tracer_->trace(render_width_, render_height_);
#endif
    
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
    
    // Connect GPU memory manager if available
#ifdef USE_GPU
    if (gpu_initialized_ && gpu_memory_ && scene_manager_) {
        scene_manager_->setGPUMemoryManager(gpu_memory_);
    }
#endif
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
    
    // Throttle camera preview updates for better responsiveness
    static auto last_preview_time = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_preview_time).count();
    
    // Only update preview every 50ms for responsive camera movement
    if (elapsed < 50) {
        // Still update camera position without rendering
        set_camera_position(camera_pos, camera_target);
        return;
    }
    last_preview_time = now;
    
    // Signal that camera is moving
    start_camera_movement();
    
    // Update camera position in the scene
    set_camera_position(camera_pos, camera_target);
    
    // Use GPU for fast camera preview when available (main thread context)
    if (path_tracer_) {
        // Use reasonable settings for good camera preview quality
        path_tracer_->set_samples_per_pixel(2); // 2 samples for better quality
        path_tracer_->set_max_depth(2);         // Some bounces for proper scene rendering
        
        bool success = false;
        
        // Try GPU first since camera movement is in main thread with OpenGL context
        if (gpu_initialized_ && path_tracer_->isGPUAvailable()) {
            success = path_tracer_->trace_gpu(render_width_, render_height_);
            if (success) {
                std::cout << "Camera preview: GPU render completed" << std::endl;
            }
        }
        
        // Fallback to CPU if GPU failed
        if (!success) {
            // Use half resolution for CPU fallback
            int preview_width = render_width_ / 2;
            int preview_height = render_height_ / 2;
            success = path_tracer_->trace_interruptible(preview_width, preview_height);
            if (success) {
                std::cout << "Camera preview: CPU render completed at " << preview_width << "x" << preview_height << std::endl;
            }
        }
        
        if (success && image_output_) {
            // Update display immediately
            const auto& image_data = path_tracer_->get_image_data();
            // Use full resolution for GPU, or preview size for CPU
            int display_width = (gpu_initialized_ && path_tracer_->isGPUAvailable()) ? render_width_ : render_width_ / 2;
            int display_height = (gpu_initialized_ && path_tracer_->isGPUAvailable()) ? render_height_ : render_height_ / 2;
            image_output_->set_image_data(image_data, display_width, display_height);
            image_output_->display_to_screen();
        }
    }
}

void RenderEngine::start_camera_movement() {
    // Stop any existing progressive rendering
    if (is_progressive_rendering() && !manual_progressive_mode_) {
        // Stopping progressive rendering due to camera movement
        stop_progressive_render();
    }
    
    camera_moving_ = true;
    last_camera_movement_ = std::chrono::steady_clock::now();
}

void RenderEngine::stop_camera_movement() {
    camera_moving_ = false;
    last_camera_movement_ = std::chrono::steady_clock::now();
    
    // Auto-progressive rendering disabled - user can manually trigger with 'M' key
    // This was causing unwanted CPU progressive renders after camera movement
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
    
    // If GPU is available, use main thread GPU progressive for live updates
    if (gpu_initialized_ && path_tracer_ && path_tracer_->isGPUAvailable()) {
        std::cout << "Using GPU progressive rendering in main thread for live updates..." << std::endl;
        
        bool success = start_progressive_gpu_main_thread(config);
        progressive_mode_ = false;
        manual_progressive_mode_ = false;
        
        if (success) {
            set_render_state(RenderState::COMPLETED);
            std::cout << "GPU progressive render completed successfully" << std::endl;
        } else {
            std::cout << "GPU progressive render failed, falling back to CPU worker thread..." << std::endl;
            // Fall through to worker thread CPU rendering
        }
    }
    
    // Use worker thread for CPU progressive rendering (if GPU failed or not available)
    if (render_state_ == RenderState::RENDERING) {
        if (render_thread_.joinable()) {
            render_thread_.join();
        }
        
        render_thread_ = std::thread(&RenderEngine::progressive_render_worker, this, config);
        std::cout << "Progressive render started in worker thread (CPU)" << std::endl;
    }
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
        
        // GPU buffers should be pre-allocated in main thread
        // Skip context management since no new allocations needed
#ifdef USE_GPU
        if (gpu_initialized_) {
            std::cout << "Using pre-allocated GPU buffers for rendering..." << std::endl;
        }
#endif
        
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
        
        // CRITICAL: GPU operations must happen in main thread where OpenGL context is current
        // Worker threads cannot access OpenGL context, so we force CPU rendering here
#ifdef USE_GPU
        bool completed;
        if (gpu_initialized_ && path_tracer_->isGPUAvailable()) {
            std::cout << "GPU available but worker thread cannot access OpenGL context" << std::endl;
            std::cout << "Using CPU rendering in worker thread" << std::endl;
            completed = path_tracer_->trace_interruptible(render_width_, render_height_);
        } else {
            completed = path_tracer_->trace_interruptible(render_width_, render_height_);
        }
#else
        bool completed = path_tracer_->trace_interruptible(render_width_, render_height_);
#endif
        
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
        
        // GPU buffers should be pre-allocated in main thread
        // Skip context management since no new allocations needed
#ifdef USE_GPU
        if (gpu_initialized_) {
            std::cout << "Using pre-allocated GPU buffers for progressive rendering..." << std::endl;
        }
#endif
        
        // Render pipeline initialization
        if (!validate_render_components()) {
            set_render_state(RenderState::ERROR);
            return;
        }
        
        // Coordinate scene data and camera setup
        synchronize_render_components();
        
        // Set up PathTracer cancellation
        path_tracer_->reset_stop_request();
        
        // Create progressive callback to update display in real-time with GPU memory coordination
        auto progressive_callback = [this](const std::vector<Color>& data, int width, int height, int current_samples, int target_samples) {
            // Coordinate GPU memory updates with progressive rendering timing
#ifdef USE_GPU
            if (gpu_initialized_ && gpu_memory_ && scene_manager_) {
                // Check if GPU memory optimization is needed during progressive render
                auto gpu_stats = gpu_memory_->getMemoryStats();
                
                // Only optimize memory pools if fragmentation is high and we're not at peak rendering
                if (gpu_stats.fragmentation_ratio > 0.3f && current_samples % 4 == 0) {
                    auto opt_start = std::chrono::high_resolution_clock::now();
                    gpu_memory_->optimizeMemoryPools();
                    auto opt_end = std::chrono::high_resolution_clock::now();
                    
                    auto opt_time = std::chrono::duration<double, std::milli>(opt_end - opt_start).count();
                    
                    // Only log if optimization takes significant time (>1ms)
                    if (opt_time > 1.0) {
                        std::cout << "GPU memory optimized during progressive render: " 
                                  << opt_time << "ms" << std::endl;
                    }
                }
                
                // Transfer current progressive image data to GPU if needed for compute operations
                if (current_samples % 8 == 0) { // Every 8th sample to avoid overhead
                    auto image_buffer = gpu_memory_->allocateImageBuffer(width, height);
                    if (image_buffer) {
                        gpu_memory_->transferImageData(image_buffer, data);
                    }
                }
            }
#endif
            
            if (image_output_) {
                image_output_->update_progressive_display(data, width, height, current_samples, target_samples);
            }
            // Also update UI progress
            if (progress_callback_) {
                progress_callback_(width, height, current_samples, target_samples);
            }
        };
        
        // Execute progressive path tracing with GPU support
        std::cout << "Executing progressive PathTracer with current scene configuration..." << std::endl;
#ifdef USE_GPU
        bool completed;
        if (gpu_initialized_ && path_tracer_->isGPUAvailable()) {
            std::cout << "Attempting GPU-accelerated progressive rendering..." << std::endl;
            
            // GPU operations must run in main thread, so we do progressive GPU renders step by step
            std::cout << "GPU progressive rendering requires main thread context - using CPU fallback" << std::endl;
            std::cout << "Note: Use 'U' key for single-shot main thread GPU rendering" << std::endl;
            
            // Fallback to CPU progressive rendering
            std::cout << "Using CPU progressive rendering" << std::endl;
            completed = path_tracer_->trace_progressive(render_width_, render_height_, config, progressive_callback);
        } else {
            std::cout << "Using CPU progressive rendering" << std::endl;
            completed = path_tracer_->trace_progressive(render_width_, render_height_, config, progressive_callback);
        }
#else
        bool completed = path_tracer_->trace_progressive(render_width_, render_height_, config, progressive_callback);
#endif
        
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
    
    // Synchronize scene data to GPU if GPU acceleration is available
#ifdef USE_GPU
    if (gpu_initialized_ && gpu_memory_ && scene_manager_) {
        if (!scene_manager_->isGPUSynced()) {
            auto start_time = std::chrono::high_resolution_clock::now();
            scene_manager_->syncSceneToGPU();
            auto end_time = std::chrono::high_resolution_clock::now();
            
            auto sync_time = std::chrono::duration<double, std::milli>(end_time - start_time).count();
            std::cout << "GPU scene synchronization completed in " << sync_time << "ms" << std::endl;
        } else {
            std::cout << "Scene already synchronized with GPU" << std::endl;
        }
    }
#endif
    
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

// GPU acceleration methods
void RenderEngine::set_render_mode(RenderMode mode) {
    render_mode_ = mode;
    
    if (initialized_) {
        // Reinitialize GPU if mode changed to GPU
        if (mode == RenderMode::GPU_PREFERRED || mode == RenderMode::GPU_ONLY || mode == RenderMode::AUTO) {
            if (!gpu_initialized_) {
                initialize_gpu();
            }
        } else if (mode == RenderMode::CPU_ONLY) {
            cleanup_gpu();
        }
    }
}

RenderMode RenderEngine::get_render_mode() const {
    return render_mode_;
}

bool RenderEngine::is_gpu_available() const {
    return gpu_initialized_;
}

bool RenderEngine::initialize_gpu() {
#ifdef USE_GPU
    if (gpu_initialized_) {
        return true;
    }
    
    try {
        // Create GPU compute pipeline
        gpu_pipeline_ = std::make_shared<GPUComputePipeline>();
        if (!gpu_pipeline_->initialize()) {
            std::cerr << "Failed to initialize GPU compute pipeline: " << gpu_pipeline_->getErrorMessage() << std::endl;
            gpu_pipeline_.reset();
            return false;
        }
        
        // Create GPU memory manager
        gpu_memory_ = std::make_shared<GPUMemoryManager>();
        gpu_memory_->enableProfiling(true); // Enable debug output
        if (!gpu_memory_->initialize()) {
            std::cerr << "Failed to initialize GPU memory manager: " << gpu_memory_->getErrorMessage() << std::endl;
            gpu_memory_.reset();
            gpu_pipeline_.reset();
            return false;
        }
        
        // Connect GPU memory manager to scene manager for coordination
        if (scene_manager_) {
            scene_manager_->setGPUMemoryManager(gpu_memory_);
            std::cout << "GPU memory manager connected to scene manager" << std::endl;
        }
        
        // Initialize PathTracer GPU components
        if (path_tracer_) {
            std::cout << "Initializing PathTracer GPU components..." << std::endl;
            if (!path_tracer_->initializeGPU()) {
                std::cerr << "Failed to initialize PathTracer GPU components" << std::endl;
                cleanup_gpu();
                return false;
            }
            std::cout << "PathTracer GPU components initialized successfully" << std::endl;
        }
        
        gpu_initialized_ = true;
        std::cout << "GPU acceleration initialized: " << gpu_pipeline_->getDriverInfo() << std::endl;
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "GPU initialization failed with exception: " << e.what() << std::endl;
        cleanup_gpu();
        return false;
    }
#else
    std::cout << "GPU support not compiled in (USE_GPU not defined)" << std::endl;
    return false;
#endif
}

void RenderEngine::cleanup_gpu() {
#ifdef USE_GPU
    if (gpu_memory_) {
        gpu_memory_->cleanup();
        gpu_memory_.reset();
    }
    
    if (gpu_pipeline_) {
        gpu_pipeline_->cleanup();
        gpu_pipeline_.reset();
    }
    
    gpu_initialized_ = false;
    std::cout << "GPU resources cleaned up" << std::endl;
#endif
}

RenderMetrics RenderEngine::get_render_metrics() const {
    RenderMetrics metrics;
    
#ifdef USE_GPU
    if (gpu_initialized_ && gpu_memory_) {
        auto gpu_stats = gpu_memory_->getMemoryStats();
        metrics.memory_usage_mb = static_cast<float>(gpu_stats.total_used) / (1024.0f * 1024.0f);
        metrics.gpu_utilization = gpu_stats.fragmentation_ratio < 0.5f ? 75.0f : 50.0f; // Simple heuristic
    }
#endif
    
    // CPU utilization and other metrics would be calculated based on actual rendering
    metrics.cpu_utilization = render_state_ == RenderState::RENDERING ? 80.0f : 10.0f;
    
    return metrics;
}

bool RenderEngine::render_gpu_main_thread() {
#ifdef USE_GPU
    if (!gpu_initialized_ || !path_tracer_ || !path_tracer_->isGPUAvailable()) {
        std::cerr << "GPU not available for main thread rendering" << std::endl;
        return false;
    }
    
    std::cout << "=== GPU RENDERING IN MAIN THREAD ===" << std::endl;
    
    // Ensure OpenGL context is current by asking image_output to make it current
    if (image_output_ && !image_output_->make_context_current()) {
        std::cerr << "ERROR: Failed to make OpenGL context current" << std::endl;
        return false;
    }
    
    // Validate that we now have an OpenGL context
    SDL_GLContext currentContext = SDL_GL_GetCurrentContext();
    if (!currentContext) {
        std::cerr << "ERROR: No OpenGL context in main thread after making current" << std::endl;
        return false;
    }
    
    std::cout << "Main thread has OpenGL context: " << currentContext << std::endl;
    
    // CRITICAL: Ensure we're using the SAME context where resources were created
    // This completely solves the multi-context issue by forcing single-context operation
    
    // Capture the current context as the GPU context to ensure consistency
    std::cout << "Capturing current context for GPU operations..." << std::endl;
    path_tracer_->captureOpenGLContext();
    
    // Validate render components
    if (!validate_render_components()) {
        std::cerr << "Render components validation failed" << std::endl;
        return false;
    }
    
    // Synchronize components
    synchronize_render_components();
    
    // Reset stop request
    path_tracer_->reset_stop_request();
    
    // Perform GPU rendering - resources and operations all in same context now
    std::cout << "Executing single-context GPU path tracing in main thread..." << std::endl;
    bool success = path_tracer_->trace_gpu(render_width_, render_height_);
    
    if (success) {
        // Process completion
        process_render_completion();
        std::cout << "GPU rendering completed successfully in main thread" << std::endl;
    } else {
        std::cerr << "GPU rendering failed in main thread" << std::endl;
    }
    
    return success;
#else
    std::cerr << "GPU support not compiled in" << std::endl;
    return false;
#endif
}

bool RenderEngine::start_progressive_gpu_main_thread(const ProgressiveConfig& config) {
#ifdef USE_GPU
    // Deprecated: Use start_progressive_gpu_non_blocking for responsive UI
    std::cout << "Warning: start_progressive_gpu_main_thread blocks UI. Use start_progressive_gpu_non_blocking instead." << std::endl;
    return start_progressive_gpu_non_blocking(config);
#else
    std::cerr << "GPU support not compiled in" << std::endl;
    return false;
#endif
}

bool RenderEngine::start_progressive_gpu_non_blocking(const ProgressiveConfig& config) {
#ifdef USE_GPU
    if (!gpu_initialized_ || !path_tracer_ || !path_tracer_->isGPUAvailable()) {
        std::cerr << "GPU not available for progressive rendering" << std::endl;
        return false;
    }
    
    if (progressive_gpu_state_.active) {
        std::cerr << "Progressive GPU rendering already active" << std::endl;
        return false;
    }
    
    std::cout << "=== NON-BLOCKING GPU PROGRESSIVE RENDERING ===" << std::endl;
    
    // Validate that we have an OpenGL context
    SDL_GLContext currentContext = SDL_GL_GetCurrentContext();
    if (!currentContext) {
        std::cerr << "ERROR: No OpenGL context in main thread" << std::endl;
        return false;
    }
    
    // Validate render components
    if (!validate_render_components()) {
        std::cerr << "Render components validation failed" << std::endl;
        return false;
    }
    
    // Synchronize components
    synchronize_render_components();
    
    // Reset stop request
    path_tracer_->reset_stop_request();
    path_tracer_->captureOpenGLContext();
    
    // Initialize progressive state
    progressive_gpu_state_.active = true;
    progressive_gpu_state_.current_step = 0;
    progressive_gpu_state_.current_samples = config.initialSamples;
    progressive_gpu_state_.target_samples = config.targetSamples;
    progressive_gpu_state_.total_steps = config.progressiveSteps;
    progressive_gpu_state_.update_interval = config.updateInterval;
    progressive_gpu_state_.sample_increment = (config.targetSamples - config.initialSamples) / config.progressiveSteps;
    if (progressive_gpu_state_.sample_increment < 1) progressive_gpu_state_.sample_increment = 1;
    progressive_gpu_state_.last_step_time = std::chrono::steady_clock::now();
    
    progressive_mode_ = true;
    manual_progressive_mode_ = true;
    set_render_state(RenderState::RENDERING);
    
    std::cout << "Progressive GPU rendering initialized (non-blocking mode)" << std::endl;
    return true;
    
#else
    std::cerr << "GPU support not compiled in" << std::endl;
    return false;
#endif
}

bool RenderEngine::step_progressive_gpu() {
#ifdef USE_GPU
    if (!progressive_gpu_state_.active) {
        return false; // No progressive rendering active
    }
    
    // First, check if we have pending async work to finalize
    if (progressive_gpu_state_.waiting_for_async_completion) {
        if (path_tracer_->is_gpu_complete()) {
            // Async work completed - finalize and display
            bool success = path_tracer_->finalize_gpu_result(render_width_, render_height_);
            progressive_gpu_state_.waiting_for_async_completion = false;
            
            if (success) {
                // Process and display the result
                process_render_completion();
                
                if (image_output_) {
                    const auto& image_data = path_tracer_->get_image_data();
                    image_output_->update_progressive_display(
                        image_data, render_width_, render_height_, 
                        progressive_gpu_state_.current_samples, progressive_gpu_state_.target_samples
                    );
                    image_output_->display_to_screen();
                }
                
                // Update progress callback
                if (progress_callback_) {
                    progress_callback_(render_width_, render_height_, 
                                      progressive_gpu_state_.current_samples, progressive_gpu_state_.target_samples);
                }
                
                // Prepare for next step
                progressive_gpu_state_.current_step++;
                progressive_gpu_state_.current_samples += progressive_gpu_state_.sample_increment;
                if (progressive_gpu_state_.current_samples > progressive_gpu_state_.target_samples) {
                    progressive_gpu_state_.current_samples = progressive_gpu_state_.target_samples;
                }
                progressive_gpu_state_.last_step_time = std::chrono::steady_clock::now();
            } else {
                std::cerr << "GPU async operation failed" << std::endl;
                cancel_progressive_gpu();
                return false;
            }
        } else {
            // Still waiting for async completion - no new work this frame
            return true;
        }
    }
    
    // Check if we've completed all steps
    if (progressive_gpu_state_.current_step >= progressive_gpu_state_.total_steps || 
        progressive_gpu_state_.current_samples >= progressive_gpu_state_.target_samples) {
        
        // Progressive rendering complete
        progressive_gpu_state_.active = false;
        progressive_mode_ = false;
        manual_progressive_mode_ = false;
        set_render_state(RenderState::COMPLETED);
        std::cout << "Progressive GPU rendering completed" << std::endl;
        return false; // No more steps
    }
    
    // Check if we should delay the next step based on update interval
    auto now = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration<float, std::milli>(now - progressive_gpu_state_.last_step_time).count();
    float required_interval_ms = progressive_gpu_state_.update_interval * 1000.0f;
    
    if (elapsed_ms < required_interval_ms) {
        return true; // More steps remain, but not ready yet
    }
    
    // Start next progressive step
    std::cout << "Progressive GPU step " << (progressive_gpu_state_.current_step + 1) 
              << "/" << progressive_gpu_state_.total_steps 
              << " (samples: " << progressive_gpu_state_.current_samples << ")" << std::endl;
    
    // Set sample count for this step
    path_tracer_->set_samples_per_pixel(progressive_gpu_state_.current_samples);
    
    // Start async GPU work (non-blocking)
    if (path_tracer_->start_gpu_async(render_width_, render_height_)) {
        progressive_gpu_state_.waiting_for_async_completion = true;
        std::cout << "GPU async work started for step " << (progressive_gpu_state_.current_step + 1) << std::endl;
    } else {
        std::cerr << "Failed to start async GPU work" << std::endl;
        cancel_progressive_gpu();
        return false;
    }
    
    return true; // More steps remain
    
#else
    return false;
#endif
}

void RenderEngine::cancel_progressive_gpu() {
    if (progressive_gpu_state_.active) {
        progressive_gpu_state_.active = false;
        progressive_gpu_state_.waiting_for_async_completion = false;
        progressive_mode_ = false;
        manual_progressive_mode_ = false;
        set_render_state(RenderState::STOPPED);
        std::cout << "Progressive GPU rendering cancelled" << std::endl;
    }
}

bool RenderEngine::is_progressive_gpu_active() const {
    return progressive_gpu_state_.active;
}