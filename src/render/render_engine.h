#pragma once

#include "core/common.h"
#include <memory>

// Forward declarations
class PathTracer;
class SceneManager;
class ImageOutput;
class Camera;

class RenderEngine {
public:
    RenderEngine();
    ~RenderEngine();
    
    void initialize();
    void render();
    void shutdown();
    
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
    
private:
    bool initialized_;
    std::shared_ptr<PathTracer> path_tracer_;
    std::shared_ptr<SceneManager> scene_manager_;
    std::shared_ptr<ImageOutput> image_output_;
    
    int render_width_;
    int render_height_;
};