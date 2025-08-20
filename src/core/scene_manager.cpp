#include "scene_manager.h"
#include "core/camera.h"
#include "core/primitives.h"
#include "render/gpu_memory.h"
#include <algorithm>
#include <iostream>
#include <fstream>

SceneManager::SceneManager() 
    : initialized_(false)
    , next_primitive_id_(1)
    , gpu_primitive_data_dirty_(false)
    , gpu_synced_(false)
    , gpu_buffer_primitive_count_(0) {
    
}

SceneManager::~SceneManager() {
    
}

void SceneManager::initialize() {
    clear_objects();
    clear_lights();
    setup_default_scene();
    create_default_camera();
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
#ifdef USE_GPU
        markGPUDirty();
#endif
    }
}

void SceneManager::remove_object(std::shared_ptr<Primitive> object) {
    auto it = std::find(objects_.begin(), objects_.end(), object);
    if (it != objects_.end()) {
        objects_.erase(it);
#ifdef USE_GPU
        markGPUDirty();
#endif
    }
}

void SceneManager::clear_objects() {
    objects_.clear();
#ifdef USE_GPU
    markGPUDirty();
#endif
}

const std::vector<std::shared_ptr<Primitive>>& SceneManager::get_objects() const {
    return objects_;
}

PrimitiveID SceneManager::addPrimitive(PrimitiveType type, const Vector3& position, 
                                      const Color& color, const Material& material) {
    PrimitiveID id = generatePrimitiveID();
    if (id == INVALID_PRIMITIVE_ID) {
        std::cout << "Failed to generate primitive ID" << std::endl;
        return INVALID_PRIMITIVE_ID;
    }
    
    auto primitive = createPrimitive(type, position, color, material);
    if (!primitive) {
        std::cout << "Failed to create primitive of type " << static_cast<uint32_t>(type) << std::endl;
        return INVALID_PRIMITIVE_ID;
    }
    
    primitives_by_id_[id] = primitive;
    primitive_ids_[primitive] = id;
    objects_.push_back(primitive);
    
    updateGPUPrimitiveData(id);
    markPrimitiveGPUDirty();
    
#ifdef USE_GPU
    markGPUDirty();
#endif
    
    std::cout << "Added primitive " << id << " of type " << static_cast<uint32_t>(type) 
              << " at position (" << position.x << ", " << position.y << ", " << position.z << ")" << std::endl;
    
    return id;
}

bool SceneManager::removePrimitive(PrimitiveID id) {
    auto it = primitives_by_id_.find(id);
    if (it == primitives_by_id_.end()) {
        return false;
    }
    
    auto primitive = it->second;
    
    auto objects_it = std::find(objects_.begin(), objects_.end(), primitive);
    if (objects_it != objects_.end()) {
        objects_.erase(objects_it);
    }
    
    primitives_by_id_.erase(it);
    primitive_ids_.erase(primitive);
    
    removeFromGPUData(id);
    markPrimitiveGPUDirty();
    
#ifdef USE_GPU
    markGPUDirty();
#endif
    
    std::cout << "Removed primitive " << id << std::endl;
    return true;
}

std::shared_ptr<Primitive> SceneManager::getPrimitive(PrimitiveID id) const {
    auto it = primitives_by_id_.find(id);
    if (it != primitives_by_id_.end()) {
        return it->second;
    }
    return nullptr;
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
    // Brighter sky gradient for better ambient lighting
    Vector3 unit_direction = ray.direction.normalized();
    float t = 0.5f * (unit_direction.y + 1.0f);
    return Color(1.0f, 1.0f, 1.0f) * (1.0f - t) * 0.8f + Color(0.5f, 0.7f, 1.0f) * t * 0.8f;
}

void SceneManager::setup_default_scene() {
    // Create some default objects for testing
    
    // Ground plane (large sphere) - Use consistent material color
    Material ground_material(Color(0.5f, 0.5f, 0.5f), 1.0f, 0.0f, 0.0f);
    auto ground = std::make_shared<Sphere>(Vector3(0, -100.5f, -1), 100.0f, ground_material.albedo, ground_material);
    add_object(ground);
    
    // Also add to GPU primitive tracking for default objects
    PrimitiveID ground_id = generatePrimitiveID();
    primitives_by_id_[ground_id] = ground;
    primitive_ids_[ground] = ground_id;
    updateGPUPrimitiveData(ground_id);
    
    // Center sphere - Use consistent material color
    Material center_material(Color(0.7f, 0.3f, 0.3f), 0.8f, 0.0f, 0.0f);
    auto center_sphere = std::make_shared<Sphere>(Vector3(0, 0, -1), 0.5f, center_material.albedo, center_material);
    add_object(center_sphere);
    
    PrimitiveID center_id = generatePrimitiveID();
    primitives_by_id_[center_id] = center_sphere;
    primitive_ids_[center_sphere] = center_id;
    updateGPUPrimitiveData(center_id);
    
    // Left sphere (metal) - Use consistent material color
    Material left_material(Color(0.8f, 0.8f, 0.9f), 0.1f, 1.0f, 0.0f);
    auto left_sphere = std::make_shared<Sphere>(Vector3(-1, 0, -1), 0.5f, left_material.albedo, left_material);
    add_object(left_sphere);
    
    PrimitiveID left_id = generatePrimitiveID();
    primitives_by_id_[left_id] = left_sphere;
    primitive_ids_[left_sphere] = left_id;
    updateGPUPrimitiveData(left_id);
    
    // Right sphere (glass-like) - Use consistent material color
    Material right_material(Color(0.8f, 0.6f, 0.2f), 0.3f, 0.0f, 0.0f);
    auto right_sphere = std::make_shared<Sphere>(Vector3(1, 0, -1), 0.5f, right_material.albedo, right_material);
    add_object(right_sphere);
    
    PrimitiveID right_id = generatePrimitiveID();
    primitives_by_id_[right_id] = right_sphere;
    primitive_ids_[right_sphere] = right_id;
    updateGPUPrimitiveData(right_id);
    
    // Add a cube - Use consistent material color
    Material cube_material(Color(0.2f, 0.8f, 0.2f), 0.6f, 0.0f, 0.0f);
    auto cube = std::make_shared<Cube>(Vector3(0, 1, -2), 0.8f, cube_material.albedo, cube_material);
    add_object(cube);
    
    PrimitiveID cube_id = generatePrimitiveID();
    primitives_by_id_[cube_id] = cube;
    primitive_ids_[cube] = cube_id;
    updateGPUPrimitiveData(cube_id);
    
    // Add a torus for debugging - Use consistent material color
    Material torus_material(Color(0.0f, 0.0f, 1.0f), 0.4f, 0.8f, 0.0f);
    auto torus = std::make_shared<Torus>(Vector3(-1.5f, 0, -2), 0.8f, 0.3f, torus_material.albedo, torus_material);
    add_object(torus);
    
    PrimitiveID torus_id = generatePrimitiveID();
    primitives_by_id_[torus_id] = torus;
    primitive_ids_[torus] = torus_id;
    updateGPUPrimitiveData(torus_id);
    
    // Add a pyramid for debugging - Use consistent material color
    Material pyramid_material(Color(1.0f, 1.0f, 0.0f), 0.5f, 0.2f, 0.0f);
    auto pyramid = std::make_shared<Pyramid>(Vector3(1.5f, 0, -2), 1.0f, 1.2f, pyramid_material.albedo, pyramid_material);
    add_object(pyramid);
    
    PrimitiveID pyramid_id = generatePrimitiveID();
    primitives_by_id_[pyramid_id] = pyramid;
    primitive_ids_[pyramid] = pyramid_id;
    updateGPUPrimitiveData(pyramid_id);
    
    // Mark primitive data as dirty to trigger GPU sync
    markPrimitiveGPUDirty();
    
    // Add a light source
    add_light(Vector3(2, 4, -1), Color(1.0f, 1.0f, 0.8f), 5.0f);
}

void SceneManager::create_default_camera() {
    // Create default camera at a reasonable position
    Vector3 default_position(0, 0, 3);
    Vector3 default_target(0, 0, 0);
    Vector3 default_up(0, 1, 0);
    
    camera_ = std::make_shared<Camera>(default_position, default_target, default_up);
}

// Camera management functions
void SceneManager::set_camera(std::shared_ptr<Camera> camera) {
    camera_ = camera;
}

std::shared_ptr<Camera> SceneManager::get_camera() const {
    return camera_;
}

Vector3 SceneManager::get_camera_position() const {
    if (camera_) {
        return camera_->get_position();
    }
    return Vector3(0, 0, 0);
}

void SceneManager::set_camera_position(const Vector3& position) {
    if (camera_ && is_valid_camera_position(position)) {
        camera_->set_position(position);
    }
}

void SceneManager::set_camera_target(const Vector3& target) {
    if (camera_) {
        camera_->set_target(target);
    }
}

void SceneManager::set_camera_up(const Vector3& up) {
    if (camera_) {
        camera_->set_up(up);
    }
}

bool SceneManager::is_valid_camera_position(const Vector3& position) const {
    // Check for reasonable coordinate ranges
    const float max_coord = 1000.0f;
    const float min_coord = -1000.0f;
    
    if (position.x < min_coord || position.x > max_coord ||
        position.y < min_coord || position.y > max_coord ||
        position.z < min_coord || position.z > max_coord) {
        return false;
    }
    
    // Check if position intersects with scene objects (simplified check)
    for (const auto& object : objects_) {
        Vector3 obj_center = object->position();
        Vector3 to_object = position - obj_center;
        
        // Simple distance check - use a conservative radius estimate
        float min_distance = 0.5f; // Minimum safe distance from any object
        if (to_object.length() < min_distance) {
            return false; // Too close to an object
        }
    }
    
    return true;
}

// Helper methods for primitive management
PrimitiveID SceneManager::generatePrimitiveID() {
    if (next_primitive_id_ == INVALID_PRIMITIVE_ID) {
        next_primitive_id_++;
    }
    return next_primitive_id_++;
}

std::shared_ptr<Primitive> SceneManager::createPrimitive(PrimitiveType type, const Vector3& position, 
                                                        const Color& color, const Material& material) {
    // Use material.albedo for consistency (material takes precedence over color parameter)
    switch (type) {
        case PrimitiveType::SPHERE:
            std::cout << "Creating Sphere primitive" << std::endl;
            return std::make_shared<Sphere>(position, 0.5f, material.albedo, material);
        case PrimitiveType::CUBE:
            std::cout << "Creating Cube primitive" << std::endl;
            return std::make_shared<Cube>(position, 1.0f, material.albedo, material);
        case PrimitiveType::TORUS:
            std::cout << "Creating Torus primitive (major=" << 0.8f << ", minor=" << 0.3f << ")" << std::endl;
            return std::make_shared<Torus>(position, 0.8f, 0.3f, material.albedo, material);
        case PrimitiveType::PYRAMID:
            std::cout << "Creating Pyramid primitive (base=" << 1.0f << ", height=" << 1.2f << ")" << std::endl;
            return std::make_shared<Pyramid>(position, 1.0f, 1.2f, material.albedo, material);
        default:
            std::cout << "Unknown primitive type: " << static_cast<uint32_t>(type) << std::endl;
            return nullptr;
    }
}

void SceneManager::updateGPUPrimitiveData(PrimitiveID id) {
    auto it = primitives_by_id_.find(id);
    if (it == primitives_by_id_.end()) {
        return;
    }
    
    auto primitive = it->second;
    GPUPrimitiveData gpu_data;
    
    Vector3 pos = primitive->position();
    Color col = primitive->color();
    Material mat = primitive->material();
    
    gpu_data.position[0] = pos.x;
    gpu_data.position[1] = pos.y;
    gpu_data.position[2] = pos.z;
    
    // Store primitive size in position[3] instead of padding
    float size = 0.5f; // Default size
    if (auto sphere = std::dynamic_pointer_cast<Sphere>(primitive)) {
        gpu_data.type = static_cast<uint32_t>(PrimitiveType::SPHERE);
        size = sphere->radius();
    } else if (auto cube = std::dynamic_pointer_cast<Cube>(primitive)) {
        gpu_data.type = static_cast<uint32_t>(PrimitiveType::CUBE);
        size = cube->size();
    } else if (auto torus = std::dynamic_pointer_cast<Torus>(primitive)) {
        gpu_data.type = static_cast<uint32_t>(PrimitiveType::TORUS);
        size = torus->major_radius(); // Use major radius as base size
    } else if (auto pyramid = std::dynamic_pointer_cast<Pyramid>(primitive)) {
        gpu_data.type = static_cast<uint32_t>(PrimitiveType::PYRAMID);
        size = pyramid->base_size();
    } else {
        gpu_data.type = 0; // Unknown type
    }
    
    gpu_data.position[3] = size; // Store size in position[3]
    
    gpu_data.rotation[0] = 0.0f; // No rotation for now
    gpu_data.rotation[1] = 0.0f;
    gpu_data.rotation[2] = 0.0f;
    gpu_data.rotation[3] = 1.0f; // w component
    
    gpu_data.scale[0] = 1.0f; // Default scale
    gpu_data.scale[1] = 1.0f;
    gpu_data.scale[2] = 1.0f;
    gpu_data.scale[3] = 0.0f; // padding
    
    gpu_data.id = id;
    
    gpu_data.color[0] = col.r;
    gpu_data.color[1] = col.g;
    gpu_data.color[2] = col.b;
    gpu_data.color[3] = col.a;
    
    gpu_data.material[0] = mat.roughness;
    gpu_data.material[1] = mat.metallic;
    gpu_data.material[2] = mat.emission;
    gpu_data.material[3] = 0.0f; // padding
    
    // Debug output
    std::cout << "GPU primitive data - Object type=" << gpu_data.type << ", size=" << size 
              << ", pos=(" << pos.x << "," << pos.y << "," << pos.z << ")" << std::endl;
    
    // Find existing data or add new
    auto data_it = std::find_if(gpu_primitive_data_.begin(), gpu_primitive_data_.end(),
                               [id](const GPUPrimitiveData& data) { return data.id == id; });
    
    if (data_it != gpu_primitive_data_.end()) {
        *data_it = gpu_data;
    } else {
        gpu_primitive_data_.push_back(gpu_data);
    }
}

void SceneManager::removeFromGPUData(PrimitiveID id) {
    auto it = std::remove_if(gpu_primitive_data_.begin(), gpu_primitive_data_.end(),
                            [id](const GPUPrimitiveData& data) { return data.id == id; });
    gpu_primitive_data_.erase(it, gpu_primitive_data_.end());
}

void SceneManager::markPrimitiveGPUDirty() {
    gpu_primitive_data_dirty_ = true;
}

#ifdef USE_GPU
// GPU memory integration methods
void SceneManager::setGPUMemoryManager(std::shared_ptr<GPUMemoryManager> gpu_memory) {
    gpu_memory_manager_ = gpu_memory;
    markGPUDirty();
}

void SceneManager::syncSceneToGPU() {
    if (!gpu_memory_manager_ || objects_.empty()) {
        return;
    }
    
    // Check if we need to resize the GPU buffer
    resizeGPUBufferIfNeeded();
    
    if (!scene_gpu_buffer_) {
        std::cout << "Warning: No GPU buffer available for scene sync" << std::endl;
        return;
    }
    
    // Convert all primitives to simple GPU data format
    std::vector<float> gpu_scene_data;
    gpu_scene_data.reserve(objects_.size() * 16); // 16 floats per primitive
    
    for (const auto& object : objects_) {
        if (object) {
            Vector3 pos = object->position();
            Color col = object->color();
            Material mat = object->material();
            
            // Determine primitive type and size
            float primitiveType = 1.0f; // Default to sphere
            float size = 0.5f; // Default size
            
            if (auto sphere = std::dynamic_pointer_cast<Sphere>(object)) {
                primitiveType = 1.0f;
                size = sphere->radius();
            } else if (auto cube = std::dynamic_pointer_cast<Cube>(object)) {
                primitiveType = 2.0f;
                size = cube->size();
            } else if (auto torus = std::dynamic_pointer_cast<Torus>(object)) {
                primitiveType = 3.0f;
                size = torus->major_radius(); // Use major radius as base size
            } else if (auto pyramid = std::dynamic_pointer_cast<Pyramid>(object)) {
                primitiveType = 4.0f;
                size = pyramid->base_size();
            }
            
            // Format: vec4(pos.xyz, size), vec4(color.rgb, material), vec4(type, padding...)
            gpu_scene_data.insert(gpu_scene_data.end(), {
                pos.x, pos.y, pos.z, size,                                    // positionData
                col.r, col.g, col.b, mat.roughness,                          // materialData
                primitiveType, mat.metallic, mat.emission, 0.0f               // typeData
            });
            
            // Debug output
            std::cout << "GPU sync - Object type=" << primitiveType << ", size=" << size 
                      << ", pos=(" << pos.x << "," << pos.y << "," << pos.z << ")" << std::endl;
            
            // Write to debug file
            static bool first_write = true;
            std::ofstream debug_file("/tmp/gpu_sync_debug.txt", first_write ? std::ios::out : std::ios::app);
            debug_file << "GPU sync - Object type=" << primitiveType << ", size=" << size 
                      << ", pos=(" << pos.x << "," << pos.y << "," << pos.z << ")" << std::endl;
            debug_file.close();
            first_write = false;
        }
    }
    
    // Transfer the scene data to GPU
    bool success = gpu_memory_manager_->transferSceneData(scene_gpu_buffer_, gpu_scene_data);
    
    if (success) {
        gpu_synced_ = true;
        gpu_buffer_primitive_count_ = objects_.size();
        std::cout << "Scene synced to GPU: " << objects_.size() << " primitives" << std::endl;
    } else {
        std::cout << "Failed to sync scene to GPU: " << gpu_memory_manager_->getErrorMessage() << std::endl;
    }
}

void SceneManager::updateGPUPrimitive(size_t primitive_index) {
    if (!gpu_memory_manager_ || !scene_gpu_buffer_ || 
        primitive_index >= objects_.size() || !objects_[primitive_index]) {
        return;
    }
    
    // Convert single primitive to simple GPU format
    Vector3 pos = objects_[primitive_index]->position();
    Color col = objects_[primitive_index]->color();
    std::vector<float> gpu_data = {pos.x, pos.y, pos.z, col.r, col.g, col.b, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    
    // Calculate offset for this primitive in the GPU buffer
    size_t offset = primitive_index * 16 * sizeof(float);
    
    // Update just this primitive in GPU memory
    bool success = gpu_memory_manager_->transferToGPU(
        scene_gpu_buffer_, 
        gpu_data.data(), 
        16 * sizeof(float), 
        offset
    );
    
    if (!success) {
        std::cout << "Failed to update GPU primitive " << primitive_index 
                  << ": " << gpu_memory_manager_->getErrorMessage() << std::endl;
        markGPUDirty(); // Mark for full resync
    }
}

void SceneManager::removeGPUPrimitive(size_t primitive_index) {
    if (!gpu_memory_manager_ || !scene_gpu_buffer_) {
        return;
    }
    
    // For now, mark entire scene as dirty since OpenGL doesn't have 
    // efficient partial buffer removal. A more sophisticated implementation
    // could compact the GPU buffer or use indirect rendering.
    markGPUDirty();
    
    std::cout << "Primitive " << primitive_index << " removed, GPU sync required" << std::endl;
}

std::shared_ptr<GPUBuffer> SceneManager::getSceneGPUBuffer() const {
    return scene_gpu_buffer_;
}

bool SceneManager::isGPUSynced() const {
    return gpu_synced_ && scene_gpu_buffer_ && 
           gpu_buffer_primitive_count_ == objects_.size();
}

// Enhanced GPU primitive management methods
void SceneManager::syncPrimitivesToGPU() {
    if (!gpu_memory_manager_ || gpu_primitive_data_.empty()) {
        return;
    }
    
    resizePrimitiveGPUBufferIfNeeded();
    
    if (!primitive_gpu_buffer_) {
        std::cout << "Warning: No GPU primitive buffer available for sync" << std::endl;
        return;
    }
    
    // Convert GPUPrimitiveData to GPU shader format: 3 vec4s per primitive
    // Format: vec4(pos.xyz, size), vec4(color.rgb, material), vec4(type, padding...)
    std::vector<float> gpu_data;
    gpu_data.reserve(gpu_primitive_data_.size() * 12); // 3 vec4s = 12 floats per primitive
    
    for (const auto& prim_data : gpu_primitive_data_) {
        // vec4: position + size (stored in position[3])
        gpu_data.insert(gpu_data.end(), {
            prim_data.position[0], prim_data.position[1], prim_data.position[2], prim_data.position[3]
        });
        
        // vec4: color + material roughness
        gpu_data.insert(gpu_data.end(), {
            prim_data.color[0], prim_data.color[1], prim_data.color[2], prim_data.material[0]
        });
        
        // vec4: type + material metallic + material emission + padding
        gpu_data.insert(gpu_data.end(), {
            static_cast<float>(prim_data.type), prim_data.material[1], prim_data.material[2], 0.0f
        });
        
        // Debug output
        std::cout << "syncPrimitivesToGPU - Object type=" << prim_data.type << ", size=" << prim_data.position[3]
                  << ", pos=(" << prim_data.position[0] << "," << prim_data.position[1] << "," << prim_data.position[2] << ")" << std::endl;
    }
    
    bool success = gpu_memory_manager_->transferToGPU(
        primitive_gpu_buffer_,
        gpu_data.data(),
        gpu_data.size() * sizeof(float)
    );
    
    if (success) {
        gpu_primitive_data_dirty_ = false;
        std::cout << "Primitives synced to GPU: " << gpu_primitive_data_.size() << " primitives (3 vec4s each)" << std::endl;
    } else {
        std::cout << "Failed to sync primitives to GPU: " << gpu_memory_manager_->getErrorMessage() << std::endl;
    }
}

void SceneManager::updateGPUPrimitive(PrimitiveID id) {
    if (!gpu_memory_manager_ || !primitive_gpu_buffer_) {
        return;
    }
    
    auto it = std::find_if(gpu_primitive_data_.begin(), gpu_primitive_data_.end(),
                          [id](const GPUPrimitiveData& data) { return data.id == id; });
    
    if (it == gpu_primitive_data_.end()) {
        return;
    }
    
    // Update the GPU data for this primitive
    updateGPUPrimitiveData(id);
    
    // For simplicity, re-sync all primitives (a real implementation could optimize this)
    syncPrimitivesToGPU();
}

bool SceneManager::getGPUPrimitiveBuffer() const {
    return primitive_gpu_buffer_ != nullptr;
}

bool SceneManager::isGPUDataValid() const {
    return primitive_gpu_buffer_ && !gpu_primitive_data_dirty_ && 
           !gpu_primitive_data_.empty();
}

// Private helper methods
void SceneManager::markGPUDirty() {
    gpu_synced_ = false;
}

bool SceneManager::validateGPUBufferSize() const {
    if (!scene_gpu_buffer_ || objects_.empty()) {
        return false;
    }
    
    size_t required_size = objects_.size() * 16 * sizeof(float);
    return scene_gpu_buffer_->size >= required_size;
}

void SceneManager::resizeGPUBufferIfNeeded() {
    if (!gpu_memory_manager_ || objects_.empty()) {
        return;
    }
    
    size_t required_size = objects_.size() * 16 * sizeof(float);
    
    // Check if we need a new buffer or can reuse existing one
    if (!scene_gpu_buffer_ || scene_gpu_buffer_->size < required_size) {
        // Deallocate old buffer if it exists
        if (scene_gpu_buffer_) {
            gpu_memory_manager_->deallocateBuffer(scene_gpu_buffer_);
        }
        
        // Allocate new buffer with some extra space to avoid frequent reallocations
        size_t buffer_size = required_size * 1.5; // 50% headroom
        scene_gpu_buffer_ = gpu_memory_manager_->allocateSceneBuffer(
            (buffer_size + 16 * sizeof(float) - 1) / (16 * sizeof(float))
        );
        
        if (!scene_gpu_buffer_) {
            std::cout << "Failed to allocate GPU buffer for scene data" << std::endl;
            return;
        }
        
        markGPUDirty(); // New buffer needs full sync
        
        std::cout << "Allocated new GPU scene buffer: " << buffer_size 
                  << " bytes for " << objects_.size() << " primitives" << std::endl;
    }
}

void SceneManager::resizePrimitiveGPUBufferIfNeeded() {
    if (!gpu_memory_manager_ || gpu_primitive_data_.empty()) {
        return;
    }
    
    // 3 vec4s per primitive = 12 floats per primitive
    size_t required_size = gpu_primitive_data_.size() * 12 * sizeof(float);
    
    if (!primitive_gpu_buffer_ || primitive_gpu_buffer_->size < required_size) {
        if (primitive_gpu_buffer_) {
            gpu_memory_manager_->deallocateBuffer(primitive_gpu_buffer_);
        }
        
        size_t buffer_size = required_size * 1.5; // 50% headroom
        primitive_gpu_buffer_ = gpu_memory_manager_->allocateBuffer(
            buffer_size,
            GPUBufferType::SHADER_STORAGE,
            GPUUsagePattern::DYNAMIC,
            "scene_primitives"
        );
        
        if (!primitive_gpu_buffer_) {
            std::cout << "Failed to allocate GPU primitive buffer" << std::endl;
            return;
        }
        
        gpu_primitive_data_dirty_ = true;
        
        std::cout << "Allocated new GPU primitive buffer: " << buffer_size 
                  << " bytes for " << gpu_primitive_data_.size() << " primitives (12 floats each)" << std::endl;
    }
}
#endif // USE_GPU

