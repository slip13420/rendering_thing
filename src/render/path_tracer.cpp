#include "path_tracer.h"
#include "core/scene_manager.h"
#include "core/camera.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <cmath>

// PathTracer implementation
PathTracer::PathTracer() 
    : camera_(Vector3(0, 0, 0), Vector3(0, 0, -1), Vector3(0, 1, 0)),
      max_depth_(10), samples_per_pixel_(10), stop_requested_(false),
      rng_(std::random_device{}()), uniform_dist_(0.0f, 1.0f) {
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
    trace_interruptible(width, height);
}

bool PathTracer::trace_interruptible(int width, int height) {
    image_data_.clear();
    image_data_.resize(width * height);
    
    reset_stop_request();
    
    std::cout << "Starting path tracing (" << width << "x" << height << ", " 
              << samples_per_pixel_ << " samples per pixel)" << std::endl;
    
    auto start_time = std::chrono::steady_clock::now();
    
    for (int j = height - 1; j >= 0; --j) {
        // Check for cancellation every few scanlines
        if (j % 5 == 0) {
            if (stop_requested_) {
                std::cout << "Path tracing cancelled at scanline " << j << std::endl;
                return false;
            }
            
            if (j % 10 == 0) {
                std::cout << "Scanlines remaining: " << j << std::endl;
            }
        }
        
        for (int i = 0; i < width; ++i) {
            Color pixel_color(0, 0, 0);
            
            for (int s = 0; s < samples_per_pixel_; ++s) {
                // Check for cancellation during expensive inner loop
                if (stop_requested_) {
                    std::cout << "Path tracing cancelled during sampling" << std::endl;
                    return false;
                }
                
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
    
    if (stop_requested_) {
        std::cout << "Path tracing cancelled near completion" << std::endl;
        return false;
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    std::cout << "Path tracing completed in " << duration.count() << " ms" << std::endl;
    return true;
}

bool PathTracer::trace_progressive(int width, int height, const ProgressiveConfig& config, ProgressiveCallback callback) {
    image_data_.clear();
    image_data_.resize(width * height);
    
    // Initialize accumulated samples buffer
    std::vector<Color> accumulated_samples(width * height, Color(0, 0, 0));
    
    reset_stop_request();
    
    std::cout << "Starting progressive path tracing (" << width << "x" << height << ")" << std::endl;
    std::cout << "Progressive config: " << config.initialSamples << " -> " << config.targetSamples 
              << " samples, " << config.progressiveSteps << " steps, " 
              << config.updateInterval << "s intervals" << std::endl;
    
    auto start_time = std::chrono::steady_clock::now();
    auto last_update_time = start_time;
    
    // Calculate sample counts for each progressive step
    std::vector<int> step_samples;
    step_samples.push_back(config.initialSamples);
    
    for (int step = 1; step < config.progressiveSteps; ++step) {
        float progress = float(step) / float(config.progressiveSteps - 1);
        float log_initial = std::log(float(config.initialSamples));
        float log_target = std::log(float(config.targetSamples));
        int samples = int(std::exp(log_initial + progress * (log_target - log_initial)));
        step_samples.push_back(samples);
    }
    
    // Debug: Print sample progression
    std::cout << "Progressive sample plan: ";
    for (size_t i = 0; i < step_samples.size(); ++i) {
        std::cout << step_samples[i];
        if (i < step_samples.size() - 1) std::cout << " -> ";
    }
    std::cout << std::endl;
    
    int total_samples_rendered = 0;
    
    for (int step = 0; step < config.progressiveSteps; ++step) {
        if (stop_requested_) {
            std::cout << "Progressive rendering cancelled at step " << step << " due to stop request" << std::endl;
            return false;
        }
        
        int samples_this_step = step_samples[step];
        if (step > 0) {
            samples_this_step -= step_samples[step - 1];
        }
        
        std::cout << "Progressive step " << (step + 1) << "/" << config.progressiveSteps 
                  << ": rendering " << samples_this_step << " additional samples" << std::endl;
        
        // Render additional samples for this step
        for (int j = height - 1; j >= 0; --j) {
            if (stop_requested_) {
                std::cout << "Progressive rendering cancelled during step " << step << std::endl;
                return false;
            }
            
            for (int i = 0; i < width; ++i) {
                Color pixel_samples(0, 0, 0);
                
                for (int s = 0; s < samples_this_step; ++s) {
                    if (stop_requested_) {
                        std::cout << "Progressive rendering cancelled during sampling" << std::endl;
                        return false;
                    }
                    
                    float u = (float(i) + uniform_dist_(rng_)) / float(width);
                    float v = (float(j) + uniform_dist_(rng_)) / float(height);
                    
                    Ray ray = camera_.get_ray(u, v);
                    pixel_samples += ray_color(ray, max_depth_);
                }
                
                // Accumulate samples
                int pixel_index = (height - 1 - j) * width + i;
                accumulated_samples[pixel_index] += pixel_samples;
            }
        }
        
        total_samples_rendered = step_samples[step];
        
        // Generate intermediate result by averaging accumulated samples
        for (int i = 0; i < width * height; ++i) {
            Color averaged = accumulated_samples[i] * (1.0f / float(total_samples_rendered));
            
            // Gamma correction (gamma=2.0)
            averaged.r = std::sqrt(averaged.r);
            averaged.g = std::sqrt(averaged.g);
            averaged.b = std::sqrt(averaged.b);
            
            image_data_[i] = averaged.clamped();
        }
        
        // Call callback with intermediate result
        if (callback) {
            std::cout << "Calling callback with " << total_samples_rendered << "/" << config.targetSamples << " samples" << std::endl;
            callback(image_data_, width, height, total_samples_rendered, config.targetSamples);
        }
        
        // Wait for update interval (except on last step) with adaptive timing
        if (step < config.progressiveSteps - 1) {
            auto current_time = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration<float>(current_time - last_update_time).count();
            
            // Adaptive update interval: reduce frequency for very small images or early steps
            float adaptive_interval = config.updateInterval;
            if (width * height < 100000) { // For images smaller than 100K pixels
                adaptive_interval *= 0.5f; // Update more frequently
            }
            if (step < config.progressiveSteps / 4) { // For early steps
                adaptive_interval *= 0.75f; // Update more frequently when changes are most visible
            }
            
            if (elapsed < adaptive_interval) {
                float sleep_time = adaptive_interval - elapsed;
                std::this_thread::sleep_for(std::chrono::duration<float>(sleep_time));
            }
            
            last_update_time = std::chrono::steady_clock::now();
        }
    }
    
    if (stop_requested_) {
        std::cout << "Progressive rendering cancelled near completion" << std::endl;
        return false;
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    std::cout << "Progressive path tracing completed in " << duration.count() << " ms" << std::endl;
    return true;
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