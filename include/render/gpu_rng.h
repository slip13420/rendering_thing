#pragma once

#include "core/common.h"
#include "render/gpu_memory.h"
#include <cstdint>
#include <memory>
#include <vector>

class GPURandomGenerator {
public:
    GPURandomGenerator();
    ~GPURandomGenerator();
    
    bool initialize(int imageWidth, int imageHeight);
    void cleanup();
    
    void seedRandom(uint32_t seed);
    void updateRNGStates(int frameNumber);
    void resetRNGStates();
    
    std::shared_ptr<GPUBuffer> getRNGBuffer() const { return rngBuffer_; }
    bool isInitialized() const { return initialized_; }
    
    // Statistical validation
    bool validateStatisticalQuality();
    void generateTestSamples(std::vector<float>& samples, size_t count);
    
private:
    void initializeStates(uint32_t baseSeed);
    uint32_t xorshift32(uint32_t& state);
    void seedPixel(int x, int y, uint32_t baseSeed);
    
    bool initialized_;
    int imageWidth_;
    int imageHeight_;
    int totalPixels_;
    
    std::shared_ptr<GPUBuffer> rngBuffer_;
    std::shared_ptr<GPUMemoryManager> memoryManager_;
    std::vector<uint32_t> rngStates_;
    
    uint32_t baseSeed_;
};