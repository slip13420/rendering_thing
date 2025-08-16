#pragma once

#include "core/common.h"
#include <vector>
#include <memory>
#include <unordered_map>
#include <string>
#include <chrono>

#ifdef USE_GPU
#include <GL/gl.h>
#include <GL/glext.h>
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

// Transfer statistics structure
struct TransferStats {
    size_t total_bytes_transferred = 0;
    double total_time_ms = 0.0;
    size_t transfer_count = 0;
    size_t total_transfers = 0;
    double total_transfer_time_ms = 0.0;
    double peak_transfer_time_ms = 0.0;
    double average_transfer_time_ms = 0.0;
};

struct GPUMemoryStats {
    size_t total_allocated;
    size_t total_used;
    size_t peak_usage;
    size_t buffer_count;
    size_t pool_count;
    float fragmentation_ratio;
    TransferStats transfer_stats;
    
    constexpr GPUMemoryStats() noexcept
        : total_allocated(0)
        , total_used(0)
        , peak_usage(0)
        , buffer_count(0)
        , pool_count(0)
        , fragmentation_ratio(0.0f)
        , transfer_stats()
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
    
    // Memory pool management
    void optimizeMemoryPools();
    void defragmentMemoryPools();
    void returnBufferToPool(std::shared_ptr<GPUBuffer> buffer);
    size_t calculateOptimalPoolSize(GPUBufferType type) const;
    
    // Transfer optimization
    bool transferImageData(std::shared_ptr<GPUBuffer> buffer, const std::vector<Color>& data);
    bool readbackImageData(std::shared_ptr<GPUBuffer> buffer, std::vector<Color>& data);
    bool transferBatched(const std::vector<std::pair<std::shared_ptr<GPUBuffer>, const void*>>& transfers);
    
    // Performance tracking
    void resetTransferStats();
    TransferStats getTransferPerformance() const;
    
private:
    // Pool management methods
    std::shared_ptr<GPUBuffer> allocateFromPool(size_t size, GPUBufferType type, GPUUsagePattern usage);
    std::shared_ptr<GPUBuffer> allocateFromExistingPool(size_t size, GPUBufferType type, GPUUsagePattern usage);
    void createMemoryPool(size_t buffer_size, size_t max_buffers, GPUBufferType type, GPUUsagePattern usage);
    
    // Performance helpers
    void recordTransfer(size_t bytes, double time_ms);
    std::chrono::high_resolution_clock::time_point getTimestamp() const;
    bool createGLBuffer(GPUBuffer& buffer, GPUBufferType type, GPUUsagePattern usage);
    void destroyGLBuffer(GPUBuffer& buffer);
    
#ifdef USE_GPU
    unsigned int getGLTarget(GPUBufferType type) const;
    unsigned int getGLUsage(GPUUsagePattern usage) const;
#endif
    
    void updateStats();
    void trackAllocation(size_t size);
    void trackDeallocation(size_t size);
    void trackBufferAllocation(const std::string& buffer_name);
    void trackBufferDeallocation(const std::string& buffer_name);
    
    bool initialized_;
    bool profiling_enabled_;
    
    std::vector<std::shared_ptr<GPUBuffer>> allocated_buffers_;
    std::unordered_map<std::string, std::weak_ptr<GPUBuffer>> named_buffers_;
    
    GPUMemoryStats stats_;
    std::string last_error_;
    
    // Debug and tracking members
    bool memory_leak_detection_enabled_;
    std::unordered_map<std::string, std::chrono::high_resolution_clock::time_point> allocation_timestamps_;
    
    // Missing member variables that are used in implementation
    TransferStats transfer_stats_;
    
    struct MemoryPool {
        std::vector<std::shared_ptr<GPUBuffer>> free_buffers;
        std::vector<std::shared_ptr<GPUBuffer>> used_buffers;
        size_t buffer_size;
        size_t max_buffers;
        GPUBufferType buffer_type;
        GPUUsagePattern usage_pattern;
        
        MemoryPool(size_t size, size_t max_buf, GPUBufferType buf_type, GPUUsagePattern usage_pat)
            : buffer_size(size), max_buffers(max_buf), buffer_type(buf_type), usage_pattern(usage_pat) {}
    };
    std::unordered_map<size_t, std::unique_ptr<MemoryPool>> memory_pools_;
    
    static constexpr size_t MAX_BUFFER_COUNT = 1024;
    static constexpr size_t MAX_MEMORY_MB = 512;
};