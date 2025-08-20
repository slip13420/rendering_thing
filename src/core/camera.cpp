#include "camera.h"
#include <cmath>

Camera::Camera(const Vector3& position, const Vector3& target, const Vector3& up, 
               float fov, float aspect_ratio) 
    : position_(position), target_(target), up_(up), fov_(fov), aspect_ratio_(aspect_ratio) {
    update_camera_vectors();
}

Ray Camera::get_ray(float u, float v) const {
    Vector3 direction = lower_left_corner_ + horizontal_ * u + vertical_ * v - position_;
    return Ray(position_, direction);
}

void Camera::set_position(const Vector3& position) {
    position_ = position;
    update_camera_vectors();
}

void Camera::set_target(const Vector3& target) {
    target_ = target;
    update_camera_vectors();
}

void Camera::set_up(const Vector3& up) {
    up_ = up;
    update_camera_vectors();
}

void Camera::set_aspect_ratio(float aspect_ratio) {
    aspect_ratio_ = aspect_ratio;
    update_camera_vectors();
}

void Camera::update_camera_vectors() {
    // Calculate camera coordinate system
    Vector3 w = (position_ - target_).normalized();  // Points away from target
    Vector3 u = up_.cross(w).normalized();           // Right vector
    Vector3 v = w.cross(u);                          // Up vector
    
    // Calculate viewport dimensions
    float theta = fov_ * M_PI / 180.0f;
    float half_height = std::tan(theta / 2.0f);
    float half_width = aspect_ratio_ * half_height;
    
    // Calculate viewport corners
    horizontal_ = u * (2.0f * half_width);
    vertical_ = v * (2.0f * half_height);
    lower_left_corner_ = position_ - horizontal_ * 0.5f - vertical_ * 0.5f - w;
}