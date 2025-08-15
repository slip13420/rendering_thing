#pragma once

#include "core/common.h"
#include <vector>
#include <memory>
#include <unordered_map>
#include <string>

#ifdef USE_GPU
#include <GL/gl.h>
#endif

struct GPUBuffer {
#ifdef USE_GPU
    unsigned int id;
    unsigned int target;
    unsigned int usage;
#endif
    size_t size;
    bool mapped;
    void* mapped_pointer;
    std::string name;
    
    GPUBuffer() {
#ifdef USE_GPU
        id = 0; target = 0; usage = 0;
#endif
        size = 0; mapped = false; mapped_pointer = nullptr;
    }
};

enum class GPUBufferType {
    SHADER_STORAGE,
    UNIFORM,
    VERTEX,
    INDEX,
    ATOMIC_COUNTER,
    TEXTURE
};

enum class GPUUsagePattern {
    STATIC,         // Data set once, used many times
    DYNAMIC,        // Data modified frequently
    STREAM          // Data set once, used few times
};

struct GPUMemoryStats {
    size_t total_allocated;
    size_t total_used;
    size_t peak_usage;
    size_t buffer_count;
    float fragmentation_ratio;
    
    constexpr GPUMemoryStats() noexcept
        : total_allocated(0)
        , total_used(0)
        , peak_usage(0)
        , buffer_count(0)
        , fragmentation_ratio(0.0f)
    {}
};

class GPUMemoryManager {
public:
    GPUMemoryManager();
    ~GPUMemoryManager();
    
    bool initialize();
    void cleanup();
    bool isInitialized() const;
    
    std::shared_ptr<GPUBuffer> allocateBuffer(
        size_t size, 
        GPUBufferType type, 
        GPUUsagePattern usage = GPUUsagePattern::STATIC,
        const std::string& name = ""
    );
    
    
    // Scene-specific buffer allocation
    std::shared_ptr<GPUBuffer> allocateSceneBuffer(size_t primitive_count);
    std::shared_ptr<GPUBuffer> allocateImageBuffer(int width, int height);
    bool deallocateBuffer(std::shared_ptr<GPUBuffer> buffer);
    void deallocateAll();
    
    bool transferToGPU(std::shared_ptr<GPUBuffer> buffer, const void* data, size_t size, size_t offset = 0);
    bool transferFromGPU(std::shared_ptr<GPUBuffer> buffer, void* data, size_t size, size_t offset = 0);
    
    // Scene-specific transfers
    bool transferSceneData(std::shared_ptr<GPUBuffer> buffer, const std::vector<float>& data);
    
    bool mapBuffer(std::shared_ptr<GPUBuffer> buffer, bool read_write = false);
    bool unmapBuffer(std::shared_ptr<GPUBuffer> buffer);
    
    void bindBuffer(std::shared_ptr<GPUBuffer> buffer, unsigned int binding_point = 0);
    void unbindBuffer(GPUBufferType type);
    
    bool validateMemoryAvailable(size_t required_size) const;
    GPUMemoryStats getMemoryStats() const;
    
    void enableProfiling(bool enable = true);
    bool isProfilingEnabled() const;
    
    void defragment();
    void garbageCollect();
    
    std::string getErrorMessage() const;
    
private:
    bool createGLBuffer(GPUBuffer& buffer, GPUBufferType type, GPUUsagePattern usage);
    void destroyGLBuffer(GPUBuffer& buffer);
    
#ifdef USE_GPU
    unsigned int getGLTarget(GPUBufferType type) const;
    unsigned int getGLUsage(GPUUsagePattern usage) const;
#endif
    
    void updateStats();
    void trackAllocation(size_t size);
    void trackDeallocation(size_t size);
    
    bool initialized_;
    bool profiling_enabled_;
    
    std::vector<std::shared_ptr<GPUBuffer>> allocated_buffers_;
    std::unordered_map<std::string, std::weak_ptr<GPUBuffer>> named_buffers_;
    
    GPUMemoryStats stats_;
    std::string last_error_;
    
    static constexpr size_t MAX_BUFFER_COUNT = 1024;
    static constexpr size_t MAX_MEMORY_MB = 512;
};