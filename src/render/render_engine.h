#pragma once

#include "core/common.h"
#include <memory>
#include <atomic>
#include <thread>
#include <functional>

// Forward declarations
class PathTracer;
class SceneManager;
class ImageOutput;
class Camera;
class GPUComputePipeline;
class GPUMemoryManager;
struct ProgressiveConfig;

enum class RenderState {
    IDLE,           // No render in progress
    RENDERING,      // Actively rendering
    COMPLETED,      // Render finished successfully
    STOPPED,        // Render cancelled by user
    ERROR           // Render failed due to error
};

enum class RenderMode {
    CPU_ONLY,
    GPU_ONLY,
    GPU_PREFERRED,
    AUTO
};

struct RenderMetrics {
    double cpuTime = 0.0;
    double gpuTime = 0.0;
    double speedupFactor = 0.0;
    int samplesPerPixel = 0;
    int imageWidth = 0;
    int imageHeight = 0;
    float cpu_utilization = 0.0f;
    float gpu_utilization = 0.0f;
    float memory_usage_mb = 0.0f;
    float render_time_ms = 0.0f;
    int samples_per_second = 0;
};

class RenderEngine {
public:
    RenderEngine();
    ~RenderEngine();
    
    void initialize();
    void render();
    void shutdown();
    
    // Render process control
    void start_render();
    void stop_render();
    bool is_rendering() const;
    RenderState get_render_state() const;
    
    // Progressive rendering
    void start_progressive_render(const ProgressiveConfig& config);
    void stop_progressive_render();
    bool is_progressive_rendering() const;
    void set_progress_callback(std::function<void(int, int, int, int)> callback) { progress_callback_ = callback; }
    
    // State change notifications
    void set_state_change_callback(std::function<void(RenderState)> callback);
    
    // State persistence and recovery
    void save_render_state();
    void restore_render_state();
    
    // Component management
    void set_scene_manager(std::shared_ptr<SceneManager> scene_manager);
    void set_image_output(std::shared_ptr<ImageOutput> image_output);
    
    // Rendering configuration
    void set_render_size(int width, int height);
    void set_max_depth(int depth);
    void set_samples_per_pixel(int samples);
    void set_camera_position(const Vector3& position, const Vector3& target, const Vector3& up = Vector3(0, 1, 0));
    
    // Output control
    void save_image(const std::string& filename);
    void display_image();
    void update_camera_preview(const Vector3& camera_pos, const Vector3& camera_target);
    
    // Camera movement handling
    void start_camera_movement();
    void stop_camera_movement();
    
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
    
    // Non-blocking progressive rendering
    bool start_progressive_gpu_non_blocking(const ProgressiveConfig& config);
    bool step_progressive_gpu(); // Returns true if more steps remain
    void cancel_progressive_gpu();
    bool is_progressive_gpu_active() const;
    
private:
    // Threading and state management
    void render_worker();
    void progressive_render_worker(const ProgressiveConfig& config);
    void set_render_state(RenderState state);
    
    // Render orchestration
    bool validate_render_components();
    void synchronize_render_components();
    void process_render_completion();
    void cleanup_partial_render();
    
    bool initialized_;
    std::shared_ptr<PathTracer> path_tracer_;
    std::shared_ptr<SceneManager> scene_manager_;
    std::shared_ptr<ImageOutput> image_output_;
    
    // GPU components (conditionally compiled)
#ifdef USE_GPU
    std::shared_ptr<GPUComputePipeline> gpu_pipeline_;
    std::shared_ptr<GPUMemoryManager> gpu_memory_;
#endif
    
    int render_width_;
    int render_height_;
    
    // Render state and threading
    std::atomic<RenderState> render_state_;
    std::atomic<bool> stop_requested_;
    std::atomic<bool> progressive_mode_;
    std::atomic<bool> manual_progressive_mode_;  // Track if this is a manual progressive render
    std::thread render_thread_;
    std::function<void(RenderState)> state_change_callback_;
    std::function<void(int, int, int, int)> progress_callback_;
    
    // GPU acceleration state
    RenderMode render_mode_;
    bool gpu_initialized_;
    RenderMetrics last_metrics_;
    
    // Camera movement state
    std::atomic<bool> camera_moving_;
    std::chrono::steady_clock::time_point last_camera_movement_;
    
    // Non-blocking progressive state
    struct ProgressiveGPUState {
        bool active = false;
        int current_step = 0;
        int current_samples = 0;
        int target_samples = 0;
        int sample_increment = 0;
        int total_steps = 0;
        float update_interval = 0.1f;
        std::chrono::steady_clock::time_point last_step_time;
        bool waiting_for_async_completion = false;  // Tracking pending async work
    } progressive_gpu_state_;
};