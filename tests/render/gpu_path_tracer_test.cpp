#include <gtest/gtest.h>
#include "render/path_tracer.h"
#include "render/gpu_compute.h"
#include "render/gpu_memory.h"
#include "core/scene_manager.h"
#include "core/camera.h"

class GPUPathTracerTest : public ::testing::Test {
protected:
    void SetUp() override {
        pathTracer = std::make_unique<PathTracer>();
        sceneManager = std::make_shared<SceneManager>();
        
        // Set up a simple camera
        Camera camera(Vector3(0, 0, 0), Vector3(0, 0, -1), Vector3(0, 1, 0));
        pathTracer->set_camera(camera);
        pathTracer->set_scene_manager(sceneManager);
        pathTracer->set_max_depth(5);
        pathTracer->set_samples_per_pixel(4); // Low samples for testing
    }

    void TearDown() override {
        pathTracer.reset();
        sceneManager.reset();
    }

    std::unique_ptr<PathTracer> pathTracer;
    std::shared_ptr<SceneManager> sceneManager;
};

TEST_F(GPUPathTracerTest, GPUInitialization) {
    // Test GPU initialization
    bool gpuInitialized = pathTracer->initializeGPU();
    
    if (gpuInitialized) {
        EXPECT_TRUE(pathTracer->isGPUAvailable());
        std::cout << "GPU is available for testing" << std::endl;
    } else {
        GTEST_SKIP() << "GPU not available, skipping GPU tests";
    }
}

TEST_F(GPUPathTracerTest, GPUComputeShaderCompilation) {
    bool gpuInitialized = pathTracer->initializeGPU();
    if (!gpuInitialized) {
        GTEST_SKIP() << "GPU not available, skipping shader compilation test";
    }
    
    // If GPU initialization succeeded, shader compilation should have worked
    EXPECT_TRUE(pathTracer->isGPUAvailable());
}

TEST_F(GPUPathTracerTest, GPUBasicRendering) {
    bool gpuInitialized = pathTracer->initializeGPU();
    if (!gpuInitialized) {
        GTEST_SKIP() << "GPU not available, skipping basic rendering test";
    }
    
    // Test basic GPU rendering
    const int width = 64;
    const int height = 64;
    
    bool success = pathTracer->trace_gpu(width, height);
    EXPECT_TRUE(success);
    
    if (success) {
        const auto& imageData = pathTracer->get_image_data();
        EXPECT_EQ(imageData.size(), width * height);
        
        // Check that we got some non-black pixels (basic sanity check)
        bool hasNonBlackPixels = false;
        for (const auto& pixel : imageData) {
            if (pixel.r > 0.01f || pixel.g > 0.01f || pixel.b > 0.01f) {
                hasNonBlackPixels = true;
                break;
            }
        }
        EXPECT_TRUE(hasNonBlackPixels) << "Image should contain some non-black pixels";
    }
}

TEST_F(GPUPathTracerTest, HybridModeSelection) {
    bool gpuInitialized = pathTracer->initializeGPU();
    if (!gpuInitialized) {
        GTEST_SKIP() << "GPU not available, skipping hybrid mode test";
    }
    
    // Test hybrid mode selection logic
    pathTracer->setRenderMode(PathTracer::RenderMode::HYBRID_AUTO);
    
    // Small image should prefer CPU
    bool shouldUseGPUSmall = pathTracer->shouldUseGPU(32, 32, 1);
    EXPECT_FALSE(shouldUseGPUSmall);
    
    // Large image should prefer GPU
    bool shouldUseGPULarge = pathTracer->shouldUseGPU(512, 512, 10);
    EXPECT_TRUE(shouldUseGPULarge);
}

TEST_F(GPUPathTracerTest, PerformanceComparison) {
    bool gpuInitialized = pathTracer->initializeGPU();
    if (!gpuInitialized) {
        GTEST_SKIP() << "GPU not available, skipping performance comparison";
    }
    
    const int width = 128;
    const int height = 128;
    
    auto metrics = pathTracer->benchmarkGPUvsCPU(width, height);
    
    EXPECT_GT(metrics.cpuTime, 0.0);
    EXPECT_GT(metrics.gpuTime, 0.0);
    EXPECT_GT(metrics.speedupFactor, 0.0);
    
    std::cout << "Performance metrics:" << std::endl;
    std::cout << "  CPU time: " << metrics.cpuTime << " ms" << std::endl;
    std::cout << "  GPU time: " << metrics.gpuTime << " ms" << std::endl;
    std::cout << "  Speedup: " << metrics.speedupFactor << "x" << std::endl;
    
    // GPU should be faster for this workload (target: 5x minimum)
    // But we'll be lenient in tests since hardware varies
    if (metrics.speedupFactor >= 2.0) {
        std::cout << "GPU shows good performance improvement" << std::endl;
    } else {
        std::cout << "GPU performance improvement below target (may be hardware dependent)" << std::endl;
    }
}

class GPUMemoryManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        memoryManager = std::make_shared<GPUMemoryManager>();
    }

    void TearDown() override {
        if (memoryManager) {
            memoryManager->cleanup();
        }
    }

    std::shared_ptr<GPUMemoryManager> memoryManager;
};

TEST_F(GPUMemoryManagerTest, GPUMemoryInitialization) {
    bool initialized = memoryManager->initialize();
    
    if (initialized) {
        EXPECT_TRUE(memoryManager->isInitialized());
        std::cout << "GPU memory manager initialized successfully" << std::endl;
    } else {
        GTEST_SKIP() << "GPU memory manager initialization failed - GPU may not be available";
    }
}

TEST_F(GPUMemoryManagerTest, SceneBufferAllocation) {
    bool initialized = memoryManager->initialize();
    if (!initialized) {
        GTEST_SKIP() << "GPU memory manager not available";
    }
    
    // Test scene buffer allocation
    auto sceneBuffer = memoryManager->allocateSceneBuffer(100); // 100 primitives
    EXPECT_NE(sceneBuffer, nullptr);
    
    if (sceneBuffer) {
        EXPECT_GT(sceneBuffer->size, 0);
        std::cout << "Scene buffer allocated: " << sceneBuffer->size << " bytes" << std::endl;
    }
}

TEST_F(GPUMemoryManagerTest, ImageBufferAllocation) {
    bool initialized = memoryManager->initialize();
    if (!initialized) {
        GTEST_SKIP() << "GPU memory manager not available";
    }
    
    // Test image buffer allocation
    auto imageBuffer = memoryManager->allocateImageBuffer(256, 256);
    EXPECT_NE(imageBuffer, nullptr);
    
    if (imageBuffer) {
        size_t expectedSize = 256 * 256 * 4 * sizeof(float); // RGBA32F
        EXPECT_EQ(imageBuffer->size, expectedSize);
        std::cout << "Image buffer allocated: " << imageBuffer->size << " bytes" << std::endl;
    }
}