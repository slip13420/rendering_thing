#pragma once

#include <memory>
#include <vector>
#include <string>
#include <stdexcept>
#include <iostream>
#include <cmath>
#include <random>
#include <limits>
#include <algorithm>

struct Vector3 {
    float x, y, z;
    
    constexpr Vector3(float x = 0.0f, float y = 0.0f, float z = 0.0f) noexcept 
        : x(x), y(y), z(z) {}
    
    // Basic vector operations
    constexpr Vector3 operator+(const Vector3& other) const noexcept {
        return Vector3(x + other.x, y + other.y, z + other.z);
    }
    
    constexpr Vector3 operator-(const Vector3& other) const noexcept {
        return Vector3(x - other.x, y - other.y, z - other.z);
    }
    
    constexpr Vector3 operator*(float scalar) const noexcept {
        return Vector3(x * scalar, y * scalar, z * scalar);
    }
    
    friend constexpr Vector3 operator*(float scalar, const Vector3& v) noexcept {
        return Vector3(v.x * scalar, v.y * scalar, v.z * scalar);
    }
    
    constexpr float dot(const Vector3& other) const noexcept {
        return x * other.x + y * other.y + z * other.z;
    }
    
    float length() const noexcept {
        return std::sqrt(x * x + y * y + z * z);
    }
    
    Vector3 normalized() const noexcept {
        float len = length();
        if (len > 1e-8f) {  // Use epsilon comparison instead of exact zero
            return *this * (1.0f / len);
        }
        return Vector3(0.0f, 0.0f, 0.0f);
    }
    
    Vector3 cross(const Vector3& other) const noexcept {
        return Vector3(
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        );
    }
    
    Vector3& operator+=(const Vector3& other) noexcept {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }
    
    Vector3& operator*=(float scalar) noexcept {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        return *this;
    }
    
    Vector3 operator-() const noexcept {
        return Vector3(-x, -y, -z);
    }
    
    constexpr float length_squared() const noexcept {
        return x * x + y * y + z * z;
    }
};

struct Color {
    float r, g, b, a;
    
    constexpr Color(float r = 0.0f, float g = 0.0f, float b = 0.0f, float a = 1.0f) noexcept 
        : r(r), g(g), b(b), a(a) {}
    
    // Common color constants
    static constexpr Color black() noexcept { return Color(0.0f, 0.0f, 0.0f, 1.0f); }
    static constexpr Color white() noexcept { return Color(1.0f, 1.0f, 1.0f, 1.0f); }
    static constexpr Color red() noexcept { return Color(1.0f, 0.0f, 0.0f, 1.0f); }
    static constexpr Color green() noexcept { return Color(0.0f, 1.0f, 0.0f, 1.0f); }
    static constexpr Color blue() noexcept { return Color(0.0f, 0.0f, 1.0f, 1.0f); }
    
    // Color blending
    constexpr Color operator*(float scalar) const noexcept {
        return Color(r * scalar, g * scalar, b * scalar, a);
    }
    
    constexpr Color operator+(const Color& other) const noexcept {
        return Color(r + other.r, g + other.g, b + other.b, a);
    }
    
    Color operator*(const Color& other) const noexcept {
        return Color(r * other.r, g * other.g, b * other.b, a);
    }
    
    Color& operator+=(const Color& other) noexcept {
        r += other.r;
        g += other.g;
        b += other.b;
        return *this;
    }
    
    Color& operator*=(float scalar) noexcept {
        r *= scalar;
        g *= scalar;
        b *= scalar;
        return *this;
    }
    
    constexpr Color operator/(float scalar) const noexcept {
        return Color(r / scalar, g / scalar, b / scalar, a);
    }
    
    Color clamped() const noexcept {
        return Color(
            std::clamp(r, 0.0f, 1.0f),
            std::clamp(g, 0.0f, 1.0f),
            std::clamp(b, 0.0f, 1.0f),
            std::clamp(a, 0.0f, 1.0f)
        );
    }
};

struct Ray {
    Vector3 origin;
    Vector3 direction;
    float t_min;
    float t_max;
    
    Ray(const Vector3& origin, const Vector3& direction, 
        float t_min = 0.001f, float t_max = std::numeric_limits<float>::infinity()) 
        : origin(origin), direction(direction.normalized()), t_min(t_min), t_max(t_max) {}
    
    Vector3 at(float t) const noexcept {
        return origin + direction * t;
    }
};

struct Material {
    Color albedo;       // Surface color
    float roughness;    // 0.0 = mirror, 1.0 = completely diffuse
    float metallic;     // 0.0 = dielectric, 1.0 = metal
    float emission;     // Emission strength for light sources
    
    Material(const Color& albedo = Color::white(), float roughness = 1.0f, 
             float metallic = 0.0f, float emission = 0.0f) 
        : albedo(albedo), roughness(std::clamp(roughness, 0.0f, 1.0f)),
          metallic(std::clamp(metallic, 0.0f, 1.0f)), emission(std::max(0.0f, emission)) {}
    
    bool is_emissive() const noexcept {
        return emission > 0.0f;
    }
};

struct HitRecord {
    Vector3 point;
    Vector3 normal;
    Material material;
    float t;
    bool front_face;
    
    void set_face_normal(const Ray& ray, const Vector3& outward_normal) noexcept {
        front_face = ray.direction.dot(outward_normal) < 0;
        normal = front_face ? outward_normal : outward_normal * -1.0f;
    }
};