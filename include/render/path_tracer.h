#pragma once

#include "core/common.h"
#include <random>
#include <memory>

// Forward declarations
class SceneManager;
class Camera;

class PathTracer {
public:
    PathTracer();
    ~PathTracer();
    
    void trace(int width, int height);
    
    // Configuration
    void set_scene_manager(std::shared_ptr<SceneManager> scene_manager);
    void set_camera(const Camera& camera);
    void set_max_depth(int depth) { max_depth_ = depth; }
    void set_samples_per_pixel(int samples) { samples_per_pixel_ = samples; }
    
    // Get rendered data
    const std::vector<Color>& get_image_data() const { return image_data_; }
    
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
    
    mutable std::mt19937 rng_;
    mutable std::uniform_real_distribution<float> uniform_dist_;
};