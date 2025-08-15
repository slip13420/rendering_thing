#include <gtest/gtest.h>
#include "render/gpu_compute.h"

class GPUComputeTest : public ::testing::Test {
protected:
    void SetUp() override {
        pipeline_ = std::make_unique<GPUComputePipeline>();
    }

    void TearDown() override {
        if (pipeline_) {
            pipeline_->cleanup();
        }
        pipeline_.reset();
    }

    std::unique_ptr<GPUComputePipeline> pipeline_;
};

// Test GPU pipeline initialization
TEST_F(GPUComputeTest, InitializationWithoutGPU) {
    // This test should work even without GPU support
    bool result = pipeline_->initialize();
    
#ifdef USE_GPU
    // If GPU support is compiled in, initialization may succeed or fail
    // depending on hardware availability
    if (result) {
        EXPECT_TRUE(pipeline_->isAvailable());
        EXPECT_TRUE(pipeline_->validateDriverCompatibility());
    } else {
        EXPECT_FALSE(pipeline_->isAvailable());
        EXPECT_FALSE(pipeline_->getErrorMessage().empty());
    }
#else
    // If GPU support is not compiled in, initialization should fail gracefully
    EXPECT_FALSE(result);
    EXPECT_FALSE(pipeline_->isAvailable());
    EXPECT_FALSE(pipeline_->getErrorMessage().empty());
#endif
}

// Test work group size management
TEST_F(GPUComputeTest, WorkGroupSizeManagement) {
    WorkGroupSize default_size(1, 1, 1);
    WorkGroupSize test_size(16, 16, 1);
    
    // Test default work group size
    WorkGroupSize current = pipeline_->getWorkGroupSize();
    EXPECT_EQ(current.x, default_size.x);
    EXPECT_EQ(current.y, default_size.y);
    EXPECT_EQ(current.z, default_size.z);
    
    // Test setting work group size
    pipeline_->setWorkGroupSize(test_size);
    current = pipeline_->getWorkGroupSize();
    EXPECT_EQ(current.x, test_size.x);
    EXPECT_EQ(current.y, test_size.y);
    EXPECT_EQ(current.z, test_size.z);
}

// Test shader compilation (will fail gracefully without GPU)
TEST_F(GPUComputeTest, ShaderCompilationFailure) {
    // This should fail without proper OpenGL context
    const std::string simple_compute_shader = R"(
#version 430
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

void main() {
    // Simple compute shader that does nothing
}
)";
    
    bool result = pipeline_->compileShader(simple_compute_shader);
    
#ifdef USE_GPU
    // With GPU support, this may succeed or fail depending on context
    if (!result) {
        EXPECT_FALSE(pipeline_->getErrorMessage().empty());
    }
#else
    // Without GPU support, this should fail
    EXPECT_FALSE(result);
    EXPECT_FALSE(pipeline_->getErrorMessage().empty());
#endif
}

// Test GPU capability queries
TEST_F(GPUComputeTest, CapabilityQueries) {
    // Test capability checks (should work regardless of actual GPU availability)
    bool has_compute = pipeline_->hasCapability(GPUCapability::COMPUTE_SHADERS);
    bool has_ssbo = pipeline_->hasCapability(GPUCapability::SHADER_STORAGE_BUFFER);
    bool has_atomic = pipeline_->hasCapability(GPUCapability::ATOMIC_COUNTERS);
    bool has_image = pipeline_->hasCapability(GPUCapability::IMAGE_LOAD_STORE);
    
#ifdef USE_GPU
    if (pipeline_->initialize() && pipeline_->isAvailable()) {
        // If GPU is available, all basic capabilities should be supported
        EXPECT_TRUE(has_compute);
        EXPECT_TRUE(has_ssbo);
        EXPECT_TRUE(has_atomic);
        EXPECT_TRUE(has_image);
    } else {
        // If GPU is not available, capabilities should return false
        EXPECT_FALSE(has_compute);
        EXPECT_FALSE(has_ssbo);
        EXPECT_FALSE(has_atomic);
        EXPECT_FALSE(has_image);
    }
#else
    // Without GPU support, all capabilities should return false
    EXPECT_FALSE(has_compute);
    EXPECT_FALSE(has_ssbo);
    EXPECT_FALSE(has_atomic);
    EXPECT_FALSE(has_image);
#endif
}

// Test debugging functionality
TEST_F(GPUComputeTest, DebuggingControls) {
    // Test debugging enable/disable
    EXPECT_FALSE(pipeline_->isDebuggingEnabled());
    
    pipeline_->enableDebugging(true);
    EXPECT_TRUE(pipeline_->isDebuggingEnabled());
    
    pipeline_->enableDebugging(false);
    EXPECT_FALSE(pipeline_->isDebuggingEnabled());
}

// Test dispatch functionality (will fail gracefully without GPU/context)
TEST_F(GPUComputeTest, DispatchFailureWithoutContext) {
    // Dispatch should fail without proper initialization and context
    bool result = pipeline_->dispatch(1, 1, 1);
    EXPECT_FALSE(result);
    
    WorkGroupSize work_groups(1, 1, 1);
    result = pipeline_->dispatch(work_groups);
    EXPECT_FALSE(result);
}

// Test ComputeShaderInfo structure
TEST_F(GPUComputeTest, ComputeShaderInfoStructure) {
    std::string test_source = "test shader source";
    std::string test_entry = "test_main";
    
    ComputeShaderInfo info(test_source, test_entry);
    EXPECT_EQ(info.source, test_source);
    EXPECT_EQ(info.entry_point, test_entry);
    EXPECT_TRUE(info.defines.empty());
    
    // Test default entry point
    ComputeShaderInfo default_info(test_source);
    EXPECT_EQ(default_info.source, test_source);
    EXPECT_EQ(default_info.entry_point, "main");
    EXPECT_TRUE(default_info.defines.empty());
}