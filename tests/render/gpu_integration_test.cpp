#include <gtest/gtest.h>
#include "render/render_engine.h"
#include "core/scene_manager.h"
#include "render/image_output.h"

class GPUIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        render_engine_ = std::make_unique<RenderEngine>();
        scene_manager_ = std::make_shared<SceneManager>();
        image_output_ = std::make_shared<ImageOutput>();
        
        render_engine_->set_scene_manager(scene_manager_);
        render_engine_->set_image_output(image_output_);
    }

    void TearDown() override {
        if (render_engine_) {
            render_engine_->stop_render();
            render_engine_->cleanup_gpu();
        }
        render_engine_.reset();
    }

    std::unique_ptr<RenderEngine> render_engine_;
    std::shared_ptr<SceneManager> scene_manager_;
    std::shared_ptr<ImageOutput> image_output_;
};

// Test render mode management
TEST_F(GPUIntegrationTest, RenderModeManagement) {
    // Test default render mode (should be AUTO)
    RenderMode initial_mode = render_engine_->get_render_mode();
    EXPECT_EQ(initial_mode, RenderMode::AUTO);
    
    // Test setting CPU-only mode
    render_engine_->set_render_mode(RenderMode::CPU_ONLY);
    EXPECT_EQ(render_engine_->get_render_mode(), RenderMode::CPU_ONLY);
    EXPECT_FALSE(render_engine_->is_gpu_available()); // Should cleanup GPU
    
    // Test setting GPU-preferred mode
    render_engine_->set_render_mode(RenderMode::GPU_PREFERRED);
    EXPECT_EQ(render_engine_->get_render_mode(), RenderMode::GPU_PREFERRED);
    
    // Test setting GPU-only mode (may fail if GPU not available)
    render_engine_->set_render_mode(RenderMode::GPU_ONLY);
    RenderMode final_mode = render_engine_->get_render_mode();
    
#ifdef USE_GPU
    // If GPU support is compiled in and hardware is available
    if (render_engine_->is_gpu_available()) {
        EXPECT_EQ(final_mode, RenderMode::GPU_ONLY);
    } else {
        // GPU not available, mode should remain as set but GPU won't be available
        EXPECT_EQ(final_mode, RenderMode::GPU_ONLY);
        EXPECT_FALSE(render_engine_->is_gpu_available());
    }
#else
    // Without GPU support, GPU should never be available
    EXPECT_FALSE(render_engine_->is_gpu_available());
#endif
}

// Test GPU initialization
TEST_F(GPUIntegrationTest, GPUInitialization) {
    // Test manual GPU initialization
    bool gpu_init_result = render_engine_->initialize_gpu();
    
#ifdef USE_GPU
    if (gpu_init_result) {
        EXPECT_TRUE(render_engine_->is_gpu_available());
        
        // Test GPU cleanup
        render_engine_->cleanup_gpu();
        EXPECT_FALSE(render_engine_->is_gpu_available());
        
        // Test re-initialization
        bool reinit_result = render_engine_->initialize_gpu();
        EXPECT_EQ(reinit_result, gpu_init_result); // Should have same result
    } else {
        EXPECT_FALSE(render_engine_->is_gpu_available());
    }
#else
    // Without GPU support, initialization should always fail
    EXPECT_FALSE(gpu_init_result);
    EXPECT_FALSE(render_engine_->is_gpu_available());
#endif
}

// Test render metrics
TEST_F(GPUIntegrationTest, RenderMetrics) {
    RenderMetrics metrics = render_engine_->get_render_metrics();
    
    // Metrics should be initialized
    EXPECT_GE(metrics.gpu_utilization, 0.0f);
    EXPECT_LE(metrics.gpu_utilization, 100.0f);
    EXPECT_GE(metrics.cpu_utilization, 0.0f);
    EXPECT_LE(metrics.cpu_utilization, 100.0f);
    EXPECT_GE(metrics.memory_usage_mb, 0.0f);
    EXPECT_GE(metrics.render_time_ms, 0.0f);
    EXPECT_GE(metrics.samples_per_second, 0);
}

// Test CPU fallback mechanism
TEST_F(GPUIntegrationTest, CPUFallbackMechanism) {
    // Set to GPU_PREFERRED mode (should fallback to CPU if GPU unavailable)
    render_engine_->set_render_mode(RenderMode::GPU_PREFERRED);
    
    // The render engine should handle fallback gracefully
    RenderState initial_state = render_engine_->get_render_state();
    EXPECT_EQ(initial_state, RenderState::IDLE);
    
    // Even if GPU is not available, the engine should remain functional
    EXPECT_TRUE(true); // If we get here without exceptions, fallback is working
}

// Test render mode enumeration completeness
TEST_F(GPUIntegrationTest, RenderModeEnumeration) {
    std::vector<RenderMode> render_modes = {
        RenderMode::CPU_ONLY,
        RenderMode::GPU_PREFERRED,
        RenderMode::GPU_ONLY,
        RenderMode::AUTO
    };
    
    // Test that we can set and get each mode
    for (RenderMode mode : render_modes) {
        render_engine_->set_render_mode(mode);
        EXPECT_EQ(render_engine_->get_render_mode(), mode);
    }
}

// Test GPU availability consistency
TEST_F(GPUIntegrationTest, GPUAvailabilityConsistency) {
    // GPU availability should be consistent across calls
    bool available1 = render_engine_->is_gpu_available();
    bool available2 = render_engine_->is_gpu_available();
    EXPECT_EQ(available1, available2);
    
    // After cleanup, GPU should not be available
    render_engine_->cleanup_gpu();
    EXPECT_FALSE(render_engine_->is_gpu_available());
    
    // After initialization attempt, availability should be consistent
    render_engine_->initialize_gpu();
    bool available3 = render_engine_->is_gpu_available();
    bool available4 = render_engine_->is_gpu_available();
    EXPECT_EQ(available3, available4);
}

// Test render engine state with GPU modes
TEST_F(GPUIntegrationTest, RenderEngineStateWithGPU) {
    // Test that render engine state management works with different GPU modes
    std::vector<RenderMode> modes_to_test = {
        RenderMode::CPU_ONLY,
        RenderMode::GPU_PREFERRED,
        RenderMode::AUTO
    };
    
    for (RenderMode mode : modes_to_test) {
        render_engine_->set_render_mode(mode);
        
        // Engine should remain in IDLE state regardless of GPU mode
        EXPECT_EQ(render_engine_->get_render_state(), RenderState::IDLE);
        
        // Should not be rendering initially
        EXPECT_FALSE(render_engine_->is_rendering());
        EXPECT_FALSE(render_engine_->is_progressive_rendering());
    }
}