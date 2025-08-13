#include "scene_manager.h"
#include <algorithm>
#include <iostream>

SceneManager::SceneManager() : initialized_(false) {
    
}

SceneManager::~SceneManager() {
    
}

void SceneManager::initialize() {
    clear_objects();
    clear_lights();
    setup_default_scene();
    initialized_ = true;
    std::cout << "SceneManager initialized with " << objects_.size() 
              << " objects and " << lights_.size() << " lights" << std::endl;
}

void SceneManager::update() {
    for (auto& object : objects_) {
        object->update();
    }
    for (auto& light : lights_) {
        light->update();
    }
}

void SceneManager::shutdown() {
    clear_objects();
    clear_lights();
    initialized_ = false;
}

void SceneManager::add_object(std::shared_ptr<Primitive> object) {
    if (object) {
        objects_.push_back(object);
    }
}

void SceneManager::remove_object(std::shared_ptr<Primitive> object) {
    auto it = std::find(objects_.begin(), objects_.end(), object);
    if (it != objects_.end()) {
        objects_.erase(it);
    }
}

void SceneManager::clear_objects() {
    objects_.clear();
}

const std::vector<std::shared_ptr<Primitive>>& SceneManager::get_objects() const {
    return objects_;
}

void SceneManager::add_light(const Vector3& position, const Color& color, float intensity) {
    // Create a small emissive sphere as a light source
    Material light_material(color, 0.0f, 0.0f, intensity);
    auto light_sphere = std::make_shared<Sphere>(position, 0.1f, color, light_material);
    lights_.push_back(light_sphere);
    objects_.push_back(light_sphere); // Also add to objects for rendering
}

void SceneManager::clear_lights() {
    lights_.clear();
}

const std::vector<std::shared_ptr<Primitive>>& SceneManager::get_lights() const {
    return lights_;
}

bool SceneManager::hit_scene(const Ray& ray, float t_min, float t_max, HitRecord& rec) const {
    HitRecord temp_rec;
    bool hit_anything = false;
    float closest_so_far = t_max;
    
    for (const auto& object : objects_) {
        if (object->hit(ray, t_min, closest_so_far, temp_rec)) {
            hit_anything = true;
            closest_so_far = temp_rec.t;
            rec = temp_rec;
        }
    }
    
    return hit_anything;
}

Color SceneManager::get_background_color(const Ray& ray) const {
    // Simple sky gradient
    Vector3 unit_direction = ray.direction.normalized();
    float t = 0.5f * (unit_direction.y + 1.0f);
    return Color(1.0f, 1.0f, 1.0f) * (1.0f - t) + Color(0.5f, 0.7f, 1.0f) * t;
}

void SceneManager::setup_default_scene() {
    // Create some default objects for testing
    
    // Ground plane (large sphere)
    Material ground_material(Color(0.5f, 0.5f, 0.5f), 1.0f, 0.0f, 0.0f);
    auto ground = std::make_shared<Sphere>(Vector3(0, -100.5f, -1), 100.0f, Color::white(), ground_material);
    add_object(ground);
    
    // Center sphere  
    Material center_material(Color(0.7f, 0.3f, 0.3f), 0.8f, 0.0f, 0.0f);
    auto center_sphere = std::make_shared<Sphere>(Vector3(0, 0, -1), 0.5f, Color::red(), center_material);
    add_object(center_sphere);
    
    // Left sphere (metal)
    Material left_material(Color(0.8f, 0.8f, 0.9f), 0.1f, 1.0f, 0.0f);
    auto left_sphere = std::make_shared<Sphere>(Vector3(-1, 0, -1), 0.5f, Color::white(), left_material);
    add_object(left_sphere);
    
    // Right sphere (glass-like)
    Material right_material(Color(0.8f, 0.6f, 0.2f), 0.3f, 0.0f, 0.0f);
    auto right_sphere = std::make_shared<Sphere>(Vector3(1, 0, -1), 0.5f, Color(0.8f, 0.6f, 0.2f), right_material);
    add_object(right_sphere);
    
    // Add a cube
    Material cube_material(Color(0.2f, 0.8f, 0.2f), 0.6f, 0.0f, 0.0f);
    auto cube = std::make_shared<Cube>(Vector3(0, 1, -2), 0.8f, Color::green(), cube_material);
    add_object(cube);
    
    // Add a light source
    add_light(Vector3(2, 4, -1), Color(1.0f, 1.0f, 0.8f), 5.0f);
}