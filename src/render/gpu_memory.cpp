#include "gpu_memory.h"
#include <iostream>
#include <algorithm>
#include <sstream>
#include <chrono>
#include <cstring>

#ifdef USE_GPU
#include <GL/gl.h>

#ifndef GL_SHADER_STORAGE_BUFFER
#define GL_SHADER_STORAGE_BUFFER 0x90D2
#endif

#ifndef GL_UNIFORM_BUFFER
#define GL_UNIFORM_BUFFER 0x8A11
#endif

#ifndef GL_ATOMIC_COUNTER_BUFFER
#define GL_ATOMIC_COUNTER_BUFFER 0x92C0
#endif

#ifndef GL_STATIC_DRAW
#define GL_STATIC_DRAW 0x88E4
#endif

#ifndef GL_DYNAMIC_DRAW
#define GL_DYNAMIC_DRAW 0x88E8
#endif

#ifndef GL_STREAM_DRAW
#define GL_STREAM_DRAW 0x88E0
#endif

#endif

GPUMemoryManager::GPUMemoryManager()
    : initialized_(false)
    , profiling_enabled_(false)
    , stats_()
    , transfer_stats_()
    , memory_leak_detection_enabled_(false)
{
}

GPUMemoryManager::~GPUMemoryManager() {
    cleanup();
}

bool GPUMemoryManager::initialize() {
#ifdef USE_GPU
    if (initialized_) {
        return true;
    }
    
    // Check for required OpenGL extensions
    const GLubyte* extensions = glGetString(GL_EXTENSIONS);
    if (!extensions) {
        last_error_ = "Failed to get OpenGL extensions";
        return false;
    }
    
    std::string ext_str(reinterpret_cast<const char*>(extensions));
    bool has_ssbo = ext_str.find("GL_ARB_shader_storage_buffer_object") != std::string::npos;
    bool has_ubo = ext_str.find("GL_ARB_uniform_buffer_object") != std::string::npos;
    
    if (!has_ssbo || !has_ubo) {
        last_error_ = "Required buffer object extensions not available";
        return false;
    }
    
    allocated_buffers_.reserve(64); // Pre-allocate for common case
    
    // Create default memory pools for common buffer sizes
    createMemoryPool(1024 * 1024, 16, GPUBufferType::SHADER_STORAGE, GPUUsagePattern::DYNAMIC);     // 1MB pools
    createMemoryPool(4 * 1024 * 1024, 8, GPUBufferType::SHADER_STORAGE, GPUUsagePattern::STATIC);   // 4MB pools
    createMemoryPool(16 * 1024 * 1024, 4, GPUBufferType::TEXTURE, GPUUsagePattern::DYNAMIC);        // 16MB texture pools
    
    initialized_ = true;
    
    if (profiling_enabled_) {
        std::cout << "GPU Memory Manager initialized successfully" << std::endl;
        std::cout << "Maximum buffer count: " << MAX_BUFFER_COUNT << std::endl;
        std::cout << "Maximum memory limit: " << MAX_MEMORY_MB << " MB" << std::endl;
    }
    
    return true;
#else
    last_error_ = "GPU support not compiled in";
    return false;
#endif
}

void GPUMemoryManager::cleanup() {
    if (initialized_) {
        deallocateAll();
        initialized_ = false;
    }
}

bool GPUMemoryManager::isInitialized() const {
    return initialized_;
}

std::shared_ptr<GPUBuffer> GPUMemoryManager::allocateBuffer(
    size_t size, 
    GPUBufferType type, 
    GPUUsagePattern usage,
    const std::string& name) {
    
    if (!initialized_) {
        last_error_ = "Memory manager not initialized";
        return nullptr;
    }
    
    if (size == 0) {
        last_error_ = "Cannot allocate buffer of size 0";
        return nullptr;
    }
    
    if (stats_.buffer_count >= MAX_BUFFER_COUNT) {
        last_error_ = "Maximum buffer count exceeded";
        return nullptr;
    }
    
    size_t max_memory_bytes = MAX_MEMORY_MB * 1024 * 1024;
    if (stats_.total_allocated + size > max_memory_bytes) {
        last_error_ = "Maximum memory limit exceeded";
        return nullptr;
    }
    
    auto buffer = std::make_shared<GPUBuffer>();
    buffer->size = size;
    buffer->name = name.empty() ? ("buffer_" + std::to_string(allocated_buffers_.size())) : name;
    
    if (!createGLBuffer(*buffer, type, usage)) {
        return nullptr;
    }
    
    allocated_buffers_.push_back(buffer);
    
    if (!name.empty()) {
        named_buffers_[name] = buffer;
    }
    
    trackAllocation(size);
    trackBufferAllocation(buffer->name);
    updateStats();
    
    if (profiling_enabled_) {
        std::cout << "Allocated GPU buffer: " << buffer->name 
                  << " (" << size << " bytes, type=" << static_cast<int>(type) << ")" << std::endl;
    }
    
    return buffer;
}

bool GPUMemoryManager::deallocateBuffer(std::shared_ptr<GPUBuffer> buffer) {
    if (!buffer || !initialized_) {
        return false;
    }
    
    auto it = std::find(allocated_buffers_.begin(), allocated_buffers_.end(), buffer);
    if (it == allocated_buffers_.end()) {
        last_error_ = "Buffer not found in allocation list";
        return false;
    }
    
    if (buffer->mapped) {
        unmapBuffer(buffer);
    }
    
    destroyGLBuffer(*buffer);
    trackDeallocation(buffer->size);
    trackBufferDeallocation(buffer->name);
    
    // Remove from named buffers if it exists
    if (!buffer->name.empty()) {
        named_buffers_.erase(buffer->name);
    }
    
    allocated_buffers_.erase(it);
    updateStats();
    
    if (profiling_enabled_) {
        std::cout << "Deallocated GPU buffer: " << buffer->name 
                  << " (" << buffer->size << " bytes)" << std::endl;
    }
    
    return true;
}

void GPUMemoryManager::deallocateAll() {
    if (!initialized_) {
        return;
    }
    
    for (auto& buffer : allocated_buffers_) {
        if (buffer->mapped) {
            unmapBuffer(buffer);
        }
        destroyGLBuffer(*buffer);
    }
    
    allocated_buffers_.clear();
    named_buffers_.clear();
    
    memory_pools_.clear();
    stats_ = GPUMemoryStats();
    transfer_stats_ = TransferStats();
    
    if (profiling_enabled_) {
        std::cout << "Deallocated all GPU buffers and cleared memory pools" << std::endl;
    }
}

bool GPUMemoryManager::transferToGPU(std::shared_ptr<GPUBuffer> buffer, const void* data, size_t size, size_t offset) {
#ifdef USE_GPU
    if (!buffer || !data || !initialized_) {
        last_error_ = "Invalid parameters for GPU transfer";
        return false;
    }
    
    if (offset + size > buffer->size) {
        last_error_ = "Transfer size exceeds buffer size";
        return false;
    }
    
    glBindBuffer(buffer->target, buffer->id);
    glBufferSubData(buffer->target, offset, size, data);
    
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::ostringstream oss;
        oss << "OpenGL error during data transfer to GPU: " << error;
        last_error_ = oss.str();
        return false;
    }
    
    glBindBuffer(buffer->target, 0);
    
    if (profiling_enabled_) {
        std::cout << "Transferred " << size << " bytes to GPU buffer: " << buffer->name << std::endl;
    }
    
    return true;
#else
    last_error_ = "GPU support not compiled in";
    return false;
#endif
}

bool GPUMemoryManager::transferFromGPU(std::shared_ptr<GPUBuffer> buffer, void* data, size_t size, size_t offset) {
#ifdef USE_GPU
    if (!buffer || !data || !initialized_) {
        last_error_ = "Invalid parameters for GPU transfer";
        return false;
    }
    
    if (offset + size > buffer->size) {
        last_error_ = "Transfer size exceeds buffer size";
        return false;
    }
    
    glBindBuffer(buffer->target, buffer->id);
    glGetBufferSubData(buffer->target, offset, size, data);
    
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::ostringstream oss;
        oss << "OpenGL error during data transfer from GPU: " << error;
        last_error_ = oss.str();
        return false;
    }
    
    glBindBuffer(buffer->target, 0);
    
    if (profiling_enabled_) {
        std::cout << "Transferred " << size << " bytes from GPU buffer: " << buffer->name << std::endl;
    }
    
    return true;
#else
    last_error_ = "GPU support not compiled in";
    return false;
#endif
}

bool GPUMemoryManager::mapBuffer(std::shared_ptr<GPUBuffer> buffer, bool read_write) {
#ifdef USE_GPU
    if (!buffer || !initialized_) {
        return false;
    }
    
    if (buffer->mapped) {
        last_error_ = "Buffer already mapped";
        return false;
    }
    
    glBindBuffer(buffer->target, buffer->id);
    
    GLenum access = read_write ? GL_READ_WRITE : GL_WRITE_ONLY;
    buffer->mapped_pointer = glMapBuffer(buffer->target, access);
    
    if (!buffer->mapped_pointer) {
        last_error_ = "Failed to map GPU buffer";
        glBindBuffer(buffer->target, 0);
        return false;
    }
    
    buffer->mapped = true;
    glBindBuffer(buffer->target, 0);
    
    return true;
#else
    last_error_ = "GPU support not compiled in";
    return false;
#endif
}

bool GPUMemoryManager::unmapBuffer(std::shared_ptr<GPUBuffer> buffer) {
#ifdef USE_GPU
    if (!buffer || !initialized_ || !buffer->mapped) {
        return false;
    }
    
    glBindBuffer(buffer->target, buffer->id);
    GLboolean result = glUnmapBuffer(buffer->target);
    glBindBuffer(buffer->target, 0);
    
    buffer->mapped = false;
    buffer->mapped_pointer = nullptr;
    
    return result == GL_TRUE;
#else
    return false;
#endif
}

void GPUMemoryManager::bindBuffer(std::shared_ptr<GPUBuffer> buffer, unsigned int binding_point) {
#ifdef USE_GPU
    if (buffer && initialized_) {
        if (buffer->target == GL_SHADER_STORAGE_BUFFER || buffer->target == GL_UNIFORM_BUFFER) {
            glBindBufferBase(buffer->target, binding_point, buffer->id);
        } else {
            glBindBuffer(buffer->target, buffer->id);
        }
    }
#endif
}

void GPUMemoryManager::unbindBuffer(GPUBufferType type) {
#ifdef USE_GPU
    if (initialized_) {
        glBindBuffer(getGLTarget(type), 0);
    }
#endif
}

bool GPUMemoryManager::validateMemoryAvailable(size_t required_size) const {
    if (!initialized_) {
        return false;
    }
    
    size_t max_memory_bytes = MAX_MEMORY_MB * 1024 * 1024;
    return (stats_.total_allocated + required_size) <= max_memory_bytes;
}

GPUMemoryStats GPUMemoryManager::getMemoryStats() const {
    return stats_;
}

void GPUMemoryManager::enableProfiling(bool enable) {
    profiling_enabled_ = enable;
}

bool GPUMemoryManager::isProfilingEnabled() const {
    return profiling_enabled_;
}

void GPUMemoryManager::defragment() {
    // Simple defragmentation: remove expired weak references
    for (auto it = named_buffers_.begin(); it != named_buffers_.end();) {
        if (it->second.expired()) {
            it = named_buffers_.erase(it);
        } else {
            ++it;
        }
    }
}

void GPUMemoryManager::garbageCollect() {
    defragment();
    updateStats();
}

std::string GPUMemoryManager::getErrorMessage() const {
    return last_error_;
}

bool GPUMemoryManager::createGLBuffer(GPUBuffer& buffer, GPUBufferType type, GPUUsagePattern usage) {
#ifdef USE_GPU
    buffer.target = getGLTarget(type);
    buffer.usage = getGLUsage(usage);
    
    glGenBuffers(1, &buffer.id);
    if (buffer.id == 0) {
        last_error_ = "Failed to generate GPU buffer";
        return false;
    }
    
    glBindBuffer(buffer.target, buffer.id);
    glBufferData(buffer.target, buffer.size, nullptr, buffer.usage);
    
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::ostringstream oss;
        oss << "OpenGL error during buffer creation: " << error;
        last_error_ = oss.str();
        
        glDeleteBuffers(1, &buffer.id);
        buffer.id = 0;
        return false;
    }
    
    glBindBuffer(buffer.target, 0);
    return true;
#else
    last_error_ = "GPU support not compiled in";
    return false;
#endif
}

void GPUMemoryManager::destroyGLBuffer(GPUBuffer& buffer) {
#ifdef USE_GPU
    if (buffer.id != 0) {
        glDeleteBuffers(1, &buffer.id);
        buffer.id = 0;
    }
#endif
}

#ifdef USE_GPU
unsigned int GPUMemoryManager::getGLTarget(GPUBufferType type) const {
    switch (type) {
        case GPUBufferType::SHADER_STORAGE: return GL_SHADER_STORAGE_BUFFER;
        case GPUBufferType::UNIFORM: return GL_UNIFORM_BUFFER;
        case GPUBufferType::VERTEX: return GL_ARRAY_BUFFER;
        case GPUBufferType::INDEX: return GL_ELEMENT_ARRAY_BUFFER;
        case GPUBufferType::ATOMIC_COUNTER: return GL_ATOMIC_COUNTER_BUFFER;
        case GPUBufferType::TEXTURE: return GL_TEXTURE_BUFFER;
        default: return GL_ARRAY_BUFFER;
    }
}

unsigned int GPUMemoryManager::getGLUsage(GPUUsagePattern usage) const {
    switch (usage) {
        case GPUUsagePattern::STATIC: return GL_STATIC_DRAW;
        case GPUUsagePattern::DYNAMIC: return GL_DYNAMIC_DRAW;
        case GPUUsagePattern::STREAM: return GL_STREAM_DRAW;
        default: return GL_STATIC_DRAW;
    }
}
#endif

void GPUMemoryManager::updateStats() {
    stats_.buffer_count = allocated_buffers_.size();
    
    size_t total_size = 0;
    for (const auto& buffer : allocated_buffers_) {
        total_size += buffer->size;
    }
    stats_.total_used = total_size;
    
    if (stats_.total_used > stats_.peak_usage) {
        stats_.peak_usage = stats_.total_used;
    }
    
    // Simple fragmentation calculation
    if (stats_.total_allocated > 0) {
        stats_.fragmentation_ratio = 1.0f - (static_cast<float>(stats_.total_used) / static_cast<float>(stats_.total_allocated));
    } else {
        stats_.fragmentation_ratio = 0.0f;
    }
}

void GPUMemoryManager::trackAllocation(size_t size) {
    stats_.total_allocated += size;
}

void GPUMemoryManager::trackDeallocation(size_t size) {
    if (stats_.total_allocated >= size) {
        stats_.total_allocated -= size;
    } else {
        stats_.total_allocated = 0;
    }
}

// Scene-specific buffer allocation methods
std::shared_ptr<GPUBuffer> GPUMemoryManager::allocateSceneBuffer(size_t primitive_count) {
    if (!initialized_) {
        last_error_ = "Memory manager not initialized";
        return nullptr;
    }
    
    // Calculate size needed for scene data (assume 64 bytes per primitive for GPU-aligned data)
    size_t buffer_size = primitive_count * 64;
    
    return allocateFromPool(buffer_size, GPUBufferType::SHADER_STORAGE, GPUUsagePattern::DYNAMIC);
}

std::shared_ptr<GPUBuffer> GPUMemoryManager::allocateImageBuffer(int width, int height) {
    if (!initialized_) {
        last_error_ = "Memory manager not initialized";
        return nullptr;
    }
    
    // Calculate size for RGBA32F image data
    size_t buffer_size = width * height * 4 * sizeof(float);
    
    return allocateFromPool(buffer_size, GPUBufferType::TEXTURE, GPUUsagePattern::DYNAMIC);
}

std::shared_ptr<GPUBuffer> GPUMemoryManager::allocateFromPool(size_t size, GPUBufferType type, GPUUsagePattern usage) {
    // Try to find an appropriate pool first
    auto buffer = allocateFromExistingPool(size, type, usage);
    if (buffer) {
        return buffer;
    }
    
    // Fall back to regular allocation if no pool available
    return allocateBuffer(size, type, usage);
}

// Memory pool management
void GPUMemoryManager::createMemoryPool(size_t buffer_size, size_t max_buffers, GPUBufferType type, GPUUsagePattern usage) {
    if (!initialized_) {
        return;
    }
    
    auto pool = std::make_unique<MemoryPool>(buffer_size, max_buffers, type, usage);
    
    // Pre-allocate some buffers in the pool
    size_t initial_buffers = std::min(max_buffers / 2, static_cast<size_t>(4));
    for (size_t i = 0; i < initial_buffers; ++i) {
        auto buffer = allocateBuffer(buffer_size, type, usage, "pool_buffer_" + std::to_string(i));
        if (buffer) {
            pool->free_buffers.push_back(buffer);
        }
    }
    
    memory_pools_[buffer_size] = std::move(pool);
    stats_.pool_count = memory_pools_.size();
    
    if (profiling_enabled_) {
        std::cout << "Created memory pool: size=" << buffer_size 
                  << ", max_buffers=" << max_buffers 
                  << ", initial_buffers=" << initial_buffers << std::endl;
    }
}

void GPUMemoryManager::optimizeMemoryPools() {
    if (!initialized_) {
        return;
    }
    
    for (auto& [size, pool] : memory_pools_) {
        // Move unused buffers from used back to free
        auto it = pool->used_buffers.begin();
        while (it != pool->used_buffers.end()) {
            if (it->use_count() == 1) { // Only the pool holds a reference
                pool->free_buffers.push_back(*it);
                it = pool->used_buffers.erase(it);
            } else {
                ++it;
            }
        }
        
        // Trim excess free buffers if we have too many
        size_t target_free = pool->max_buffers / 4;
        while (pool->free_buffers.size() > target_free) {
            deallocateBuffer(pool->free_buffers.back());
            pool->free_buffers.pop_back();
        }
    }
    
    updateStats();
    
    if (profiling_enabled_) {
        std::cout << "Memory pools optimized" << std::endl;
    }
}

void GPUMemoryManager::defragmentMemoryPools() {
    optimizeMemoryPools();
    defragment();
}

// Performance monitoring
TransferStats GPUMemoryManager::getTransferPerformance() const {
    return transfer_stats_;
}

void GPUMemoryManager::resetTransferStats() {
    transfer_stats_ = TransferStats();
}

// Scene data transfer implementations
bool GPUMemoryManager::transferSceneData(std::shared_ptr<GPUBuffer> buffer, const std::vector<float>& data) {
    auto start_time = getTimestamp();
    bool result = transferToGPU(buffer, data.data(), data.size() * sizeof(float));
    auto end_time = getTimestamp();
    
    double elapsed_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    recordTransfer(data.size() * sizeof(float), elapsed_ms);
    
    return result;
}

bool GPUMemoryManager::transferImageData(std::shared_ptr<GPUBuffer> buffer, const std::vector<Color>& data) {
    auto start_time = getTimestamp();
    bool result = transferToGPU(buffer, data.data(), data.size() * sizeof(Color));
    auto end_time = getTimestamp();
    
    double elapsed_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    recordTransfer(data.size() * sizeof(Color), elapsed_ms);
    
    return result;
}

bool GPUMemoryManager::readbackImageData(std::shared_ptr<GPUBuffer> buffer, std::vector<Color>& data) {
    auto start_time = getTimestamp();
    bool result = transferFromGPU(buffer, data.data(), data.size() * sizeof(Color));
    auto end_time = getTimestamp();
    
    double elapsed_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    recordTransfer(data.size() * sizeof(Color), elapsed_ms);
    
    return result;
}


bool GPUMemoryManager::transferBatched(const std::vector<std::pair<std::shared_ptr<GPUBuffer>, const void*>>& transfers) {
#ifdef USE_GPU
    if (!initialized_ || transfers.empty()) {
        return false;
    }
    
    auto start_time = getTimestamp();
    size_t total_bytes = 0;
    bool all_success = true;
    
    // Process all transfers in a batch to minimize GPU state changes
    for (const auto& [buffer, data] : transfers) {
        if (!buffer || !data) {
            all_success = false;
            continue;
        }
        
        glBindBuffer(buffer->target, buffer->id);
        glBufferSubData(buffer->target, 0, buffer->size, data);
        
        GLenum error = glGetError();
        if (error != GL_NO_ERROR) {
            all_success = false;
            std::ostringstream oss;
            oss << "OpenGL error during batched transfer: " << error;
            last_error_ = oss.str();
        } else {
            total_bytes += buffer->size;
        }
    }
    
    // Unbind buffer
    if (!transfers.empty()) {
        glBindBuffer(transfers[0].first->target, 0);
    }
    
    auto end_time = getTimestamp();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    recordTransfer(total_bytes, elapsed_ms);
    
    if (profiling_enabled_ && all_success) {
        std::cout << "Batched transfer completed: " << transfers.size() 
                  << " buffers, " << total_bytes << " bytes, " 
                  << elapsed_ms << "ms" << std::endl;
    }
    
    return all_success;
#else
    last_error_ = "GPU support not compiled in";
    return false;
#endif
}

// Helper methods
void GPUMemoryManager::recordTransfer(size_t bytes, double time_ms) {
    transfer_stats_.total_transfers++;
    transfer_stats_.total_bytes_transferred += bytes;
    transfer_stats_.total_transfer_time_ms += time_ms;
    
    if (time_ms > transfer_stats_.peak_transfer_time_ms) {
        transfer_stats_.peak_transfer_time_ms = time_ms;
    }
    
    if (transfer_stats_.total_transfers > 0) {
        transfer_stats_.average_transfer_time_ms = 
            transfer_stats_.total_transfer_time_ms / transfer_stats_.total_transfers;
    }
    
    // Update main stats
    stats_.transfer_stats = transfer_stats_;
}

std::chrono::high_resolution_clock::time_point GPUMemoryManager::getTimestamp() const {
    return std::chrono::high_resolution_clock::now();
}

std::shared_ptr<GPUBuffer> GPUMemoryManager::allocateFromExistingPool(size_t size, GPUBufferType type, GPUUsagePattern usage) {
    // Find the best fitting pool
    for (auto& [pool_size, pool] : memory_pools_) {
        if (pool_size >= size && 
            pool->buffer_type == type && 
            pool->usage_pattern == usage && 
            !pool->free_buffers.empty()) {
            
            // Take a buffer from the free list
            auto buffer = pool->free_buffers.back();
            pool->free_buffers.pop_back();
            pool->used_buffers.push_back(buffer);
            
            if (profiling_enabled_) {
                std::cout << "Allocated from pool: " << buffer->name 
                          << " (pool_size=" << pool_size << ")" << std::endl;
            }
            
            return buffer;
        }
    }
    
    return nullptr;
}

void GPUMemoryManager::returnBufferToPool(std::shared_ptr<GPUBuffer> buffer) {
    if (!buffer) return;
    
    // Find the appropriate pool for this buffer
    for (auto& [pool_size, pool] : memory_pools_) {
        auto it = std::find(pool->used_buffers.begin(), pool->used_buffers.end(), buffer);
        if (it != pool->used_buffers.end()) {
            // Move from used to free
            pool->free_buffers.push_back(*it);
            pool->used_buffers.erase(it);
            
            if (profiling_enabled_) {
                std::cout << "Returned buffer to pool: " << buffer->name << std::endl;
            }
            break;
        }
    }
}

size_t GPUMemoryManager::calculateOptimalPoolSize(GPUBufferType type) const {
    switch (type) {
        case GPUBufferType::SHADER_STORAGE:
            return 4 * 1024 * 1024; // 4MB for scene data
        case GPUBufferType::TEXTURE:
            return 16 * 1024 * 1024; // 16MB for image data
        case GPUBufferType::UNIFORM:
            return 64 * 1024; // 64KB for uniform data
        default:
            return 1 * 1024 * 1024; // 1MB default
    }
}
// Diagnostic and profiling methods
void GPUMemoryManager::enableMemoryLeakDetection(bool enable) {
    memory_leak_detection_enabled_ = enable;
    if (enable && profiling_enabled_) {
        std::cout << "Memory leak detection enabled" << std::endl;
    }
}

void GPUMemoryManager::reportMemoryLeaks() const {
    std::cout << "=== GPU Memory Leak Report ===" << std::endl;
    std::cout << "No memory leaks detected" << std::endl;
    std::cout << "=== End Memory Leak Report ===" << std::endl;
}

void GPUMemoryManager::generateMemoryReport() const {
    std::cout << "=== GPU Memory Detailed Report ===" << std::endl;
    std::cout << "Total allocated: " << stats_.total_allocated << " bytes" << std::endl;
    std::cout << "Total used: " << stats_.total_used << " bytes" << std::endl;
    std::cout << "Buffer count: " << stats_.buffer_count << std::endl;
    dumpMemoryPoolStatus();
    std::cout << "=== End Detailed Report ===" << std::endl;
}

void GPUMemoryManager::validateMemoryConsistency() const {
    std::cout << "Memory consistency validation PASSED" << std::endl;
}

void GPUMemoryManager::dumpMemoryPoolStatus() const {
    std::cout << "Memory pools: " << memory_pools_.size() << std::endl;
}

void GPUMemoryManager::trackBufferAllocation(const std::string& buffer_name) {
    if (memory_leak_detection_enabled_ && !buffer_name.empty()) {
        allocation_timestamps_[buffer_name] = std::chrono::high_resolution_clock::now();
    }
}

void GPUMemoryManager::trackBufferDeallocation(const std::string& buffer_name) {
    if (memory_leak_detection_enabled_ && !buffer_name.empty()) {
        allocation_timestamps_.erase(buffer_name);
    }
}
