#pragma once

#include "core/common.h"
#include <memory>

// Forward declarations
class PathTracer;
class SceneManager;
class ImageOutput;

class RenderEngine {
public:
    RenderEngine();
    ~RenderEngine();
    
    void initialize();
    void render();
    void shutdown();
    
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
    
private:
    bool initialized_;
    int render_width_;
    int render_height_;
    
    std::shared_ptr<PathTracer> path_tracer_;
    std::shared_ptr<SceneManager> scene_manager_;
    std::shared_ptr<ImageOutput> image_output_;
};