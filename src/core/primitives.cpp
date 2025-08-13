#include "primitives.h"

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