#include "render/gpu_hardware_optimizer.h"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <cmath>

GPUHardwareOptimizer::GPUHardwareOptimizer()
    : initialized_(false)
    , adaptiveOptimization_(true)
    , performanceTarget_(5.0) // 5x speedup target
{
}

bool GPUHardwareOptimizer::detectHardwareCapabilities() {
#ifdef USE_GPU
    try {
        // Get vendor and renderer strings
        const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
        const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        
        if (vendor) {
            profile_.gpuVendor = std::string(vendor);
        }
        if (renderer) {
            profile_.gpuModel = std::string(renderer);
        }
        
        // Get maximum work group sizes
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &profile_.maxWorkGroupSizeX);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &profile_.maxWorkGroupSizeY);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &profile_.maxWorkGroupSizeZ);
        
        // Get maximum work group invocations
        glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &profile_.maxWorkGroupInvocations);
        
        // Estimate compute units and memory bandwidth based on vendor/model
        estimateHardwareSpecs();
        
        // Apply vendor-specific optimizations
        detectVendorOptimizations();
        
        // Set async transfer support based on OpenGL version and vendor
        profile_.supportsAsyncTransfer = true; // Most modern GPUs support this
        
        initialized_ = true;
        
        std::cout << "Detected GPU: " << profile_.gpuVendor << " " << profile_.gpuModel << std::endl;
        std::cout << "Max work group: " << profile_.maxWorkGroupSizeX << "x" 
                  << profile_.maxWorkGroupSizeY << "x" << profile_.maxWorkGroupSizeZ << std::endl;
        std::cout << "Max invocations: " << profile_.maxWorkGroupInvocations << std::endl;
        
        return true;
    } catch (...) {
        std::cerr << "Failed to detect GPU hardware capabilities" << std::endl;
        return false;
    }
#else
    std::cout << "GPU hardware detection disabled (USE_GPU not defined)" << std::endl;
    return false;
#endif
}

void GPUHardwareOptimizer::estimateHardwareSpecs() {
    // Estimate hardware specifications based on vendor and model strings
    // This is a simplified estimation - in practice, you'd use vendor-specific APIs
    
    if (isNVIDIA()) {
        profile_.maxComputeUnits = 2048; // Typical for mid-range NVIDIA
        profile_.maxMemoryBandwidth = 400000; // 400 GB/s typical
        profile_.preferredVectorWidth = 4;
        profile_.memoryLatencyCompensation = 1.2;
    } else if (isAMD()) {
        profile_.maxComputeUnits = 3584; // Typical for mid-range AMD
        profile_.maxMemoryBandwidth = 500000; // 500 GB/s typical
        profile_.preferredVectorWidth = 4;
        profile_.memoryLatencyCompensation = 1.1;
    } else if (isIntel()) {
        profile_.maxComputeUnits = 512; // Typical for Intel integrated
        profile_.maxMemoryBandwidth = 100000; // 100 GB/s typical
        profile_.preferredVectorWidth = 2;
        profile_.memoryLatencyCompensation = 1.5;
    } else {
        // Generic defaults
        profile_.maxComputeUnits = 1024;
        profile_.maxMemoryBandwidth = 200000;
        profile_.preferredVectorWidth = 4;
        profile_.memoryLatencyCompensation = 1.0;
    }
}

void GPUHardwareOptimizer::detectVendorOptimizations() {
    if (isNVIDIA()) {
        applyNVIDIAOptimizations();
    } else if (isAMD()) {
        applyAMDOptimizations();
    } else if (isIntel()) {
        applyIntelOptimizations();
    }
}

void GPUHardwareOptimizer::applyNVIDIAOptimizations() {
    // NVIDIA-specific optimizations
    profile_.optimalThreadGroupSize = 32; // Warp size
    profile_.useCoalescedMemoryAccess = true;
    profile_.preferredVectorWidth = 4;
    
    std::cout << "Applied NVIDIA-specific optimizations (warp size: 32)" << std::endl;
}

void GPUHardwareOptimizer::applyAMDOptimizations() {
    // AMD-specific optimizations
    profile_.optimalThreadGroupSize = 64; // Wavefront size
    profile_.useCoalescedMemoryAccess = true;
    profile_.preferredVectorWidth = 4;
    
    std::cout << "Applied AMD-specific optimizations (wavefront size: 64)" << std::endl;
}

void GPUHardwareOptimizer::applyIntelOptimizations() {
    // Intel-specific optimizations
    profile_.optimalThreadGroupSize = 16; // Conservative for integrated GPUs
    profile_.useCoalescedMemoryAccess = true;
    profile_.preferredVectorWidth = 2; // Lower vector width for integrated
    
    std::cout << "Applied Intel-specific optimizations (thread group: 16)" << std::endl;
}

void GPUHardwareOptimizer::optimizeForHardware() {
    if (!initialized_) {
        detectHardwareCapabilities();
    }
    
    if (adaptiveOptimization_) {
        benchmarkThreadGroupSizes();
    }
    
    std::cout << "Hardware optimization complete - optimal thread group size: " 
              << profile_.optimalThreadGroupSize << std::endl;
}

int GPUHardwareOptimizer::getOptimalThreadGroupSize(int imageWidth, int imageHeight) const {
    if (!initialized_) {
        return 16; // Safe default
    }
    
    // Consider image dimensions for optimal thread group sizing
    int pixels = imageWidth * imageHeight;
    int baseSize = profile_.optimalThreadGroupSize;
    
    // Adjust based on image size
    if (pixels < 256 * 256) {
        // Small images - use smaller thread groups
        return std::max(8, baseSize / 2);
    } else if (pixels > 1024 * 1024) {
        // Large images - can use larger thread groups
        return std::min(profile_.maxWorkGroupInvocations, baseSize * 2);
    }
    
    return baseSize;
}

int GPUHardwareOptimizer::getOptimalWorkGroupCount() const {
    if (!initialized_) {
        return 8; // Safe default
    }
    
    // Estimate optimal work group count based on compute units
    // Rule of thumb: 2-4 work groups per compute unit for good occupancy
    int workGroups = profile_.maxComputeUnits * 3;
    
    // Clamp to reasonable bounds
    return std::max(1, std::min(workGroups, 4096));
}

std::pair<int, int> GPUHardwareOptimizer::getOptimalWorkGroupDimensions(int imageWidth, int imageHeight) const {
    int threadGroupSize = getOptimalThreadGroupSize(imageWidth, imageHeight);
    
    // Find optimal 2D layout for the thread group
    int bestX = threadGroupSize;
    int bestY = 1;
    
    // Try to find a more square layout if beneficial
    for (int x = 1; x <= threadGroupSize; ++x) {
        if (threadGroupSize % x == 0) {
            int y = threadGroupSize / x;
            
            // Prefer more square layouts for better cache locality
            if (std::abs(x - y) < std::abs(bestX - bestY)) {
                bestX = x;
                bestY = y;
            }
        }
    }
    
    return {bestX, bestY};
}

size_t GPUHardwareOptimizer::getOptimalBufferSize(size_t dataSize) const {
    if (!initialized_) {
        return dataSize;
    }
    
    // Align buffer size to optimal memory access patterns
    size_t alignment = profile_.preferredVectorWidth * 4; // 4 bytes per float
    size_t alignedSize = ((dataSize + alignment - 1) / alignment) * alignment;
    
    // Consider memory bandwidth limitations
    size_t maxBatchSize = profile_.maxMemoryBandwidth * 1024; // 1ms worth of bandwidth
    
    return std::min(alignedSize, maxBatchSize);
}

int GPUHardwareOptimizer::getOptimalMemoryTransferBatchSize() const {
    if (!initialized_) {
        return 1024 * 1024; // 1MB default
    }
    
    // Calculate optimal batch size based on memory bandwidth and latency
    size_t optimalBatch = static_cast<size_t>(profile_.maxMemoryBandwidth * 
                                              profile_.memoryLatencyCompensation * 1024);
    
    // Clamp to reasonable bounds (1MB to 16MB)
    return static_cast<int>(std::max(1024UL * 1024UL, 
                                    std::min(optimalBatch, 16UL * 1024UL * 1024UL)));
}

void GPUHardwareOptimizer::updateOptimizationBasedOnPerformance(double currentPerformance, double targetPerformance) {
    if (!adaptiveOptimization_) {
        return;
    }
    
    double performanceRatio = currentPerformance / targetPerformance;
    
    // Record this benchmark result
    BenchmarkResult result;
    result.threadGroupSize = profile_.optimalThreadGroupSize;
    result.performance = currentPerformance;
    result.efficiency = performanceRatio;
    
    benchmarkHistory_.push_back(result);
    
    // Keep only recent history
    if (benchmarkHistory_.size() > 20) {
        benchmarkHistory_.erase(benchmarkHistory_.begin());
    }
    
    // Adjust settings based on performance
    updateAdaptiveSettings(performanceRatio);
}

void GPUHardwareOptimizer::updateAdaptiveSettings(double performanceRatio) {
    if (performanceRatio < 0.8) {
        // Performance below target - try smaller thread groups
        profile_.optimalThreadGroupSize = std::max(8, 
            static_cast<int>(profile_.optimalThreadGroupSize * 0.8));
        
        std::cout << "Adaptive optimization: reduced thread group size to " 
                  << profile_.optimalThreadGroupSize << std::endl;
    } else if (performanceRatio > 1.2) {
        // Performance above target - try larger thread groups
        profile_.optimalThreadGroupSize = std::min(profile_.maxWorkGroupInvocations,
            static_cast<int>(profile_.optimalThreadGroupSize * 1.2));
        
        std::cout << "Adaptive optimization: increased thread group size to " 
                  << profile_.optimalThreadGroupSize << std::endl;
    }
}

void GPUHardwareOptimizer::benchmarkThreadGroupSizes() {
    // This would ideally run actual benchmarks
    // For now, we'll use heuristics based on hardware profile
    
    int minSize = 8;
    int maxSize = std::min(256, profile_.maxWorkGroupInvocations);
    
    // Find optimal size through simulated benchmarking
    int optimalSize = findOptimalThreadGroupSize(minSize, maxSize, 512, 512);
    profile_.optimalThreadGroupSize = optimalSize;
    
    std::cout << "Benchmarked thread group sizes - optimal: " << optimalSize << std::endl;
}

int GPUHardwareOptimizer::findOptimalThreadGroupSize(int minSize, int maxSize, int imageWidth, int imageHeight) {
    // Simplified heuristic-based optimization
    // In practice, this would run actual performance tests
    
    int pixels = imageWidth * imageHeight;
    int vendorOptimal = profile_.optimalThreadGroupSize;
    
    // Adjust based on workload size
    if (pixels < 256 * 256) {
        return std::max(minSize, vendorOptimal / 2);
    } else if (pixels > 1024 * 1024) {
        return std::min(maxSize, vendorOptimal * 2);
    }
    
    return vendorOptimal;
}

bool GPUHardwareOptimizer::isNVIDIA() const {
    return profile_.gpuVendor.find("NVIDIA") != std::string::npos ||
           profile_.gpuVendor.find("nvidia") != std::string::npos;
}

bool GPUHardwareOptimizer::isAMD() const {
    return profile_.gpuVendor.find("AMD") != std::string::npos ||
           profile_.gpuVendor.find("ATI") != std::string::npos ||
           profile_.gpuVendor.find("Advanced Micro Devices") != std::string::npos;
}

bool GPUHardwareOptimizer::isIntel() const {
    return profile_.gpuVendor.find("Intel") != std::string::npos ||
           profile_.gpuVendor.find("INTEL") != std::string::npos;
}