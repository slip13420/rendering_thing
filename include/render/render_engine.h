#pragma once

#include "core/common.h"
#include <memory>
#include <thread>
#include <atomic>
#include <functional>

// Forward declarations
class PathTracer;
class SceneManager;
class ImageOutput;
class GPUComputePipeline;
class GPUMemoryManager;
struct ProgressiveConfig;

// Render states for background rendering
enum class RenderState {
    IDLE,
    RENDERING,
    COMPLETED,
    STOPPED,
    ERROR
};

class RenderEngine {
public:
    RenderEngine();
    ~RenderEngine();
    
    void initialize();
    void render();
    void shutdown();
    
    // Background rendering controls
    void start_render();
    void stop_render();
    bool is_rendering() const;
    RenderState get_render_state() const;
    void set_state_change_callback(std::function<void(RenderState)> callback);
    
    // Progressive rendering
    void start_progressive_render(const ProgressiveConfig& config);
    void stop_progressive_render();
    bool is_progressive_rendering() const;
    void set_progress_callback(std::function<void(int, int, int, int)> callback) { progress_callback_ = callback; }
    
    // Configuration
    void set_scene_manager(std::shared_ptr<SceneManager> scene_manager);
    void set_image_output(std::shared_ptr<ImageOutput> image_output);
    void set_render_size(int width, int height);
    void set_max_depth(int depth);
    void set_samples_per_pixel(int samples);
    void set_camera_position(const Vector3& position, const Vector3& target, const Vector3& up);
    
    // Output
    void save_image(const std::string& filename);
    void display_image();
    
    // Camera preview support
    void update_camera_preview(const Vector3& camera_pos, const Vector3& camera_target);
    
    // GPU acceleration
    void set_render_mode(RenderMode mode);
    RenderMode get_render_mode() const;
    bool is_gpu_available() const;
    bool initialize_gpu();
    void cleanup_gpu();
    RenderMetrics get_render_metrics() const;
    
    // Main thread GPU rendering (requires active OpenGL context)
    bool render_gpu_main_thread();
    bool start_progressive_gpu_main_thread(const ProgressiveConfig& config);
    
private:
    void render_worker();
    void progressive_render_worker(const ProgressiveConfig& config);
    void set_render_state(RenderState state);
    void save_render_state();
    void restore_render_state();
    bool validate_render_components();
    void synchronize_render_components();
    void process_render_completion();
    void cleanup_partial_render();
    
    bool initialized_;
    int render_width_;
    int render_height_;
    
    std::shared_ptr<PathTracer> path_tracer_;
    std::shared_ptr<SceneManager> scene_manager_;
    std::shared_ptr<ImageOutput> image_output_;
    
    // GPU components (conditionally compiled)
#ifdef USE_GPU
    std::shared_ptr<GPUComputePipeline> gpu_pipeline_;
    std::shared_ptr<GPUMemoryManager> gpu_memory_;
#endif
    
    // Threading and state management
    std::thread render_thread_;
    std::atomic<RenderState> render_state_;
    std::atomic<bool> stop_requested_;
    std::atomic<bool> progressive_mode_;
    std::function<void(RenderState)> state_change_callback_;
    std::function<void(int, int, int, int)> progress_callback_;
    
    // GPU acceleration state
    RenderMode render_mode_;
    bool gpu_initialized_;
    RenderMetrics last_metrics_;
};