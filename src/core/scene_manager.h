#pragma once

#include "common.h"
#include "primitives.h"
#include <vector>
#include <memory>
#include <unordered_map>
#include <algorithm>

// Forward declarations
class Camera;
class GPUMemoryManager;
struct GPUBuffer;

// Primitive ID type for tracking individual instances
using PrimitiveID = uint32_t;
constexpr PrimitiveID INVALID_PRIMITIVE_ID = 0;

// Primitive types for GPU-friendly identification
enum class PrimitiveType : uint32_t {
    SPHERE = 1,
    CUBE = 2,
    TORUS = 3,
    PYRAMID = 4
};

// GPU-friendly primitive data structure
struct GPUPrimitiveData {
    float position[4];      // Aligned for GPU (xyz + padding)
    float rotation[4];      // Quaternion representation  
    float scale[4];         // Uniform scaling with padding
    uint32_t type;          // Primitive type enum
    uint32_t id;            // Unique primitive identifier
    float color[4];         // RGBA color data
    float material[4];      // Material properties (roughness, metallic, emission, padding)
    
    GPUPrimitiveData() {
        std::fill(std::begin(position), std::end(position), 0.0f);
        std::fill(std::begin(rotation), std::end(rotation), 0.0f);
        std::fill(std::begin(scale), std::end(scale), 1.0f);
        type = 0; id = 0;
        std::fill(std::begin(color), std::end(color), 1.0f);
        std::fill(std::begin(material), std::end(material), 0.0f);
    }
};

class SceneManager {
public:
    SceneManager();
    ~SceneManager();
    
    void initialize();
    void update();
    void shutdown();
    
    // Enhanced primitive management with GPU acceleration (AC: 1, 2, 4, 5, 7)
    PrimitiveID addPrimitive(PrimitiveType type, const Vector3& position = Vector3(0,0,0), 
                           const Color& color = Color::white(), const Material& material = Material());
    bool removePrimitive(PrimitiveID id);
    std::shared_ptr<Primitive> getPrimitive(PrimitiveID id) const;
    
    // Legacy object management (for backward compatibility)
    void add_object(std::shared_ptr<Primitive> object);
    void remove_object(std::shared_ptr<Primitive> object);
    void clear_objects();
    const std::vector<std::shared_ptr<Primitive>>& get_objects() const;
    
    // GPU memory integration with enhanced primitive support
    void setGPUMemoryManager(std::shared_ptr<GPUMemoryManager> gpu_memory);
    void syncPrimitivesToGPU();
    void updateGPUPrimitive(PrimitiveID id);
    bool getGPUPrimitiveBuffer() const;
    bool isGPUDataValid() const;
    
    // Legacy GPU methods (maintained for compatibility)
    void syncSceneToGPU();
    void updateGPUPrimitive(size_t primitive_index);
    void removeGPUPrimitive(size_t primitive_index);
    std::shared_ptr<GPUBuffer> getSceneGPUBuffer() const;
    bool isGPUSynced() const;
    
    // Light source management  
    void add_light(const Vector3& position, const Color& color, float intensity);
    void clear_lights();
    const std::vector<std::shared_ptr<Primitive>>& get_lights() const;
    
    // Scene queries for path tracer
    bool hit_scene(const Ray& ray, float t_min, float t_max, HitRecord& rec) const;
    Color get_background_color(const Ray& ray) const;
    
    // Camera management
    void set_camera(std::shared_ptr<Camera> camera);
    std::shared_ptr<Camera> get_camera() const;
    Vector3 get_camera_position() const;
    void set_camera_position(const Vector3& position);
    void set_camera_target(const Vector3& target);
    void set_camera_up(const Vector3& up);
    bool is_valid_camera_position(const Vector3& position) const;
    
private:
    bool initialized_;
    std::vector<std::shared_ptr<Primitive>> objects_;
    std::vector<std::shared_ptr<Primitive>> lights_;
    std::shared_ptr<Camera> camera_;
    
    // Enhanced primitive tracking with ID system  
    std::unordered_map<PrimitiveID, std::shared_ptr<Primitive>> primitives_by_id_;
    std::unordered_map<std::shared_ptr<Primitive>, PrimitiveID> primitive_ids_;
    PrimitiveID next_primitive_id_;
    std::vector<GPUPrimitiveData> gpu_primitive_data_;
    bool gpu_primitive_data_dirty_;
    
    // GPU memory coordination
    std::shared_ptr<GPUMemoryManager> gpu_memory_manager_;
    std::shared_ptr<GPUBuffer> scene_gpu_buffer_;
    std::shared_ptr<GPUBuffer> primitive_gpu_buffer_;
    bool gpu_synced_;
    size_t gpu_buffer_primitive_count_;
    
    void setup_default_scene();
    void create_default_camera();
    void markGPUDirty();
    void markPrimitiveGPUDirty();
    bool validateGPUBufferSize() const;
    void resizeGPUBufferIfNeeded();
    void resizePrimitiveGPUBufferIfNeeded();
    
    // Helper methods for primitive management
    PrimitiveID generatePrimitiveID();
    std::shared_ptr<Primitive> createPrimitive(PrimitiveType type, const Vector3& position, 
                                             const Color& color, const Material& material);
    void updateGPUPrimitiveData(PrimitiveID id);
    void removeFromGPUData(PrimitiveID id);
    
    
};