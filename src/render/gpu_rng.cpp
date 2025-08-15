#include "gpu_rng.h"
#include "gpu_memory.h"
#include <iostream>
#include <chrono>
#include <random>
#include <algorithm>

GPURandomGenerator::GPURandomGenerator()
    : initialized_(false)
    , imageWidth_(0)
    , imageHeight_(0)
    , totalPixels_(0)
    , baseSeed_(0)
{
}

GPURandomGenerator::~GPURandomGenerator() {
    cleanup();
}

bool GPURandomGenerator::initialize(int imageWidth, int imageHeight) {
    if (initialized_) {
        return true;
    }
    
    imageWidth_ = imageWidth;
    imageHeight_ = imageHeight;
    totalPixels_ = imageWidth * imageHeight;
    
    if (totalPixels_ <= 0) {
        std::cerr << "Invalid image dimensions for GPU RNG: " << imageWidth << "x" << imageHeight << std::endl;
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
    initializeStates(baseSeed_);
    
    // Transfer initial states to GPU
    if (!memoryManager_->transferToGPU(rngBuffer_, rngStates_.data(), bufferSize)) {
        std::cerr << "Failed to transfer RNG states to GPU" << std::endl;
        cleanup();
        return false;
    }
    
    initialized_ = true;
    std::cout << "GPU RNG initialized: " << imageWidth << "x" << imageHeight 
              << " (" << totalPixels_ << " pixels), seed=" << baseSeed_ << std::endl;
    
    return true;
}

void GPURandomGenerator::cleanup() {
    if (rngBuffer_) {
        if (memoryManager_) {
            memoryManager_->deallocateBuffer(rngBuffer_);
        }
        rngBuffer_.reset();
    }
    
    rngStates_.clear();
    initialized_ = false;
}

void GPURandomGenerator::seedRandom(uint32_t seed) {
    if (!initialized_) {
        return;
    }
    
    baseSeed_ = seed;
    initializeStates(seed);
    
    // Transfer updated states to GPU
    size_t bufferSize = totalPixels_ * sizeof(uint32_t);
    if (!memoryManager_->transferToGPU(rngBuffer_, rngStates_.data(), bufferSize)) {
        std::cerr << "Failed to transfer updated RNG states to GPU" << std::endl;
    }
}

void GPURandomGenerator::updateRNGStates(int frameNumber) {
    if (!initialized_) {
        return;
    }
    
    // Update seed based on frame number to ensure different random sequences per frame
    uint32_t frameSeed = baseSeed_ ^ static_cast<uint32_t>(frameNumber * 1664525 + 1013904223);
    
    // Update states with frame-based variation
    for (int i = 0; i < totalPixels_; ++i) {
        rngStates_[i] = rngStates_[i] ^ frameSeed;
        // Advance the state to ensure good distribution
        xorshift32(rngStates_[i]);
    }
    
    // Transfer updated states to GPU
    size_t bufferSize = totalPixels_ * sizeof(uint32_t);
    if (!memoryManager_->transferToGPU(rngBuffer_, rngStates_.data(), bufferSize)) {
        std::cerr << "Failed to transfer frame-updated RNG states to GPU" << std::endl;
    }
}

void GPURandomGenerator::resetRNGStates() {
    if (!initialized_) {
        return;
    }
    
    initializeStates(baseSeed_);
    
    // Transfer reset states to GPU
    size_t bufferSize = totalPixels_ * sizeof(uint32_t);
    if (!memoryManager_->transferToGPU(rngBuffer_, rngStates_.data(), bufferSize)) {
        std::cerr << "Failed to transfer reset RNG states to GPU" << std::endl;
    }
}

void GPURandomGenerator::initializeStates(uint32_t baseSeed) {
    // Initialize each pixel with a unique seed derived from base seed and position
    for (int y = 0; y < imageHeight_; ++y) {
        for (int x = 0; x < imageWidth_; ++x) {
            seedPixel(x, y, baseSeed);
        }
    }
}

void GPURandomGenerator::seedPixel(int x, int y, uint32_t baseSeed) {
    int pixelIndex = y * imageWidth_ + x;
    
    // Create unique seed for this pixel using hash of position and base seed
    uint32_t seed = baseSeed;
    seed ^= static_cast<uint32_t>(x * 1664525);
    seed ^= static_cast<uint32_t>(y * 1013904223);
    seed ^= static_cast<uint32_t>(pixelIndex * 3141592653u);
    
    // Ensure seed is never zero (XORshift32 requirement)
    if (seed == 0) {
        seed = 1;
    }
    
    rngStates_[pixelIndex] = seed;
}

uint32_t GPURandomGenerator::xorshift32(uint32_t& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

bool GPURandomGenerator::validateStatisticalQuality() {
    if (!initialized_) {
        std::cerr << "GPU RNG not initialized for statistical validation" << std::endl;
        return false;
    }
    
    const size_t testSamples = 10000;
    std::vector<float> samples;
    generateTestSamples(samples, testSamples);
    
    if (samples.size() != testSamples) {
        std::cerr << "Failed to generate test samples for validation" << std::endl;
        return false;
    }
    
    // Test 1: Mean should be approximately 0.5
    double mean = 0.0;
    for (float sample : samples) {
        mean += sample;
    }
    mean /= samples.size();
    
    if (std::abs(mean - 0.5) > 0.05) { // Allow 5% deviation
        std::cerr << "RNG mean test failed: " << mean << " (expected ~0.5)" << std::endl;
        return false;
    }
    
    // Test 2: Standard deviation should be approximately sqrt(1/12) ≈ 0.289
    double variance = 0.0;
    for (float sample : samples) {
        double diff = sample - mean;
        variance += diff * diff;
    }
    variance /= samples.size();
    double stddev = std::sqrt(variance);
    double expectedStddev = std::sqrt(1.0 / 12.0); // ~0.289
    
    if (std::abs(stddev - expectedStddev) > 0.05) {
        std::cerr << "RNG standard deviation test failed: " << stddev 
                  << " (expected ~" << expectedStddev << ")" << std::endl;
        return false;
    }
    
    // Test 3: Distribution should be roughly uniform (chi-square test)
    const int bins = 10;
    std::vector<int> histogram(bins, 0);
    
    for (float sample : samples) {
        int bin = static_cast<int>(sample * bins);
        if (bin >= bins) bin = bins - 1; // Handle edge case
        histogram[bin]++;
    }
    
    double expectedCount = static_cast<double>(testSamples) / bins;
    double chiSquare = 0.0;
    
    for (int count : histogram) {
        double diff = count - expectedCount;
        chiSquare += (diff * diff) / expectedCount;
    }
    
    // Critical value for chi-square with 9 degrees of freedom at 95% confidence: ~16.9
    if (chiSquare > 16.9) {
        std::cerr << "RNG uniformity test failed: chi-square = " << chiSquare 
                  << " (critical value = 16.9)" << std::endl;
        return false;
    }
    
    std::cout << "GPU RNG statistical validation passed:" << std::endl;
    std::cout << "  Mean: " << mean << " (expected ~0.5)" << std::endl;
    std::cout << "  Std Dev: " << stddev << " (expected ~" << expectedStddev << ")" << std::endl;
    std::cout << "  Chi-square: " << chiSquare << " (critical value = 16.9)" << std::endl;
    
    return true;
}

void GPURandomGenerator::generateTestSamples(std::vector<float>& samples, size_t count) {
    samples.clear();
    samples.reserve(count);
    
    // Use a subset of our RNG states to generate test samples
    size_t stateIndex = 0;
    for (size_t i = 0; i < count; ++i) {
        if (stateIndex >= rngStates_.size()) {
            stateIndex = 0;
        }
        
        uint32_t state = rngStates_[stateIndex];
        uint32_t randomValue = xorshift32(state);
        float sample = static_cast<float>(randomValue) / 4294967296.0f; // Convert to [0,1)
        
        samples.push_back(sample);
        rngStates_[stateIndex] = state; // Update state
        stateIndex++;
    }
}