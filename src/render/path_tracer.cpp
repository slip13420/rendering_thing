#include "path_tracer.h"
#include "core/scene_manager.h"
#include <iostream>
#include <chrono>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Camera implementation
Camera::Camera(const Vector3& position, const Vector3& target, const Vector3& up,
               float fov, float aspect_ratio) : position_(position) {
    float theta = fov * M_PI / 180.0f;
    float half_height = std::tan(theta / 2.0f);
    float half_width = aspect_ratio * half_height;
    
    Vector3 w = (position - target).normalized();
    Vector3 u = up.cross(w).normalized();
    Vector3 v = w.cross(u);
    
    lower_left_corner_ = position - half_width * u - half_height * v - w;
    horizontal_ = u * (2.0f * half_width);
    vertical_ = v * (2.0f * half_height);
}

Ray Camera::get_ray(float u, float v) const {
    Vector3 direction = lower_left_corner_ + horizontal_ * u + vertical_ * v - position_;
    return Ray(position_, direction);
}

// PathTracer implementation
PathTracer::PathTracer() 
    : max_depth_(10), samples_per_pixel_(10), rng_(std::random_device{}()), uniform_dist_(0.0f, 1.0f) {
}

PathTracer::~PathTracer() {
}

void PathTracer::set_scene_manager(std::shared_ptr<SceneManager> scene_manager) {
    scene_manager_ = scene_manager;
}

void PathTracer::set_camera(const Camera& camera) {
    camera_ = camera;
}

void PathTracer::trace(int width, int height) {
    image_data_.clear();
    image_data_.resize(width * height);
    
    std::cout << "Starting path tracing (" << width << "x" << height << ", " 
              << samples_per_pixel_ << " samples per pixel)" << std::endl;
    
    auto start_time = std::chrono::steady_clock::now();
    
    for (int j = height - 1; j >= 0; --j) {
        if (j % 10 == 0) {
            std::cout << "Scanlines remaining: " << j << std::endl;
        }
        
        for (int i = 0; i < width; ++i) {
            Color pixel_color(0, 0, 0);
            
            for (int s = 0; s < samples_per_pixel_; ++s) {
                float u = (float(i) + uniform_dist_(rng_)) / float(width);
                float v = (float(j) + uniform_dist_(rng_)) / float(height);
                
                Ray ray = camera_.get_ray(u, v);
                pixel_color += ray_color(ray, max_depth_);
            }
            
            // Average the samples and gamma correct
            float scale = 1.0f / float(samples_per_pixel_);
            pixel_color *= scale;
            
            // Gamma correction (gamma=2.0)
            pixel_color.r = std::sqrt(pixel_color.r);
            pixel_color.g = std::sqrt(pixel_color.g);
            pixel_color.b = std::sqrt(pixel_color.b);
            
            image_data_[(height - 1 - j) * width + i] = pixel_color.clamped();
        }
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    std::cout << "Path tracing completed in " << duration.count() << " ms" << std::endl;
}


Color PathTracer::ray_color(const Ray& ray, int depth) const {
    if (depth <= 0) {
        return Color::black();
    }
    
    if (!scene_manager_) {
        // No scene manager, return background
        Vector3 unit_direction = ray.direction.normalized();
        float t = 0.5f * (unit_direction.y + 1.0f);
        return Color(1.0f, 1.0f, 1.0f) * (1.0f - t) + Color(0.5f, 0.7f, 1.0f) * t;
    }
    
    HitRecord rec;
    if (scene_manager_->hit_scene(ray, 0.001f, std::numeric_limits<float>::infinity(), rec)) {
        // Handle emissive materials
        if (rec.material.is_emissive()) {
            return rec.material.albedo * rec.material.emission;
        }
        
        // Diffuse reflection with some randomness
        Vector3 target;
        if (rec.material.roughness > 0.5f) {
            // More diffuse
            target = rec.point + rec.normal + random_unit_vector();
        } else {
            // More reflective
            Vector3 reflected = reflect(ray.direction, rec.normal);
            target = rec.point + reflected + random_in_unit_sphere() * rec.material.roughness;
        }
        
        Ray scattered(rec.point, target - rec.point);
        return rec.material.albedo * ray_color(scattered, depth - 1);
    }
    
    // Use scene manager's background color
    return scene_manager_->get_background_color(ray);
}

Vector3 PathTracer::random_in_unit_sphere() const {
    Vector3 p;
    do {
        p = Vector3(uniform_dist_(rng_), uniform_dist_(rng_), uniform_dist_(rng_)) * 2.0f - Vector3(1, 1, 1);
    } while (p.dot(p) >= 1.0f);
    return p;
}

Vector3 PathTracer::random_unit_vector() const {
    return random_in_unit_sphere().normalized();
}

Vector3 PathTracer::random_in_hemisphere(const Vector3& normal) const {
    Vector3 in_unit_sphere = random_in_unit_sphere();
    if (in_unit_sphere.dot(normal) > 0.0f) {
        return in_unit_sphere;
    } else {
        return in_unit_sphere * -1.0f;
    }
}

Vector3 PathTracer::reflect(const Vector3& v, const Vector3& n) const {
    return v - n * (2.0f * v.dot(n));
}

bool PathTracer::near_zero(const Vector3& v) const {
    const float s = 1e-8f;
    return (std::abs(v.x) < s) && (std::abs(v.y) < s) && (std::abs(v.z) < s);
}