#pragma once

#include <memory>
#include <vector>
#include <string>
#include <stdexcept>
#include <iostream>
#include <cmath>

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
    
    constexpr float dot(const Vector3& other) const noexcept {
        return x * other.x + y * other.y + z * other.z;
    }
    
    float length() const noexcept {
        return std::sqrt(x * x + y * y + z * z);
    }
    
    Vector3 normalized() const noexcept {
        float len = length();
        if (len > 0.0f) {
            return *this * (1.0f / len);
        }
        return Vector3(0.0f, 0.0f, 0.0f);
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
};