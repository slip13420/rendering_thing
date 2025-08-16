#include "gpu_rng.h"
#include "gpu_memory.h"
#include <iostream>
#include <chrono>
#include <random>
#include <algorithm>

GPURandomGenerator::GPURandomGenerator()
    : initialized_(false)
    , image_width_(0)
    , image_height_(0)
    , totalPixels_(0)
    , baseSeed_(0)
{
}

GPURandomGenerator::~GPURandomGenerator() {
    cleanup();
}

bool GPURandomGenerator::initialize(int width, int height) {
    if (initialized_) {
        return true;
    }
    
    image_width_ = width;
    image_height_ = height;
    totalPixels_ = width * height;
    
    if (totalPixels_ <= 0) {
        std::cerr << "Invalid image dimensions for GPU RNG: " << width << "x" << height << std::endl;
        return false;
    }
    
    // Create memory manager if not exists
    if (!memoryManager_) {
        memoryManager_ = std::make_shared<GPUMemoryManager>();
        if (!memoryManager_->initialize()) {
            std::cerr << "Failed to initialize GPU memory manager for RNG" << std::endl;
            return false;
        }
    }
    
    // Allocate GPU buffer for RNG states
    size_t bufferSize = totalPixels_ * sizeof(uint32_t);
    rngBuffer_ = memoryManager_->allocateBuffer(
        bufferSize, 
        GPUBufferType::SHADER_STORAGE, 
        GPUUsagePattern::DYNAMIC,
        "gpu_rng_states"
    );
    
    if (!rngBuffer_) {
        std::cerr << "Failed to allocate GPU buffer for RNG states" << std::endl;
        return false;
    }
    
    // Initialize CPU-side RNG states
    rngStates_.resize(totalPixels_);
    
    // Seed with current time
    auto now = std::chrono::high_resolution_clock::now();
    baseSeed_ = static_cast<uint32_t>(now.time_since_epoch().count());
    // Initialize RNG states here instead
    generateSeeds();
    
    // Transfer will be done lazily when buffer is allocated
    
    initialized_ = true;
    std::cout << "GPU RNG initialized: " << image_width_ << "x" << image_height_ 
              << " (" << totalPixels_ << " pixels), seed=" << baseSeed_ << std::endl;
    
    return true;
}

bool GPURandomGenerator::initialize(int width, int height, std::shared_ptr<GPUMemoryManager> memoryManager) {
    if (initialized_) {
        return true;
    }
    
    image_width_ = width;
    image_height_ = height;
    totalPixels_ = width * height;
    
    if (totalPixels_ <= 0) {
        std::cerr << "Invalid image dimensions for GPU RNG: " << width << "x" << height << std::endl;
        return false;
    }
    
    // Use provided memory manager instead of creating our own
    if (!memoryManager) {
        std::cerr << "No GPU memory manager provided for RNG initialization" << std::endl;
        return false;
    }
    memoryManager_ = memoryManager;
    
    // Skip GPU buffer allocation during initialization - will be done lazily
    // when we first need to use GPU rendering and have a valid OpenGL context
    rngBuffer_ = nullptr;
    std::cout << "GPU RNG: Deferred buffer allocation until first GPU render" << std::endl;
    
    // Initialize CPU-side RNG states
    rngStates_.resize(totalPixels_);
    
    // Seed with current time
    auto now = std::chrono::high_resolution_clock::now();
    baseSeed_ = static_cast<uint32_t>(now.time_since_epoch().count());
    // Initialize RNG states here instead
    generateSeeds();
    
    // Transfer will be done lazily when buffer is allocated
    
    initialized_ = true;
    std::cout << "GPU RNG initialized: " << image_width_ << "x" << image_height_ 
              << " (" << totalPixels_ << " pixels), seed=" << baseSeed_ << std::endl;
    
    return true;
}

bool GPURandomGenerator::ensureBuffersAllocated() {
    if (rngBuffer_ || !initialized_ || !memoryManager_) {
        return rngBuffer_ != nullptr; // Already allocated or can't allocate
    }
    
    std::cout << "GPU RNG: Allocating buffers lazily (OpenGL context should be ready)" << std::endl;
    
    // Now allocate the GPU buffer
    size_t bufferSize = totalPixels_ * sizeof(uint32_t);
    rngBuffer_ = memoryManager_->allocateBuffer(
        bufferSize, 
        GPUBufferType::SHADER_STORAGE, 
        GPUUsagePattern::DYNAMIC,
        "gpu_rng_states"
    );
    
    if (!rngBuffer_) {
        std::cerr << "Failed to allocate GPU buffer for RNG states (lazy allocation)" << std::endl;
        std::cerr << "Memory manager error: " << memoryManager_->getErrorMessage() << std::endl;
        return false;
    }
    
    // Transfer initial states to GPU
    if (!memoryManager_->transferToGPU(rngBuffer_, rngStates_.data(), bufferSize)) {
        std::cerr << "Failed to transfer RNG states to GPU (lazy allocation)" << std::endl;
        memoryManager_->deallocateBuffer(rngBuffer_);
        rngBuffer_ = nullptr;
        return false;
    }
    
    std::cout << "GPU RNG: Buffer allocated successfully" << std::endl;
    return true;
}

void GPURandomGenerator::cleanup() {
    if (rngBuffer_) {
        if (memoryManager_) {
            memoryManager_->deallocateBuffer(rngBuffer_);
        }
        rngBuffer_ = nullptr;
    }
    
    rngStates_.clear();
    initialized_ = false;
}

bool GPURandomGenerator::isInitialized() const {
    return initialized_;
}

std::shared_ptr<GPUBuffer> GPURandomGenerator::getRNGBuffer() const {
    return rngBuffer_;
}

bool GPURandomGenerator::validateStatistics() const {
    if (!initialized_) {
        return false;
    }
    
    // Simple validation - check if we have valid states
    for (const auto& state : rngStates_) {
        if (state == 0) {
            return false; // XORshift32 requires non-zero state
        }
    }
    
    return true;
}

void GPURandomGenerator::resetStatistics() {
    // Generate new seeds
    if (initialized_) {
        generateSeeds();
    }
}

std::string GPURandomGenerator::getErrorMessage() const {
    return last_error_;
}

void GPURandomGenerator::generateSeeds() {
    if (rngStates_.empty()) {
        return;
    }
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(1, UINT32_MAX); // Avoid 0 for XORshift
    
    // Generate unique seed for each pixel
    for (int y = 0; y < image_height_; ++y) {
        for (int x = 0; x < image_width_; ++x) {
            int pixelIndex = y * image_width_ + x;
            
            // Create unique seed for this pixel using hash of position and base seed
            uint32_t seed = baseSeed_;
            seed ^= static_cast<uint32_t>(x * 1664525);
            seed ^= static_cast<uint32_t>(y * 1013904223);
            seed ^= static_cast<uint32_t>(pixelIndex * 3141592653u);
            seed ^= dis(gen); // Add some randomness
            
            // Ensure seed is never zero (XORshift32 requirement)
            if (seed == 0) {
                seed = 1;
            }
            
            rngStates_[pixelIndex] = seed;
        }
    }
}

bool GPURandomGenerator::createRNGTexture() {
#ifdef USE_GPU
    // For now, we use buffer storage instead of texture
    // This method can be extended later if texture-based RNG is needed
    return true;
#else
    return false;
#endif
}