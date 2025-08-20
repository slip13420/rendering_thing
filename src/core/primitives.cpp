#include "primitives.h"
#include <vector>
#include <limits>

void Sphere::update() {
    
}

bool Sphere::hit(const Ray& ray, float t_min, float t_max, HitRecord& rec) const {
    Vector3 oc = ray.origin - position_;
    float a = ray.direction.length_squared();  // More efficient than dot product with self
    float half_b = oc.dot(ray.direction);
    float c = oc.length_squared() - radius_ * radius_;  // More efficient than dot product with self
    float discriminant = half_b * half_b - a * c;
    
    if (discriminant < 0) return false;
    
    float sqrtd = std::sqrt(discriminant);
    
    // Find the nearest root that lies in the acceptable range
    float root = (-half_b - sqrtd) / a;
    if (root < t_min || t_max < root) {
        root = (-half_b + sqrtd) / a;
        if (root < t_min || t_max < root) {
            return false;
        }
    }
    
    rec.t = root;
    rec.point = ray.at(rec.t);
    Vector3 outward_normal = (rec.point - position_) * (1.0f / radius_);
    rec.set_face_normal(ray, outward_normal);
    rec.material = material_;
    
    return true;
}

void Cube::update() {
    
}

bool Cube::hit(const Ray& ray, float t_min, float t_max, HitRecord& rec) const {
    const float half_size = size_ * 0.5f;
    const Vector3 min_bound = position_ + Vector3(-half_size, -half_size, -half_size);
    const Vector3 max_bound = position_ + Vector3(half_size, half_size, half_size);
    
    // Ray-AABB intersection using slab method - improved version
    float t_near = t_min;
    float t_far = t_max;
    Vector3 normal_candidate(0, 0, 0);
    
    // Use array access for cleaner code
    const float ray_origin[3] = {ray.origin.x, ray.origin.y, ray.origin.z};
    const float ray_dir[3] = {ray.direction.x, ray.direction.y, ray.direction.z};
    const float bounds_min[3] = {min_bound.x, min_bound.y, min_bound.z};
    const float bounds_max[3] = {max_bound.x, max_bound.y, max_bound.z};
    
    for (int i = 0; i < 3; ++i) {
        if (std::abs(ray_dir[i]) < 1e-6f) {
            // Ray is parallel to the slab
            if (ray_origin[i] < bounds_min[i] || ray_origin[i] > bounds_max[i]) {
                return false;
            }
        } else {
            float t1 = (bounds_min[i] - ray_origin[i]) / ray_dir[i];
            float t2 = (bounds_max[i] - ray_origin[i]) / ray_dir[i];
            
            if (t1 > t2) std::swap(t1, t2);
            
            if (t1 > t_near) {
                t_near = t1;
                normal_candidate = Vector3(0, 0, 0);
                // Set normal component based on which face was hit
                if (i == 0) normal_candidate.x = (ray_dir[i] > 0) ? -1.0f : 1.0f;
                else if (i == 1) normal_candidate.y = (ray_dir[i] > 0) ? -1.0f : 1.0f;
                else normal_candidate.z = (ray_dir[i] > 0) ? -1.0f : 1.0f;
            }
            
            if (t2 < t_far) t_far = t2;
            
            if (t_near > t_far) return false;
        }
    }
    
    if (t_near > t_max || t_far < t_min) return false;
    
    float t_hit = (t_near > t_min) ? t_near : t_far;
    if (t_hit < t_min || t_hit > t_max) return false;
    
    rec.t = t_hit;
    rec.point = ray.at(rec.t);
    rec.set_face_normal(ray, normal_candidate);
    rec.material = material_;
    
    return true;
}

void Torus::update() {
    
}

bool Torus::hit(const Ray& ray, float t_min, float t_max, HitRecord& rec) const {
    // Simplified torus intersection using iterative approach
    // Torus equation: (sqrt(x²+z²) - R)² + y² = r²
    
    Vector3 oc = ray.origin - position_;
    Vector3 rd = ray.direction.normalized();
    
    float R = major_radius_;
    float r = minor_radius_;
    
    float best_t = std::numeric_limits<float>::max();
    Vector3 best_normal;
    
    // Fine sampling to find torus intersection
    const int samples = 100; // More samples for better accuracy
    float t_start = std::max(t_min, 0.01f);
    float t_end = std::min(t_max, 15.0f);
    float dt = (t_end - t_start) / samples;
    
    for (int i = 0; i < samples; i++) {
        float t = t_start + i * dt;
        Vector3 p = oc + rd * t; // Point in torus local space
        
        // Distance from Y axis (torus center line)
        float dist_from_axis = std::sqrt(p.x * p.x + p.z * p.z);
        
        // Distance from torus center ring
        float ring_dist = std::abs(dist_from_axis - R);
        
        // Distance from torus surface
        float surface_dist = std::sqrt(ring_dist * ring_dist + p.y * p.y);
        
        // Check if we're close enough to torus surface
        if (surface_dist <= r * 0.1f && t < best_t && t >= t_min) { // Tight tolerance
            best_t = t;
            
            // Calculate normal: point from ring center to surface point
            if (dist_from_axis > 1e-6f) {
                Vector3 ring_center = Vector3(p.x, 0, p.z).normalized() * R;
                Vector3 to_surface = p - ring_center;
                best_normal = to_surface.normalized();
            } else {
                // Fallback for degenerate case
                best_normal = Vector3(0, p.y > 0 ? 1 : -1, 0);
            }
        }
    }
    
    if (best_t < std::numeric_limits<float>::max()) {
        rec.t = best_t;
        rec.point = ray.at(rec.t);
        rec.set_face_normal(ray, best_normal);
        rec.material = material_;
        
        
        return true;
    }
    
    return false;
}

void Pyramid::update() {
    
}

bool Pyramid::hit(const Ray& ray, float t_min, float t_max, HitRecord& rec) const {
    
    // Square pyramid with base centered at position and apex pointing up
    float half_base = base_size_ * 0.5f;
    Vector3 base_center = position_;
    Vector3 apex = position_ + Vector3(0, height_, 0);
    
    float closest_t = std::numeric_limits<float>::max();
    Vector3 closest_normal;
    
    // Check intersection with base (square at y = position.y)
    if (std::abs(ray.direction.y) > 1e-6f) {
        float t = (base_center.y - ray.origin.y) / ray.direction.y;
        if (t >= t_min && t <= t_max) {
            Vector3 p = ray.at(t);
            if (std::abs(p.x - position_.x) <= half_base && std::abs(p.z - position_.z) <= half_base) {
                if (t < closest_t) {
                    closest_t = t;
                    closest_normal = Vector3(0, -1, 0); // Base normal points down
                }
            }
        }
    }
    
    // Check intersection with four triangular faces
    Vector3 corners[4] = {
        Vector3(position_.x - half_base, position_.y, position_.z - half_base),
        Vector3(position_.x + half_base, position_.y, position_.z - half_base),
        Vector3(position_.x + half_base, position_.y, position_.z + half_base),
        Vector3(position_.x - half_base, position_.y, position_.z + half_base)
    };
    
    for (int i = 0; i < 4; i++) {
        Vector3 v0 = corners[i];
        Vector3 v1 = corners[(i + 1) % 4];
        Vector3 v2 = apex;
        
        // Ray-triangle intersection using Möller-Trumbore algorithm
        Vector3 edge1 = v1 - v0;
        Vector3 edge2 = v2 - v0;
        Vector3 h = ray.direction.cross(edge2);
        float a = edge1.dot(h);
        
        if (a > -1e-6f && a < 1e-6f) continue; // Ray parallel to triangle
        
        float f = 1.0f / a;
        Vector3 s = ray.origin - v0;
        float u = f * s.dot(h);
        
        if (u < 0.0f || u > 1.0f) continue;
        
        Vector3 q = s.cross(edge1);
        float v = f * ray.direction.dot(q);
        
        if (v < 0.0f || u + v > 1.0f) continue;
        
        float t = f * edge2.dot(q);
        
        if (t >= t_min && t <= t_max && t < closest_t) {
            closest_t = t;
            closest_normal = edge1.cross(edge2).normalized();
        }
    }
    
    if (closest_t == std::numeric_limits<float>::max()) {
        return false;
    }
    
    rec.t = closest_t;
    rec.point = ray.at(rec.t);
    rec.set_face_normal(ray, closest_normal);
    rec.material = material_;
    
    
    return true;
}