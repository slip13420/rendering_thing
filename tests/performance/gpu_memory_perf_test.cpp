#include <gtest/gtest.h>
#include "render/gpu_memory.h"
#include <chrono>
#include <vector>
#include <random>

class GPUMemoryPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        gpu_memory_ = std::make_unique<GPUMemoryManager>();
        gpu_memory_->enableProfiling(true);
    }

    void TearDown() override {
        if (gpu_memory_) {
            gpu_memory_->cleanup();
        }
    }

    std::unique_ptr<GPUMemoryManager> gpu_memory_;
    
    // Helper to generate test data
    std::vector<float> generateTestData(size_t count) {
        std::vector<float> data(count);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(0.0f, 1.0f);
        
        for (auto& value : data) {
            value = dis(gen);
        }
        return data;
    }
};

// Test memory allocation performance
TEST_F(GPUMemoryPerformanceTest, AllocationPerformance) {
#ifdef USE_GPU
    if (!gpu_memory_->initialize()) {
        GTEST_SKIP() << "GPU not available for performance testing";
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::vector<std::shared_ptr<GPUBuffer>> buffers;
    const int num_allocations = 100;
    
    // Allocate multiple buffers
    for (int i = 0; i < num_allocations; ++i) {
        auto buffer = gpu_memory_->allocateBuffer(
            1024 * 1024, // 1MB each
            GPUBufferType::SHADER_STORAGE,
            GPUUsagePattern::DYNAMIC,
            "perf_test_" + std::to_string(i)
        );
        if (buffer) {
            buffers.push_back(buffer);
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "Allocated " << buffers.size() << " buffers in " << duration.count() << "ms" << std::endl;
    
    // Cleanup
    for (auto& buffer : buffers) {
        gpu_memory_->deallocateBuffer(buffer);
    }
    
    // Performance should be reasonable (less than 1 second for 100 allocations)
    EXPECT_LT(duration.count(), 1000);
#endif
}

// Test data transfer performance
TEST_F(GPUMemoryPerformanceTest, DataTransferPerformance) {
#ifdef USE_GPU
    if (!gpu_memory_->initialize()) {
        GTEST_SKIP() << "GPU not available for performance testing";
    }
    
    // Allocate large buffer for testing
    const size_t data_size = 10 * 1024 * 1024; // 10MB
    auto buffer = gpu_memory_->allocateBuffer(
        data_size * sizeof(float),
        GPUBufferType::SHADER_STORAGE,
        GPUUsagePattern::DYNAMIC,
        "transfer_perf_test"
    );
    
    if (!buffer) {
        GTEST_SKIP() << "Failed to allocate buffer for transfer test";
    }
    
    // Generate test data
    auto test_data = generateTestData(data_size);
    
    // Measure transfer performance
    auto start_time = std::chrono::high_resolution_clock::now();
    
    bool transfer_success = gpu_memory_->transferSceneData(buffer, test_data);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    if (transfer_success) {
        double transfer_time_ms = duration.count() / 1000.0;
        double data_size_mb = (data_size * sizeof(float)) / (1024.0 * 1024.0);
        double throughput_mb_s = data_size_mb / (transfer_time_ms / 1000.0);
        
        std::cout << "Transferred " << data_size_mb << "MB in " << transfer_time_ms 
                  << "ms (throughput: " << throughput_mb_s << " MB/s)" << std::endl;
        
        // Verify transfer stats were updated
        auto transfer_stats = gpu_memory_->getTransferPerformance();
        EXPECT_GT(transfer_stats.total_transfers, 0);
        EXPECT_GT(transfer_stats.total_bytes_transferred, 0);
        
        // Performance should be reasonable (>50 MB/s for local GPU)
        EXPECT_GT(throughput_mb_s, 50.0);
    }
    
    gpu_memory_->deallocateBuffer(buffer);
#endif
}

// Test memory pool performance
TEST_F(GPUMemoryPerformanceTest, MemoryPoolPerformance) {
#ifdef USE_GPU
    if (!gpu_memory_->initialize()) {
        GTEST_SKIP() << "GPU not available for performance testing";
    }
    
    // Create memory pool
    gpu_memory_->createMemoryPool(
        1024 * 1024, // 1MB buffers
        20, // max 20 buffers
        GPUBufferType::SHADER_STORAGE,
        GPUUsagePattern::DYNAMIC
    );
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::vector<std::shared_ptr<GPUBuffer>> buffers;
    const int num_allocations = 50;
    
    // Allocate from pool
    for (int i = 0; i < num_allocations; ++i) {
        auto buffer = gpu_memory_->allocateFromPool(
            1024 * 1024,
            GPUBufferType::SHADER_STORAGE,
            GPUUsagePattern::DYNAMIC
        );
        if (buffer) {
            buffers.push_back(buffer);
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "Pool-allocated " << buffers.size() << " buffers in " << duration.count() << "ms" << std::endl;
    
    // Cleanup
    for (auto& buffer : buffers) {
        gpu_memory_->deallocateBuffer(buffer);
    }
    
    // Pool allocation should be fast
    EXPECT_LT(duration.count(), 500); // Less than 500ms
#endif
}

// Test batched transfer performance
TEST_F(GPUMemoryPerformanceTest, BatchedTransferPerformance) {
#ifdef USE_GPU
    if (!gpu_memory_->initialize()) {
        GTEST_SKIP() << "GPU not available for performance testing";
    }
    
    const int num_buffers = 10;
    const size_t buffer_size = 1024 * 1024; // 1MB each
    
    std::vector<std::shared_ptr<GPUBuffer>> buffers;
    std::vector<std::vector<float>> test_data_sets;
    std::vector<std::pair<std::shared_ptr<GPUBuffer>, const void*>> transfer_pairs;
    
    // Prepare buffers and data
    for (int i = 0; i < num_buffers; ++i) {
        auto buffer = gpu_memory_->allocateBuffer(
            buffer_size * sizeof(float),
            GPUBufferType::SHADER_STORAGE,
            GPUUsagePattern::DYNAMIC,
            "batch_test_" + std::to_string(i)
        );
        
        if (buffer) {
            buffers.push_back(buffer);
            test_data_sets.push_back(generateTestData(buffer_size));\n            transfer_pairs.emplace_back(buffer, test_data_sets.back().data());\n        }\n    }\n    \n    if (transfer_pairs.empty()) {\n        GTEST_SKIP() << \"No buffers allocated for batch test\";\n    }\n    \n    // Measure batched transfer\n    auto start_time = std::chrono::high_resolution_clock::now();\n    \n    bool batch_success = gpu_memory_->transferBatched(transfer_pairs);\n    \n    auto end_time = std::chrono::high_resolution_clock::now();\n    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);\n    \n    if (batch_success) {\n        double total_mb = (transfer_pairs.size() * buffer_size * sizeof(float)) / (1024.0 * 1024.0);\n        std::cout << \"Batch transferred \" << total_mb << \"MB in \" << duration.count() << \"ms\" << std::endl;\n        \n        // Verify performance tracking\n        auto transfer_stats = gpu_memory_->getTransferPerformance();\n        EXPECT_GT(transfer_stats.total_transfers, 0);\n    }\n    \n    // Cleanup\n    for (auto& buffer : buffers) {\n        gpu_memory_->deallocateBuffer(buffer);\n    }\n#endif\n}\n\n// Test memory optimization performance\nTEST_F(GPUMemoryPerformanceTest, MemoryOptimizationPerformance) {\n#ifdef USE_GPU\n    if (!gpu_memory_->initialize()) {\n        GTEST_SKIP() << \"GPU not available for performance testing\";\n    }\n    \n    // Create several memory pools\n    for (int i = 0; i < 5; ++i) {\n        gpu_memory_->createMemoryPool(\n            (1 << (10 + i)) * 1024, // Powers of 2: 1MB, 2MB, 4MB, etc.\n            10,\n            GPUBufferType::SHADER_STORAGE,\n            GPUUsagePattern::DYNAMIC\n        );\n    }\n    \n    // Allocate and deallocate many buffers to create fragmentation\n    std::vector<std::shared_ptr<GPUBuffer>> buffers;\n    for (int i = 0; i < 50; ++i) {\n        auto buffer = gpu_memory_->allocateBuffer(\n            1024 * (1 + i % 10),\n            GPUBufferType::SHADER_STORAGE,\n            GPUUsagePattern::DYNAMIC\n        );\n        if (buffer) {\n            buffers.push_back(buffer);\n        }\n    }\n    \n    // Deallocate every other buffer to create fragmentation\n    for (size_t i = 1; i < buffers.size(); i += 2) {\n        gpu_memory_->deallocateBuffer(buffers[i]);\n    }\n    \n    // Measure optimization performance\n    auto start_time = std::chrono::high_resolution_clock::now();\n    \n    gpu_memory_->optimizeMemoryPools();\n    gpu_memory_->defragmentMemoryPools();\n    \n    auto end_time = std::chrono::high_resolution_clock::now();\n    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);\n    \n    std::cout << \"Memory optimization completed in \" << duration.count() << \"ms\" << std::endl;\n    \n    // Optimization should be reasonably fast\n    EXPECT_LT(duration.count(), 100); // Less than 100ms\n    \n    // Cleanup remaining buffers\n    for (size_t i = 0; i < buffers.size(); i += 2) {\n        gpu_memory_->deallocateBuffer(buffers[i]);\n    }\n#endif\n}\n\n// Test diagnostic performance\nTEST_F(GPUMemoryPerformanceTest, DiagnosticPerformance) {\n    auto start_time = std::chrono::high_resolution_clock::now();\n    \n    // Run all diagnostic functions\n    gpu_memory_->generateMemoryReport();\n    gpu_memory_->validateMemoryConsistency();\n    gpu_memory_->dumpMemoryPoolStatus();\n    gpu_memory_->reportMemoryLeaks();\n    \n    auto end_time = std::chrono::high_resolution_clock::now();\n    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);\n    \n    std::cout << \"Diagnostic operations completed in \" << duration.count() << \"ms\" << std::endl;\n    \n    // Diagnostics should be very fast\n    EXPECT_LT(duration.count(), 50); // Less than 50ms\n}"