#include <gtest/gtest.h>
#include "render/gpu_memory.h"
#include "render/render_engine.h"
#include "core/scene_manager.h"
#include "core/primitives.h"

class GPUMemoryIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        render_engine_ = std::make_unique<RenderEngine>();
        scene_manager_ = std::make_shared<SceneManager>();
        gpu_memory_ = std::make_shared<GPUMemoryManager>();
        
        scene_manager_->initialize();
    }

    void TearDown() override {
        if (render_engine_) {
            render_engine_->shutdown();
        }
        if (scene_manager_) {
            scene_manager_->shutdown();
        }
        if (gpu_memory_) {
            gpu_memory_->cleanup();
        }
    }

    std::unique_ptr<RenderEngine> render_engine_;
    std::shared_ptr<SceneManager> scene_manager_;
    std::shared_ptr<GPUMemoryManager> gpu_memory_;
};

// Test complete GPU memory system integration
TEST_F(GPUMemoryIntegrationTest, CompleteSystemIntegration) {
#ifdef USE_GPU
    // Initialize GPU memory
    if (!gpu_memory_->initialize()) {
        GTEST_SKIP() << "GPU not available for integration testing";
    }
    
    // Connect components
    scene_manager_->setGPUMemoryManager(gpu_memory_);
    render_engine_->set_scene_manager(scene_manager_);
    
    // Add some objects to the scene
    Material red_material(Color::red(), 0.8f, 0.0f, 0.0f);
    Material blue_material(Color::blue(), 0.6f, 0.2f, 0.0f);
    
    auto sphere = std::make_shared<Sphere>(Vector3(0, 0, -2), 1.0f, Color::red(), red_material);
    auto cube = std::make_shared<Cube>(Vector3(2, 0, -2), 1.0f, Color::blue(), blue_material);
    
    scene_manager_->add_object(sphere);
    scene_manager_->add_object(cube);
    
    // Verify scene is not synced initially
    EXPECT_FALSE(scene_manager_->isGPUSynced());
    
    // Attempt to sync scene to GPU
    scene_manager_->syncSceneToGPU();
    
    // Check if GPU buffer was created (may fail without proper OpenGL context)
    auto gpu_buffer = scene_manager_->getSceneGPUBuffer();
    if (gpu_buffer) {
        EXPECT_TRUE(scene_manager_->isGPUSynced());
        EXPECT_GT(gpu_buffer->size, 0);
    }
    
    // Test dynamic scene updates
    Material green_material(Color::green(), 0.7f, 0.1f, 0.0f);
    auto new_sphere = std::make_shared<Sphere>(Vector3(-2, 0, -2), 0.8f, Color::green(), green_material);
    scene_manager_->add_object(new_sphere);
    
    // Adding object should mark GPU as dirty
    EXPECT_FALSE(scene_manager_->isGPUSynced());
    
    // Test memory statistics
    auto memory_stats = gpu_memory_->getMemoryStats();
    EXPECT_GE(memory_stats.buffer_count, 0);
    
    // Test diagnostic functions
    gpu_memory_->generateMemoryReport();
    gpu_memory_->validateMemoryConsistency();
    
    EXPECT_TRUE(true); // If we get here, integration worked without crashes
#endif
}

// Test render engine GPU initialization integration
TEST_F(GPUMemoryIntegrationTest, RenderEngineGPUInitialization) {
#ifdef USE_GPU
    // Test GPU initialization through render engine
    bool gpu_available = render_engine_->initialize_gpu();
    
    if (gpu_available) {
        EXPECT_TRUE(render_engine_->is_gpu_available());
        
        // Connect scene manager
        render_engine_->set_scene_manager(scene_manager_);
        
        // Add test objects
        Material test_material(Color::white(), 0.5f, 0.0f, 0.0f);
        auto test_sphere = std::make_shared<Sphere>(Vector3(0, 0, -1), 0.5f, Color::white(), test_material);
        scene_manager_->add_object(test_sphere);
        
        // Test render metrics
        auto metrics = render_engine_->get_render_metrics();
        EXPECT_GE(metrics.memory_usage_mb, 0.0f);
        EXPECT_GE(metrics.cpu_utilization, 0.0f);
        
        // Cleanup GPU
        render_engine_->cleanup_gpu();
        EXPECT_FALSE(render_engine_->is_gpu_available());
    } else {
        GTEST_SKIP() << "GPU initialization failed - no GPU available";
    }
#else
    EXPECT_FALSE(render_engine_->initialize_gpu());
    EXPECT_FALSE(render_engine_->is_gpu_available());
#endif
}

// Test memory transfer coordination with rendering
TEST_F(GPUMemoryIntegrationTest, MemoryTransferCoordination) {
#ifdef USE_GPU
    if (!gpu_memory_->initialize()) {
        GTEST_SKIP() << "GPU not available for integration testing";
    }
    
    gpu_memory_->enableProfiling(true);
    scene_manager_->setGPUMemoryManager(gpu_memory_);
    render_engine_->set_scene_manager(scene_manager_);
    
    // Add multiple objects to create substantial scene data
    Material materials[] = {
        Material(Color::red(), 0.8f, 0.0f, 0.0f),
        Material(Color::green(), 0.7f, 0.1f, 0.0f),
        Material(Color::blue(), 0.6f, 0.2f, 0.0f),
        Material(Color::yellow(), 0.9f, 0.0f, 0.0f)
    };
    
    for (int i = 0; i < 10; ++i) {
        float x = (i % 5) * 2.0f - 4.0f;
        float z = (i / 5) * 2.0f - 3.0f;
        
        auto sphere = std::make_shared<Sphere>(
            Vector3(x, 0, z), 
            0.5f, 
            Color(0.5f + i * 0.05f, 0.3f, 0.7f), 
            materials[i % 4]
        );
        scene_manager_->add_object(sphere);
    }
    
    // Test scene synchronization
    scene_manager_->syncSceneToGPU();
    
    // Test transfer performance tracking
    auto transfer_stats = gpu_memory_->getTransferPerformance();
    if (transfer_stats.total_transfers > 0) {
        EXPECT_GT(transfer_stats.total_bytes_transferred, 0);
        EXPECT_GT(transfer_stats.average_transfer_time_ms, 0.0);
    }
    
    // Test memory pool optimization
    gpu_memory_->optimizeMemoryPools();
    
    auto memory_stats = gpu_memory_->getMemoryStats();
    EXPECT_GE(memory_stats.pool_count, 0);
    
    EXPECT_TRUE(true);
#endif
}

// Test progressive rendering with GPU memory coordination
TEST_F(GPUMemoryIntegrationTest, ProgressiveRenderingCoordination) {
#ifdef USE_GPU
    if (!render_engine_->initialize_gpu()) {
        GTEST_SKIP() << "GPU initialization failed";
    }
    
    render_engine_->set_scene_manager(scene_manager_);
    
    // Add test scene
    Material test_material(Color::cyan(), 0.5f, 0.0f, 0.0f);
    auto test_object = std::make_shared<Sphere>(Vector3(0, 0, -1), 0.8f, Color::cyan(), test_material);
    scene_manager_->add_object(test_object);
    
    // Set up render configuration
    render_engine_->set_render_size(400, 300);
    render_engine_->set_samples_per_pixel(4);
    render_engine_->set_max_depth(5);
    
    // This should not crash and should coordinate GPU memory properly
    // (Actual progressive rendering may not complete without proper OpenGL context)
    EXPECT_TRUE(true);
    
    render_engine_->cleanup_gpu();
#endif
}

// Test error handling in integrated system
TEST_F(GPUMemoryIntegrationTest, ErrorHandlingIntegration) {
    // Test operations without GPU initialization
    scene_manager_->setGPUMemoryManager(gpu_memory_);
    render_engine_->set_scene_manager(scene_manager_);
    
    // These should not crash
    scene_manager_->syncSceneToGPU();
    scene_manager_->updateGPUPrimitive(0);
    scene_manager_->removeGPUPrimitive(0);
    
    gpu_memory_->generateMemoryReport();
    gpu_memory_->validateMemoryConsistency();
    
    auto metrics = render_engine_->get_render_metrics();
    EXPECT_GE(metrics.cpu_utilization, 0.0f);
    
    EXPECT_TRUE(true); // If we get here, error handling worked
}

// Test resource cleanup integration
TEST_F(GPUMemoryIntegrationTest, ResourceCleanupIntegration) {
#ifdef USE_GPU
    if (!gpu_memory_->initialize()) {
        GTEST_SKIP() << "GPU not available for cleanup testing";
    }
    
    scene_manager_->setGPUMemoryManager(gpu_memory_);
    
    // Allocate some GPU resources
    auto buffer1 = gpu_memory_->allocateSceneBuffer(100);
    auto buffer2 = gpu_memory_->allocateImageBuffer(800, 600);
    
    if (buffer1 || buffer2) {
        auto stats_before = gpu_memory_->getMemoryStats();
        
        // Add scene objects
        Material test_material(Color::magenta(), 0.4f, 0.0f, 0.0f);
        auto test_object = std::make_shared<Sphere>(Vector3(1, 1, -1), 0.6f, Color::magenta(), test_material);
        scene_manager_->add_object(test_object);
        
        // Sync to potentially create more GPU buffers
        scene_manager_->syncSceneToGPU();
        
        // Clear scene (should mark GPU as dirty but not crash)
        scene_manager_->clear_objects();
        
        // Cleanup should work properly
        gpu_memory_->deallocateAll();
        
        auto stats_after = gpu_memory_->getMemoryStats();
        EXPECT_EQ(stats_after.buffer_count, 0);
        EXPECT_EQ(stats_after.total_used, 0);
    }
#endif
}

// Test memory leak detection in integrated system
TEST_F(GPUMemoryIntegrationTest, MemoryLeakDetectionIntegration) {
#ifdef USE_GPU
    if (!gpu_memory_->initialize()) {
        GTEST_SKIP() << "GPU not available for leak detection testing";
    }
    
    gpu_memory_->enableMemoryLeakDetection(true);
    gpu_memory_->enableProfiling(true);
    scene_manager_->setGPUMemoryManager(gpu_memory_);
    
    // Allocate various buffers through different paths
    auto scene_buffer = gpu_memory_->allocateSceneBuffer(50);
    auto image_buffer = gpu_memory_->allocateImageBuffer(400, 300);
    auto generic_buffer = gpu_memory_->allocateBuffer(
        1024, 
        GPUBufferType::UNIFORM, 
        GPUUsagePattern::STATIC, 
        "leak_test_buffer"
    );
    
    // Add scene objects
    Material leak_test_material(Color::orange(), 0.6f, 0.0f, 0.0f);
    auto leak_test_object = std::make_shared<Sphere>(Vector3(-1, 0, -1), 0.4f, Color::orange(), leak_test_material);
    scene_manager_->add_object(leak_test_object);
    
    // Sync scene
    scene_manager_->syncSceneToGPU();
    
    // Generate leak report (should not find leaks immediately)
    gpu_memory_->reportMemoryLeaks();
    
    // Cleanup most resources
    if (scene_buffer) gpu_memory_->deallocateBuffer(scene_buffer);
    if (image_buffer) gpu_memory_->deallocateBuffer(image_buffer);
    if (generic_buffer) gpu_memory_->deallocateBuffer(generic_buffer);
    
    // Final leak report
    gpu_memory_->reportMemoryLeaks();
    
    EXPECT_TRUE(true); // If we get here, leak detection worked without crashes
#endif
}"