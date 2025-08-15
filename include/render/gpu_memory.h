#pragma once

#include "core/common.h"
#include <vector>
#include <memory>
#include <unordered_map>
#include <map>
#include <string>
#include <chrono>

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
    
    GPUBuffer() 
#ifdef USE_GPU
        : id(0), target(0), usage(0)
#endif
        , size(0), mapped(false), mapped_pointer(nullptr) {}
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

struct MemoryPool {
    std::vector<std::shared_ptr<GPUBuffer>> free_buffers;
    std::vector<std::shared_ptr<GPUBuffer>> used_buffers;
    size_t buffer_size;
    size_t max_buffers;
    GPUBufferType buffer_type;
    GPUUsagePattern usage_pattern;
    
    MemoryPool(size_t size, size_t max_count, GPUBufferType type, GPUUsagePattern usage)
        : buffer_size(size), max_buffers(max_count), buffer_type(type), usage_pattern(usage) {}
};

struct TransferStats {
    size_t total_transfers;
    size_t total_bytes_transferred;
    double average_transfer_time_ms;
    double peak_transfer_time_ms;
    double total_transfer_time_ms;
    
    constexpr TransferStats() noexcept
        : total_transfers(0)
        , total_bytes_transferred(0)
        , average_transfer_time_ms(0.0)
        , peak_transfer_time_ms(0.0)
        , total_transfer_time_ms(0.0)
    {}
};

struct GPUMemoryStats {
    size_t total_allocated;
    size_t total_used;
    size_t peak_usage;
    size_t buffer_count;
    float fragmentation_ratio;
    size_t pool_count;
    TransferStats transfer_stats;
    
    constexpr GPUMemoryStats() noexcept
        : total_allocated(0)
        , total_used(0)
        , peak_usage(0)
        , buffer_count(0)
        , fragmentation_ratio(0.0f)
        , pool_count(0)
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
    std::shared_ptr<GPUBuffer> allocateFromPool(size_t size, GPUBufferType type, GPUUsagePattern usage);
    
    bool deallocateBuffer(std::shared_ptr<GPUBuffer> buffer);
    void deallocateAll();
    
    bool transferToGPU(std::shared_ptr<GPUBuffer> buffer, const void* data, size_t size, size_t offset = 0);
    bool transferFromGPU(std::shared_ptr<GPUBuffer> buffer, void* data, size_t size, size_t offset = 0);
    
    // Scene-specific transfers
    bool transferSceneData(std::shared_ptr<GPUBuffer> buffer, const std::vector<float>& data);
    bool transferImageData(std::shared_ptr<GPUBuffer> buffer, const std::vector<Color>& data);
    bool readbackImageData(std::shared_ptr<GPUBuffer> buffer, std::vector<Color>& data);
    
    // Batched transfers
    bool transferBatched(const std::vector<std::pair<std::shared_ptr<GPUBuffer>, const void*>>& transfers);
    
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
    
    // Memory pool management
    void createMemoryPool(size_t buffer_size, size_t max_buffers, GPUBufferType type, GPUUsagePattern usage);
    void optimizeMemoryPools();
    void defragmentMemoryPools();
    
    // Performance monitoring
    TransferStats getTransferPerformance() const;
    void resetTransferStats();
    
    // Diagnostic and profiling features
    void enableMemoryLeakDetection(bool enable = true);
    void reportMemoryLeaks() const;
    void generateMemoryReport() const;
    void validateMemoryConsistency() const;
    void dumpMemoryPoolStatus() const;
    
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
    std::map<size_t, std::unique_ptr<MemoryPool>> memory_pools_;
    
    GPUMemoryStats stats_;
    TransferStats transfer_stats_;
    std::string last_error_;
    
    // Performance monitoring
    void recordTransfer(size_t bytes, double time_ms);
    std::chrono::high_resolution_clock::time_point getTimestamp() const;
    
    // Pool management helpers
    std::shared_ptr<GPUBuffer> allocateFromExistingPool(size_t size, GPUBufferType type, GPUUsagePattern usage);
    void returnBufferToPool(std::shared_ptr<GPUBuffer> buffer);
    size_t calculateOptimalPoolSize(GPUBufferType type) const;
    
    // Diagnostic helpers
    bool memory_leak_detection_enabled_;
    std::map<std::string, std::chrono::high_resolution_clock::time_point> allocation_timestamps_;
    void trackBufferAllocation(const std::string& buffer_name);
    void trackBufferDeallocation(const std::string& buffer_name);
    
    static constexpr size_t MAX_BUFFER_COUNT = 1024;
    static constexpr size_t MAX_MEMORY_MB = 512;
};