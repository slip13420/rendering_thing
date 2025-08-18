#include <iostream>
#include <memory>
#include <chrono>
#include <thread>
#include <iomanip>
#include <vector>
#include <tuple>
#include "render/gpu_performance.h"
#include "render/gpu_hardware_optimizer.h" 
#include "render/hybrid_mode_selector.h"
#include "performance/gpu_benchmark.h"
#include "render/path_tracer.h"
#include "core/scene_manager.h"

int main() {
    std::cout << "=== GPU Optimization Test Suite ===" << std::endl;
    
    // Test 1: Hardware Detection
    std::cout << "\n1. Testing GPU Hardware Detection..." << std::endl;
    auto hardwareOptimizer = std::make_shared<GPUHardwareOptimizer>();
    bool hardwareDetected = hardwareOptimizer->detectHardwareCapabilities();
    
    if (hardwareDetected) {
        auto profile = hardwareOptimizer->getHardwareProfile();
        std::cout << "✓ GPU Detected: " << profile.gpuVendor << " " << profile.gpuModel << std::endl;
        std::cout << "✓ Optimal thread group size: " << profile.optimalThreadGroupSize << std::endl;
        hardwareOptimizer->optimizeForHardware();
    } else {
        std::cout << "⚠ No GPU detected or OpenGL unavailable" << std::endl;
    }
    
    // Test 2: Performance Monitor
    std::cout << "\n2. Testing GPU Performance Monitor..." << std::endl;
    auto performanceMonitor = std::make_shared<GPUPerformanceMonitor>();
    performanceMonitor->enableDetailedLogging(true);
    
    // Simulate performance measurement
    performanceMonitor->startGPUTiming();
    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Simulate work
    performanceMonitor->endGPUTiming();
    performanceMonitor->recordMemoryTransfer(1024*1024, 2.0); // 1MB transfer in 2ms
    
    auto metrics = performanceMonitor->getMetrics();
    std::cout << "✓ CPU Time: " << metrics.cpuComputeTime << "ms" << std::endl;
    std::cout << "✓ Memory Transfer: " << metrics.gpuMemoryTransferTime << "ms" << std::endl;
    
    // Test 3: Hybrid Mode Selection
    std::cout << "\n3. Testing Hybrid Mode Selection..." << std::endl;
    auto hybridSelector = std::make_shared<HybridModeSelector>();
    hybridSelector->setPerformanceMonitor(performanceMonitor);
    hybridSelector->setHardwareOptimizer(hardwareOptimizer);
    
    // Test different scenarios including high-res GPU target
    std::vector<std::tuple<int, int, int, std::string>> testScenarios = {
        {256, 256, 10, "Small Scene"},
        {512, 512, 25, "Medium Scene"}, 
        {1024, 1024, 50, "Large Scene"},
        {1280, 720, 50, "HD (GPU Target)"}
    };
    
    for (const auto& [width, height, samples, name] : testScenarios) {
        bool shouldUseGPU = hybridSelector->shouldUseGPU(width, height, samples, 100);
        double expectedSpeedup = hybridSelector->getExpectedSpeedup(width, height, samples);
        
        std::cout << "✓ " << name << " (" << width << "x" << height << ", " << samples << " samples): "
                  << (shouldUseGPU ? "GPU" : "CPU") << " (expected speedup: " 
                  << std::fixed << std::setprecision(1) << expectedSpeedup << "x)" << std::endl;
    }
    
    // Test 4: Benchmark Suite (if PathTracer is available)
    std::cout << "\n4. Testing GPU Benchmark Suite..." << std::endl;
    try {
        auto pathTracer = std::make_shared<PathTracer>();
        auto sceneManager = std::make_shared<SceneManager>();
        sceneManager->initialize();
        pathTracer->set_scene_manager(sceneManager);
        
        auto benchmarkSuite = std::make_unique<GPUBenchmarkSuite>();
        benchmarkSuite->setPathTracer(pathTracer);
        benchmarkSuite->setPerformanceMonitor(performanceMonitor);
        benchmarkSuite->setHardwareOptimizer(hardwareOptimizer);
        benchmarkSuite->setHybridModeSelector(hybridSelector);
        
        // Set up path tracer for GPU-friendly resolution
        pathTracer->set_samples_per_pixel(10); // Lower samples for faster testing
        
        // Run a high-res benchmark to trigger GPU
        std::cout << "Running HD GPU benchmark (1280x720)..." << std::endl;
        auto result = benchmarkSuite->runBenchmarkScenario("HD_GPU_Test", 1280, 720, 10, 100);
        
        if (result.errorMessage.empty()) {
            std::cout << "✓ Benchmark completed successfully!" << std::endl;
            std::cout << "  GPU Time: " << result.gpuTime << "ms" << std::endl;
            std::cout << "  CPU Time: " << result.cpuTime << "ms" << std::endl;
            std::cout << "  Speedup: " << result.speedupRatio << "x" << std::endl;
            std::cout << "  Performance Target Met: " << (result.meetsPerformanceTarget ? "YES" : "NO") << std::endl;
        } else {
            std::cout << "⚠ Benchmark failed: " << result.errorMessage << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << "⚠ Benchmark test failed: " << e.what() << std::endl;
    }
    
    std::cout << "\n=== GPU Optimization Test Complete ===" << std::endl;
    return 0;
}