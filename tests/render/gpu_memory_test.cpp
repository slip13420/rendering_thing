#include <gtest/gtest.h>
#include "render/gpu_memory.h"
#include <vector>

class GPUMemoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        memory_manager_ = std::make_unique<GPUMemoryManager>();
    }

    void TearDown() override {
        if (memory_manager_) {
            memory_manager_->cleanup();
        }
        memory_manager_.reset();
    }

    std::unique_ptr<GPUMemoryManager> memory_manager_;
};

// Test memory manager initialization
TEST_F(GPUMemoryTest, InitializationWithoutGPU) {
    EXPECT_FALSE(memory_manager_->isInitialized());
    
    bool result = memory_manager_->initialize();
    
#ifdef USE_GPU
    // With GPU support, initialization may succeed or fail depending on hardware
    if (result) {
        EXPECT_TRUE(memory_manager_->isInitialized());
    } else {
        EXPECT_FALSE(memory_manager_->isInitialized());
        EXPECT_FALSE(memory_manager_->getErrorMessage().empty());
    }
#else
    // Without GPU support, initialization should fail gracefully
    EXPECT_FALSE(result);
    EXPECT_FALSE(memory_manager_->isInitialized());
    EXPECT_FALSE(memory_manager_->getErrorMessage().empty());
#endif
}

// Test buffer allocation without GPU
TEST_F(GPUMemoryTest, BufferAllocationWithoutInitialization) {
    // Should fail when not initialized
    auto buffer = memory_manager_->allocateBuffer(
        1024, 
        GPUBufferType::SHADER_STORAGE,
        GPUUsagePattern::STATIC,
        "test_buffer"
    );
    
    EXPECT_EQ(buffer, nullptr);
    EXPECT_FALSE(memory_manager_->getErrorMessage().empty());
}

// Test memory statistics
TEST_F(GPUMemoryTest, MemoryStatistics) {
    GPUMemoryStats initial_stats = memory_manager_->getMemoryStats();
    
    // Initial stats should be zero
    EXPECT_EQ(initial_stats.total_allocated, 0);
    EXPECT_EQ(initial_stats.total_used, 0);
    EXPECT_EQ(initial_stats.peak_usage, 0);
    EXPECT_EQ(initial_stats.buffer_count, 0);
    EXPECT_FLOAT_EQ(initial_stats.fragmentation_ratio, 0.0f);
}

// Test profiling functionality
TEST_F(GPUMemoryTest, ProfilingControls) {
    EXPECT_FALSE(memory_manager_->isProfilingEnabled());
    
    memory_manager_->enableProfiling(true);
    EXPECT_TRUE(memory_manager_->isProfilingEnabled());
    
    memory_manager_->enableProfiling(false);
    EXPECT_FALSE(memory_manager_->isProfilingEnabled());
}

// Test memory validation
TEST_F(GPUMemoryTest, MemoryValidation) {
    // Should work even without initialization
    bool can_allocate_small = memory_manager_->validateMemoryAvailable(1024);
    bool can_allocate_huge = memory_manager_->validateMemoryAvailable(1024 * 1024 * 1024); // 1GB
    
    // Without initialization, validation should return false
    EXPECT_FALSE(can_allocate_small);
    EXPECT_FALSE(can_allocate_huge);
}

// Test garbage collection and defragmentation
TEST_F(GPUMemoryTest, GarbageCollectionAndDefragmentation) {
    // These should not crash even without initialization
    memory_manager_->garbageCollect();
    memory_manager_->defragment();
    
    // Should work without throwing exceptions
    EXPECT_TRUE(true); // If we get here, no exceptions were thrown
}

// Test buffer type enumeration
TEST_F(GPUMemoryTest, BufferTypeEnumeration) {
    // Test that all buffer types are valid enum values
    std::vector<GPUBufferType> buffer_types = {
        GPUBufferType::SHADER_STORAGE,
        GPUBufferType::UNIFORM,
        GPUBufferType::VERTEX,
        GPUBufferType::INDEX,
        GPUBufferType::ATOMIC_COUNTER,
        GPUBufferType::TEXTURE
    };
    
    // Each type should be distinct
    for (size_t i = 0; i < buffer_types.size(); ++i) {
        for (size_t j = i + 1; j < buffer_types.size(); ++j) {
            EXPECT_NE(buffer_types[i], buffer_types[j]);
        }
    }
}

// Test usage pattern enumeration
TEST_F(GPUMemoryTest, UsagePatternEnumeration) {
    std::vector<GPUUsagePattern> usage_patterns = {
        GPUUsagePattern::STATIC,
        GPUUsagePattern::DYNAMIC,
        GPUUsagePattern::STREAM
    };
    
    // Each pattern should be distinct
    for (size_t i = 0; i < usage_patterns.size(); ++i) {
        for (size_t j = i + 1; j < usage_patterns.size(); ++j) {
            EXPECT_NE(usage_patterns[i], usage_patterns[j]);
        }
    }
}

// Test GPUBuffer structure
TEST_F(GPUMemoryTest, GPUBufferStructure) {
    GPUBuffer buffer;
    
    // Test default initialization
    EXPECT_EQ(buffer.size, 0);
    EXPECT_FALSE(buffer.mapped);
    EXPECT_EQ(buffer.mapped_pointer, nullptr);
    EXPECT_TRUE(buffer.name.empty());
    
#ifdef USE_GPU
    EXPECT_EQ(buffer.id, 0);
    EXPECT_EQ(buffer.target, 0);
    EXPECT_EQ(buffer.usage, 0);
#endif
}

// Test deallocate all functionality
TEST_F(GPUMemoryTest, DeallocateAll) {
    // Should work without crashing even when not initialized
    memory_manager_->deallocateAll();
    
    GPUMemoryStats stats = memory_manager_->getMemoryStats();
    EXPECT_EQ(stats.buffer_count, 0);
    EXPECT_EQ(stats.total_allocated, 0);
    EXPECT_EQ(stats.total_used, 0);
}
// Test new scene-specific buffer allocation
TEST_F(GPUMemoryTest, SceneBufferAllocation) {
    auto scene_buffer = memory_manager_->allocateSceneBuffer(100);
    EXPECT_EQ(scene_buffer, nullptr);
    
    auto image_buffer = memory_manager_->allocateImageBuffer(800, 600);
    EXPECT_EQ(image_buffer, nullptr);
}

// Test memory pool functionality
TEST_F(GPUMemoryTest, MemoryPoolFunctionality) {
    memory_manager_->createMemoryPool(1024, 4, GPUBufferType::SHADER_STORAGE, GPUUsagePattern::DYNAMIC);
    memory_manager_->optimizeMemoryPools();
    memory_manager_->defragmentMemoryPools();
    EXPECT_TRUE(true);
}

// Test transfer performance tracking
TEST_F(GPUMemoryTest, TransferPerformanceTracking) {
    auto transfer_stats = memory_manager_->getTransferPerformance();
    EXPECT_EQ(transfer_stats.total_transfers, 0);
    EXPECT_EQ(transfer_stats.total_bytes_transferred, 0);
    
    memory_manager_->resetTransferStats();
    EXPECT_TRUE(true);
}

// Test diagnostic functionality
TEST_F(GPUMemoryTest, DiagnosticFunctionality) {
    memory_manager_->enableMemoryLeakDetection(true);
    memory_manager_->reportMemoryLeaks();
    memory_manager_->generateMemoryReport();
    memory_manager_->validateMemoryConsistency();
    memory_manager_->dumpMemoryPoolStatus();
    EXPECT_TRUE(true);
}
