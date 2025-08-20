#pragma once

#include "common.h"

class Camera {
public:
    Camera(const Vector3& position = Vector3(0, 0, 0),
           const Vector3& target = Vector3(0, 0, -1),
           const Vector3& up = Vector3(0, 1, 0),
           float fov = 45.0f, float aspect_ratio = 16.0f/9.0f);
    
    // Default copy/move operations are fine for this simple class
    Camera(const Camera&) = default;
    Camera& operator=(const Camera&) = default;
    Camera(Camera&&) = default;
    Camera& operator=(Camera&&) = default;
    
    Ray get_ray(float u, float v) const;
    
    // Getters
    const Vector3& get_position() const { return position_; }
    const Vector3& get_lower_left_corner() const { return lower_left_corner_; }
    const Vector3& get_horizontal() const { return horizontal_; }
    const Vector3& get_vertical() const { return vertical_; }
    float get_aspect_ratio() const { return aspect_ratio_; }
    
    // Setters
    void set_position(const Vector3& position);
    void set_target(const Vector3& target);
    void set_up(const Vector3& up);
    void set_aspect_ratio(float aspect_ratio);
    
private:
    void update_camera_vectors();
    
    Vector3 position_;
    Vector3 target_;
    Vector3 up_;
    Vector3 lower_left_corner_;
    Vector3 horizontal_;
    Vector3 vertical_;
    float fov_;
    float aspect_ratio_;
};