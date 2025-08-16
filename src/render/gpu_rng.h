#pragma once

#include "core/common.h"
#include <memory>
#include <vector>

#ifdef USE_GPU
#include <GL/gl.h>
#include <GL/glext.h>
#endif

struct GPUBuffer;
class GPUMemoryManager;

class GPURandomGenerator {
public:
    GPURandomGenerator();
    ~GPURandomGenerator();
    
    bool initialize(int width, int height);
    bool initialize(int width, int height, std::shared_ptr<GPUMemoryManager> memoryManager);
    bool ensureBuffersAllocated(); // Lazy buffer allocation
    void cleanup();
    
    bool isInitialized() const;
    
    // Get RNG buffer for binding to shaders
    std::shared_ptr<GPUBuffer> getRNGBuffer() const;
    
    // Statistical validation
    bool validateStatistics() const;
    void resetStatistics();
    
    std::string getErrorMessage() const;
    
private:
    bool initialized_;
    int image_width_;
    int image_height_;
    int totalPixels_;
    uint32_t baseSeed_;
    std::vector<uint32_t> rngStates_;
    std::shared_ptr<class GPUMemoryManager> memoryManager_;
    std::shared_ptr<GPUBuffer> rngBuffer_;
    
#ifdef USE_GPU
    std::shared_ptr<GPUBuffer> rng_buffer_;
    unsigned int rng_texture_;
#endif
    
    std::string last_error_;
    
    void generateSeeds();
    bool createRNGTexture();
};