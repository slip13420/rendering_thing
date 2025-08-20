#pragma once

#include "common.h"

class Primitive {
public:
    explicit Primitive(const Vector3& position, const Color& color = Color::white(), 
                      const Material& material = Material()) 
        : position_(position), color_(color), material_(material) {}
    virtual ~Primitive() = default;
    
    // Non-copyable to avoid slicing issues
    Primitive(const Primitive&) = delete;
    Primitive& operator=(const Primitive&) = delete;
    
    // Movable
    Primitive(Primitive&&) = default;
    Primitive& operator=(Primitive&&) = default;
    
    virtual void update() = 0;
    virtual bool hit(const Ray& ray, float t_min, float t_max, HitRecord& rec) const = 0;
    
    // Getters
    const Vector3& position() const noexcept { return position_; }
    const Color& color() const noexcept { return color_; }
    const Material& material() const noexcept { return material_; }
    
    // Setters
    void set_position(const Vector3& position) noexcept { position_ = position; }
    void set_color(const Color& color) noexcept { color_ = color; }
    void set_material(const Material& material) noexcept { material_ = material; }
    
protected:
    Vector3 position_;
    Color color_;
    Material material_;
};

class Sphere : public Primitive {
public:
    Sphere(const Vector3& position, float radius, const Color& color = Color::white(),
           const Material& material = Material()) 
        : Primitive(position, color, material), radius_(radius) {
        if (radius <= 0.0f) {
            throw std::invalid_argument("Sphere radius must be positive");
        }
    }
    
    void update() override;
    bool hit(const Ray& ray, float t_min, float t_max, HitRecord& rec) const override;
    
    float radius() const noexcept { return radius_; }
    void set_radius(float radius) {
        if (radius <= 0.0f) {
            throw std::invalid_argument("Sphere radius must be positive");
        }
        radius_ = radius;
    }
    
private:
    float radius_;
};

class Cube : public Primitive {
public:
    Cube(const Vector3& position, float size, const Color& color = Color::white(),
         const Material& material = Material())
        : Primitive(position, color, material), size_(size) {
        if (size <= 0.0f) {
            throw std::invalid_argument("Cube size must be positive");
        }
    }
    
    void update() override;
    bool hit(const Ray& ray, float t_min, float t_max, HitRecord& rec) const override;
    
    float size() const noexcept { return size_; }
    void set_size(float size) {
        if (size <= 0.0f) {
            throw std::invalid_argument("Cube size must be positive");
        }
        size_ = size;
    }
    
private:
    float size_;
};

class Torus : public Primitive {
public:
    Torus(const Vector3& position, float major_radius, float minor_radius, 
          const Color& color = Color::white(), const Material& material = Material())
        : Primitive(position, color, material), major_radius_(major_radius), minor_radius_(minor_radius) {
        if (major_radius <= 0.0f || minor_radius <= 0.0f) {
            throw std::invalid_argument("Torus radii must be positive");
        }
        if (minor_radius >= major_radius) {
            throw std::invalid_argument("Torus minor radius must be less than major radius");
        }
    }
    
    void update() override;
    bool hit(const Ray& ray, float t_min, float t_max, HitRecord& rec) const override;
    
    float major_radius() const noexcept { return major_radius_; }
    float minor_radius() const noexcept { return minor_radius_; }
    void set_radii(float major_radius, float minor_radius) {
        if (major_radius <= 0.0f || minor_radius <= 0.0f) {
            throw std::invalid_argument("Torus radii must be positive");
        }
        if (minor_radius >= major_radius) {
            throw std::invalid_argument("Torus minor radius must be less than major radius");
        }
        major_radius_ = major_radius;
        minor_radius_ = minor_radius;
    }
    
private:
    float major_radius_;
    float minor_radius_;
};

class Pyramid : public Primitive {
public:
    Pyramid(const Vector3& position, float base_size, float height,
            const Color& color = Color::white(), const Material& material = Material())
        : Primitive(position, color, material), base_size_(base_size), height_(height) {
        if (base_size <= 0.0f || height <= 0.0f) {
            throw std::invalid_argument("Pyramid dimensions must be positive");
        }
    }
    
    void update() override;
    bool hit(const Ray& ray, float t_min, float t_max, HitRecord& rec) const override;
    
    float base_size() const noexcept { return base_size_; }
    float height() const noexcept { return height_; }
    void set_dimensions(float base_size, float height) {
        if (base_size <= 0.0f || height <= 0.0f) {
            throw std::invalid_argument("Pyramid dimensions must be positive");
        }
        base_size_ = base_size;
        height_ = height;
    }
    
private:
    float base_size_;
    float height_;
};