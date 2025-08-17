#pragma once

#include <string>
#include <vector>
#include <memory>

#ifdef USE_GPU
#include <GL/gl.h>

// OpenGL constants
#ifndef GL_MAX_COMPUTE_WORK_GROUP_SIZE
#define GL_MAX_COMPUTE_WORK_GROUP_SIZE 0x91BF
#endif

#ifndef GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS
#define GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS 0x90EB
#endif

#ifndef GL_VENDOR
#define GL_VENDOR 0x1F00
#endif

#ifndef GL_RENDERER
#define GL_RENDERER 0x1F01
#endif

// Declare missing OpenGL functions
extern "C" {
    void glGetIntegeri_v(unsigned int target, unsigned int index, int* data);
    const unsigned char* glGetString(unsigned int name);
    void glGetIntegerv(unsigned int pname, int* data);
}

#endif

class GPUHardwareOptimizer {
public:
    struct HardwareProfile {
        std::string gpuVendor;
        std::string gpuModel;
        int maxComputeUnits = 0;
        size_t maxMemoryBandwidth = 0;      // MB/s
        int optimalThreadGroupSize = 16;     // Default conservative value
        bool supportsAsyncTransfer = false;
        int maxWorkGroupSizeX = 1;
        int maxWorkGroupSizeY = 1;
        int maxWorkGroupSizeZ = 1;
        int maxWorkGroupInvocations = 1;
        
        // Vendor-specific optimizations
        bool useCoalescedMemoryAccess = true;
        int preferredVectorWidth = 4;
        double memoryLatencyCompensation = 1.0;
    };

    GPUHardwareOptimizer();
    ~GPUHardwareOptimizer() = default;

    // Hardware detection and profiling
    bool detectHardwareCapabilities();
    HardwareProfile getHardwareProfile() const { return profile_; }
    void optimizeForHardware();

    // Performance optimization
    int getOptimalThreadGroupSize(int imageWidth, int imageHeight) const;
    int getOptimalWorkGroupCount() const;
    bool shouldUseAsyncTransfer() const { return profile_.supportsAsyncTransfer; }
    
    // Adaptive optimization
    void updateOptimizationBasedOnPerformance(double currentPerformance, double targetPerformance);
    std::pair<int, int> getOptimalWorkGroupDimensions(int imageWidth, int imageHeight) const;
    
    // Memory optimization
    size_t getOptimalBufferSize(size_t dataSize) const;
    int getOptimalMemoryTransferBatchSize() const;
    
    // Vendor-specific optimizations
    bool isNVIDIA() const;
    bool isAMD() const;
    bool isIntel() const;
    
    // Configuration
    void setPerformanceTarget(double target) { performanceTarget_ = target; }
    void enableAdaptiveOptimization(bool enable) { adaptiveOptimization_ = enable; }

private:
    bool initialized_;
    bool adaptiveOptimization_;
    double performanceTarget_;
    HardwareProfile profile_;
    
    // Benchmarking data for adaptive optimization
    struct BenchmarkResult {
        int threadGroupSize;
        double performance;
        double efficiency;
    };
    std::vector<BenchmarkResult> benchmarkHistory_;
    
    // Internal methods
    void estimateHardwareSpecs();
    void detectVendorOptimizations();
    void benchmarkThreadGroupSizes();
    int findOptimalThreadGroupSize(int minSize, int maxSize, int imageWidth, int imageHeight);
    void applyNVIDIAOptimizations();
    void applyAMDOptimizations();
    void applyIntelOptimizations();
    void updateAdaptiveSettings(double performanceRatio);
};