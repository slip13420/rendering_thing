#pragma once

#include "common.h"
#include "primitives.h"
#include <vector>
#include <memory>

class SceneManager {
public:
    SceneManager();
    ~SceneManager();
    
    void initialize();
    void update();
    void shutdown();
    
    // Scene object management
    void add_object(std::shared_ptr<Primitive> object);
    void remove_object(std::shared_ptr<Primitive> object);
    void clear_objects();
    const std::vector<std::shared_ptr<Primitive>>& get_objects() const;
    
    // Light source management  
    void add_light(const Vector3& position, const Color& color, float intensity);
    void clear_lights();
    const std::vector<std::shared_ptr<Primitive>>& get_lights() const;
    
    // Scene queries for path tracer
    bool hit_scene(const Ray& ray, float t_min, float t_max, HitRecord& rec) const;
    Color get_background_color(const Ray& ray) const;
    
private:
    bool initialized_;
    std::vector<std::shared_ptr<Primitive>> objects_;
    std::vector<std::shared_ptr<Primitive>> lights_;
    
    void setup_default_scene();
};