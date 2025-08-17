#pragma once

#include "core/common.h"
#include <string>
#include <vector>
#include <memory>

#ifdef USE_GPU
#include <GL/gl.h>
#include <GL/glext.h>
#endif

struct WorkGroupSize {
    unsigned int x, y, z;
    
    constexpr WorkGroupSize(unsigned int x = 1, unsigned int y = 1, unsigned int z = 1) noexcept 
        : x(x), y(y), z(z) {}
};

struct ComputeShaderInfo {
    std::string source;
    std::string entry_point;
    std::vector<std::string> defines;
    
    ComputeShaderInfo(const std::string& src, const std::string& entry = "main") 
        : source(src), entry_point(entry) {}
};

enum class GPUCapability {
    COMPUTE_SHADERS,
    SHADER_STORAGE_BUFFER,
    ATOMIC_COUNTERS,
    IMAGE_LOAD_STORE
};

class GPUComputePipeline {
public:
    GPUComputePipeline();
    ~GPUComputePipeline();
    
    bool initialize();
    bool isAvailable() const;
    bool validateDriverCompatibility();
    void cleanup();
    
    bool compileShader(const ComputeShaderInfo& shader_info);
    bool compileShader(const std::string& source);
    bool linkProgram();
    
    bool dispatch(const WorkGroupSize& work_groups);
    bool dispatch(unsigned int work_groups_x, unsigned int work_groups_y = 1, unsigned int work_groups_z = 1);
    void memoryBarrier();
    void synchronize();
    
    // Asynchronous operations
    bool dispatchAsync(const WorkGroupSize& work_groups);
    bool dispatchAsync(unsigned int work_groups_x, unsigned int work_groups_y = 1, unsigned int work_groups_z = 1);
    bool isComplete() const;  // Check if async operation is done (non-blocking)
    
    void setWorkGroupSize(const WorkGroupSize& size);
    WorkGroupSize getMaxWorkGroupSize() const;
    WorkGroupSize getWorkGroupSize() const;
    
    bool hasCapability(GPUCapability capability) const;
    std::string getDriverInfo() const;
    std::string getErrorMessage() const;
    
    void enableDebugging(bool enable = true);
    bool isDebuggingEnabled() const;
    
    // Program access for advanced usage
    unsigned int getProgramHandle() const;
    
private:
    bool checkGLExtensions();
    bool validateWorkGroupSize(const WorkGroupSize& size);
    void logShaderError(unsigned int shader);
    void logProgramError(unsigned int program);
    
    bool initialized_;
    bool gpu_available_;
    bool debugging_enabled_;
    
#ifdef USE_GPU
    unsigned int compute_program_;
    unsigned int compute_shader_;
    
    // Async operation state
    unsigned int sync_object_;  // OpenGL sync object for async operations
    bool async_operation_active_;
#endif
    
    WorkGroupSize max_work_group_size_;
    WorkGroupSize current_work_group_size_;
    
    std::string last_error_;
    std::string driver_info_;
};