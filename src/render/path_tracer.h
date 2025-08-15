#pragma once

#include "core/common.h"
#include "core/primitives.h"
#include "core/camera.h"
#include <vector>
#include <memory>
#include <atomic>
#include <functional>

// Forward declarations
class SceneManager;

// Progressive rendering configuration
struct ProgressiveConfig {
    int initialSamples = 1;      // Quick preview sample count
    int targetSamples = 2000;    // Final quality sample count
    int progressiveSteps = 15;   // Number of progressive improvement passes
    float updateInterval = 0.5f; // Seconds between progressive updates
};

// Progressive rendering callback for intermediate results
using ProgressiveCallback = std::function<void(const std::vector<Color>&, int, int, int, int)>;

class PathTracer {
public:
    PathTracer();
    ~PathTracer();
    
    void trace(int width, int height);
    bool trace_interruptible(int width, int height);
    
    // Progressive rendering
    bool trace_progressive(int width, int height, const ProgressiveConfig& config, ProgressiveCallback callback);
    
    void set_scene_manager(std::shared_ptr<SceneManager> scene_manager);
    void set_camera(const Camera& camera);
    void set_max_depth(int depth) { max_depth_ = depth; }
    void set_samples_per_pixel(int samples) { samples_per_pixel_ = samples; }
    
    // Cancellation control
    void request_stop() { stop_requested_ = true; }
    void reset_stop_request() { stop_requested_ = false; }
    bool is_stop_requested() const { return stop_requested_; }
    
    // Get the rendered image data
    const std::vector<Color>& get_image_data() const noexcept { return image_data_; }
    
    // Get rendering configuration
    int get_max_depth() const noexcept { return max_depth_; }
    int get_samples_per_pixel() const noexcept { return samples_per_pixel_; }
    
private:
    Color ray_color(const Ray& ray, int depth) const;
    Vector3 random_in_unit_sphere() const;
    Vector3 random_unit_vector() const;
    Vector3 random_in_hemisphere(const Vector3& normal) const;
    Vector3 reflect(const Vector3& v, const Vector3& n) const;
    bool near_zero(const Vector3& v) const;
    
    std::shared_ptr<SceneManager> scene_manager_;
    Camera camera_;
    std::vector<Color> image_data_;
    int max_depth_;
    int samples_per_pixel_;
    std::atomic<bool> stop_requested_;
    mutable std::mt19937 rng_;
    mutable std::uniform_real_distribution<float> uniform_dist_;
};