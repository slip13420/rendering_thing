#include "gpu_compute.h"
#include <iostream>
#include <sstream>

#ifdef USE_GPU
#include <GL/gl.h>

#ifndef GL_COMPUTE_SHADER
#define GL_COMPUTE_SHADER 0x91B9
#endif

#ifndef GL_SHADER_STORAGE_BARRIER_BIT
#define GL_SHADER_STORAGE_BARRIER_BIT 0x00002000
#endif

#ifndef GL_ALL_BARRIER_BITS
#define GL_ALL_BARRIER_BITS 0xFFFFFFFF
#endif

#ifndef GL_MAX_COMPUTE_WORK_GROUP_SIZE
#define GL_MAX_COMPUTE_WORK_GROUP_SIZE 0x91BF
#endif

#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS 0x8B81
#endif

#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS 0x8B82
#endif

#ifndef GL_INFO_LOG_LENGTH
#define GL_INFO_LOG_LENGTH 0x8B84
#endif

#ifndef GL_SYNC_GPU_COMMANDS_COMPLETE
#define GL_SYNC_GPU_COMMANDS_COMPLETE 0x9117
#endif

#ifndef GL_ALREADY_SIGNALED
#define GL_ALREADY_SIGNALED 0x911A
#endif

#ifndef GL_TIMEOUT_EXPIRED
#define GL_TIMEOUT_EXPIRED 0x911B
#endif

#ifndef GL_CONDITION_SATISFIED
#define GL_CONDITION_SATISFIED 0x911C
#endif

#ifndef GL_WAIT_FAILED
#define GL_WAIT_FAILED 0x911D
#endif

// Declare missing OpenGL functions as extern
extern "C" {
    void glGetIntegeri_v(unsigned int target, unsigned int index, int* data);
    void glDeleteProgram(unsigned int program);
    void glDeleteShader(unsigned int shader);
    unsigned int glCreateShader(unsigned int type);
    void glShaderSource(unsigned int shader, int count, const char* const* string, const int* length);
    void glCompileShader(unsigned int shader);
    void glGetShaderiv(unsigned int shader, unsigned int pname, int* params);
    unsigned int glCreateProgram(void);
    void glAttachShader(unsigned int program, unsigned int shader);
    void glLinkProgram(unsigned int program);
    void glGetProgramiv(unsigned int program, unsigned int pname, int* params);
    void glUseProgram(unsigned int program);
    void glDispatchCompute(unsigned int num_groups_x, unsigned int num_groups_y, unsigned int num_groups_z);
    void glMemoryBarrier(unsigned int barriers);
    void glGetShaderInfoLog(unsigned int shader, int bufSize, int* length, char* infoLog);
    void glGetProgramInfoLog(unsigned int program, int bufSize, int* length, char* infoLog);
    
    // OpenGL sync functions for async operations
    void* glFenceSync(unsigned int condition, unsigned int flags);
    void glDeleteSync(void* sync);
    unsigned int glClientWaitSync(void* sync, unsigned int flags, unsigned long long timeout);
}

#endif

GPUComputePipeline::GPUComputePipeline()
    : initialized_(false)
    , gpu_available_(false)
    , debugging_enabled_(false)
#ifdef USE_GPU
    , compute_program_(0)
    , compute_shader_(0)
    , sync_object_(0)
    , async_operation_active_(false)
#endif
    , max_work_group_size_(1, 1, 1)
    , current_work_group_size_(1, 1, 1)
{
}

GPUComputePipeline::~GPUComputePipeline() {
    cleanup();
}

bool GPUComputePipeline::initialize() {
#ifdef USE_GPU
    if (initialized_) {
        return true;
    }
    
    if (!checkGLExtensions()) {
        last_error_ = "Required OpenGL extensions not available";
        return false;
    }
    
    if (!validateDriverCompatibility()) {
        last_error_ = "GPU driver compatibility check failed";
        return false;
    }
    
    GLint max_x, max_y, max_z;
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &max_x);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &max_y);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &max_z);
    
    max_work_group_size_ = WorkGroupSize(
        static_cast<unsigned int>(max_x),
        static_cast<unsigned int>(max_y), 
        static_cast<unsigned int>(max_z)
    );
    
    const GLubyte* vendor = glGetString(GL_VENDOR);
    const GLubyte* renderer = glGetString(GL_RENDERER);
    const GLubyte* version = glGetString(GL_VERSION);
    
    std::ostringstream info;
    info << "Vendor: " << (vendor ? reinterpret_cast<const char*>(vendor) : "Unknown")
         << ", Renderer: " << (renderer ? reinterpret_cast<const char*>(renderer) : "Unknown")
         << ", Version: " << (version ? reinterpret_cast<const char*>(version) : "Unknown");
    driver_info_ = info.str();
    
    initialized_ = true;
    gpu_available_ = true;
    
    if (debugging_enabled_) {
        std::cout << "GPU Compute Pipeline initialized successfully" << std::endl;
        std::cout << "Driver Info: " << driver_info_ << std::endl;
        std::cout << "Max Work Group Size: " << max_work_group_size_.x 
                  << "x" << max_work_group_size_.y 
                  << "x" << max_work_group_size_.z << std::endl;
    }
    
    return true;
#else
    last_error_ = "GPU support not compiled in (USE_GPU not defined)";
    gpu_available_ = false;
    return false;
#endif
}

bool GPUComputePipeline::isAvailable() const {
    return gpu_available_ && initialized_;
}

bool GPUComputePipeline::validateDriverCompatibility() {
#ifdef USE_GPU
    GLint major_version, minor_version;
    glGetIntegerv(GL_MAJOR_VERSION, &major_version);
    glGetIntegerv(GL_MINOR_VERSION, &minor_version);
    
    if (major_version < 4 || (major_version == 4 && minor_version < 3)) {
        last_error_ = "OpenGL 4.3 or higher required for compute shaders";
        return false;
    }
    
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::ostringstream oss;
        oss << "OpenGL error during compatibility check: " << error;
        last_error_ = oss.str();
        return false;
    }
    
    return true;
#else
    return false;
#endif
}

void GPUComputePipeline::cleanup() {
#ifdef USE_GPU
    if (compute_program_ != 0) {
        glDeleteProgram(compute_program_);
        compute_program_ = 0;
    }
    
    if (compute_shader_ != 0) {
        glDeleteShader(compute_shader_);
        compute_shader_ = 0;
    }
#endif
    
    initialized_ = false;
    gpu_available_ = false;
}

bool GPUComputePipeline::compileShader(const ComputeShaderInfo& shader_info) {
    return compileShader(shader_info.source);
}

bool GPUComputePipeline::compileShader(const std::string& source) {
#ifdef USE_GPU
    if (!initialized_) {
        last_error_ = "Pipeline not initialized";
        return false;
    }
    
    if (compute_shader_ != 0) {
        glDeleteShader(compute_shader_);
    }
    
    compute_shader_ = glCreateShader(GL_COMPUTE_SHADER);
    if (compute_shader_ == 0) {
        last_error_ = "Failed to create compute shader";
        return false;
    }
    
    const char* source_cstr = source.c_str();
    glShaderSource(compute_shader_, 1, &source_cstr, nullptr);
    glCompileShader(compute_shader_);
    
    GLint compiled;
    glGetShaderiv(compute_shader_, GL_COMPILE_STATUS, &compiled);
    
    if (!compiled) {
        logShaderError(compute_shader_);
        glDeleteShader(compute_shader_);
        compute_shader_ = 0;
        return false;
    }
    
    if (debugging_enabled_) {
        std::cout << "Compute shader compiled successfully" << std::endl;
    }
    
    return true;
#else
    last_error_ = "GPU support not compiled in";
    return false;
#endif
}

bool GPUComputePipeline::linkProgram() {
#ifdef USE_GPU
    if (!initialized_ || compute_shader_ == 0) {
        last_error_ = "Shader not compiled";
        return false;
    }
    
    if (compute_program_ != 0) {
        glDeleteProgram(compute_program_);
    }
    
    compute_program_ = glCreateProgram();
    if (compute_program_ == 0) {
        last_error_ = "Failed to create compute program";
        return false;
    }
    
    glAttachShader(compute_program_, compute_shader_);
    glLinkProgram(compute_program_);
    
    GLint linked;
    glGetProgramiv(compute_program_, GL_LINK_STATUS, &linked);
    
    if (!linked) {
        logProgramError(compute_program_);
        glDeleteProgram(compute_program_);
        compute_program_ = 0;
        return false;
    }
    
    if (debugging_enabled_) {
        std::cout << "Compute program linked successfully" << std::endl;
    }
    
    return true;
#else
    last_error_ = "GPU support not compiled in";
    return false;
#endif
}

bool GPUComputePipeline::dispatch(const WorkGroupSize& work_groups) {
    return dispatch(work_groups.x, work_groups.y, work_groups.z);
}

bool GPUComputePipeline::dispatch(unsigned int work_groups_x, unsigned int work_groups_y, unsigned int work_groups_z) {
#ifdef USE_GPU
    if (!initialized_ || compute_program_ == 0) {
        last_error_ = "Pipeline not ready for dispatch";
        return false;
    }
    
    WorkGroupSize dispatch_size(work_groups_x, work_groups_y, work_groups_z);
    if (!validateWorkGroupSize(dispatch_size)) {
        return false;
    }
    
    glUseProgram(compute_program_);
    glDispatchCompute(work_groups_x, work_groups_y, work_groups_z);
    
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::ostringstream oss;
        oss << "OpenGL error during compute dispatch: " << error;
        last_error_ = oss.str();
        return false;
    }
    
    if (debugging_enabled_) {
        std::cout << "Compute dispatch: " << work_groups_x 
                  << "x" << work_groups_y 
                  << "x" << work_groups_z << std::endl;
    }
    
    return true;
#else
    last_error_ = "GPU support not compiled in";
    return false;
#endif
}

void GPUComputePipeline::memoryBarrier() {
#ifdef USE_GPU
    if (initialized_) {
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }
#endif
}

void GPUComputePipeline::synchronize() {
#ifdef USE_GPU
    if (initialized_) {
        glMemoryBarrier(GL_ALL_BARRIER_BITS);
        glFinish();
    }
#endif
}

bool GPUComputePipeline::dispatchAsync(const WorkGroupSize& work_groups) {
    return dispatchAsync(work_groups.x, work_groups.y, work_groups.z);
}

bool GPUComputePipeline::dispatchAsync(unsigned int work_groups_x, unsigned int work_groups_y, unsigned int work_groups_z) {
#ifdef USE_GPU
    if (!initialized_ || !gpu_available_) {
        last_error_ = "GPU compute pipeline not available for async dispatch";
        return false;
    }
    
    if (async_operation_active_) {
        last_error_ = "Another async operation is already in progress";
        return false;
    }
    
    if (!validateWorkGroupSize(WorkGroupSize(work_groups_x, work_groups_y, work_groups_z))) {
        return false;
    }
    
    // Clean up any previous sync object
    if (sync_object_ != 0) {
        glDeleteSync((void*)sync_object_);
        sync_object_ = 0;
    }
    
    // Dispatch the compute work (non-blocking)
    glDispatchCompute(work_groups_x, work_groups_y, work_groups_z);
    
    unsigned int error = glGetError();
    if (error != GL_NO_ERROR) {
        std::ostringstream oss;
        oss << "OpenGL error during async dispatch: " << error;
        last_error_ = oss.str();
        return false;
    }
    
    // Create a sync object to track completion
    void* sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    if (sync == nullptr) {
        last_error_ = "Failed to create GPU sync object";
        return false;
    }
    
    sync_object_ = (unsigned int)(uintptr_t)sync;
    async_operation_active_ = true;
    
    return true;
#else
    last_error_ = "GPU support not compiled in";
    return false;
#endif
}

bool GPUComputePipeline::isComplete() const {
#ifdef USE_GPU
    if (!async_operation_active_ || sync_object_ == 0) {
        return true; // No operation in progress
    }
    
    // Check the sync object status without blocking
    void* sync = (void*)(uintptr_t)sync_object_;
    unsigned int result = glClientWaitSync(sync, 0, 0); // 0 timeout = immediate return
    
    if (result == GL_ALREADY_SIGNALED || result == GL_CONDITION_SATISFIED) {
        // Operation is complete, clean up
        glDeleteSync(sync);
        const_cast<GPUComputePipeline*>(this)->sync_object_ = 0;
        const_cast<GPUComputePipeline*>(this)->async_operation_active_ = false;
        return true;
    } else if (result == GL_TIMEOUT_EXPIRED) {
        // Still working
        return false;
    } else {
        // Error occurred
        glDeleteSync(sync);
        const_cast<GPUComputePipeline*>(this)->sync_object_ = 0;
        const_cast<GPUComputePipeline*>(this)->async_operation_active_ = false;
        const_cast<GPUComputePipeline*>(this)->last_error_ = "GPU sync operation failed";
        return true; // Consider it "complete" with error
    }
#else
    return true;
#endif
}

void GPUComputePipeline::setWorkGroupSize(const WorkGroupSize& size) {
    current_work_group_size_ = size;
}

WorkGroupSize GPUComputePipeline::getMaxWorkGroupSize() const {
    return max_work_group_size_;
}

WorkGroupSize GPUComputePipeline::getWorkGroupSize() const {
    return current_work_group_size_;
}

bool GPUComputePipeline::hasCapability(GPUCapability capability) const {
#ifdef USE_GPU
    if (!initialized_) {
        return false;
    }
    
    switch (capability) {
        case GPUCapability::COMPUTE_SHADERS:
            return true;
        case GPUCapability::SHADER_STORAGE_BUFFER:
            return true;
        case GPUCapability::ATOMIC_COUNTERS:
            return true;
        case GPUCapability::IMAGE_LOAD_STORE:
            return true;
        default:
            return false;
    }
#else
    return false;
#endif
}

std::string GPUComputePipeline::getDriverInfo() const {
    return driver_info_;
}

std::string GPUComputePipeline::getErrorMessage() const {
    return last_error_;
}

void GPUComputePipeline::enableDebugging(bool enable) {
    debugging_enabled_ = enable;
}

bool GPUComputePipeline::isDebuggingEnabled() const {
    return debugging_enabled_;
}

unsigned int GPUComputePipeline::getProgramHandle() const {
#ifdef USE_GPU
    return compute_program_;
#else
    return 0;
#endif
}

bool GPUComputePipeline::checkGLExtensions() {
#ifdef USE_GPU
    const GLubyte* extensions = glGetString(GL_EXTENSIONS);
    if (!extensions) {
        return false;
    }
    
    std::string ext_str(reinterpret_cast<const char*>(extensions));
    return ext_str.find("GL_ARB_compute_shader") != std::string::npos;
#else
    return false;
#endif
}

bool GPUComputePipeline::validateWorkGroupSize(const WorkGroupSize& size) {
    if (size.x > max_work_group_size_.x || 
        size.y > max_work_group_size_.y || 
        size.z > max_work_group_size_.z) {
        std::ostringstream oss;
        oss << "Work group size " << size.x << "x" << size.y << "x" << size.z
            << " exceeds maximum " << max_work_group_size_.x 
            << "x" << max_work_group_size_.y 
            << "x" << max_work_group_size_.z;
        last_error_ = oss.str();
        return false;
    }
    return true;
}

void GPUComputePipeline::logShaderError(unsigned int shader) {
#ifdef USE_GPU
    GLint log_length;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
    
    if (log_length > 0) {
        std::vector<char> log(log_length);
        glGetShaderInfoLog(shader, log_length, nullptr, log.data());
        last_error_ = "Shader compilation error: " + std::string(log.data());
        
        if (debugging_enabled_) {
            std::cerr << last_error_ << std::endl;
        }
    } else {
        last_error_ = "Unknown shader compilation error";
    }
#endif
}

void GPUComputePipeline::logProgramError(unsigned int program) {
#ifdef USE_GPU
    GLint log_length;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
    
    if (log_length > 0) {
        std::vector<char> log(log_length);
        glGetProgramInfoLog(program, log_length, nullptr, log.data());
        last_error_ = "Program linking error: " + std::string(log.data());
        
        if (debugging_enabled_) {
            std::cerr << last_error_ << std::endl;
        }
    } else {
        last_error_ = "Unknown program linking error";
    }
#endif
}