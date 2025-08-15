#include "gpu_memory.h"
#include <iostream>
#include <algorithm>
#include <sstream>

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
    
    stats_ = GPUMemoryStats();
    
    if (profiling_enabled_) {
        std::cout << "Deallocated all GPU buffers" << std::endl;
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