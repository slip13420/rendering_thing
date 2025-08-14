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

enum class RenderState {
    IDLE,           // No render in progress
    RENDERING,      // Actively rendering
    COMPLETED,      // Render finished successfully
    STOPPED,        // Render cancelled by user
    ERROR           // Render failed due to error
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
    
private:
    // Threading and state management
    void render_worker();
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
    
    int render_width_;
    int render_height_;
    
    // Render state and threading
    std::atomic<RenderState> render_state_;
    std::atomic<bool> stop_requested_;
    std::thread render_thread_;
    std::function<void(RenderState)> state_change_callback_;
};