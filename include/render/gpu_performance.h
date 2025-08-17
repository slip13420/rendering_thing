#pragma once

#include <chrono>
#include <deque>
#include <string>
#include <memory>

#ifdef USE_GPU
#include <GL/gl.h>

#ifndef GL_TIMESTAMP
#define GL_TIMESTAMP 0x8E28
#endif

#ifndef GL_TIME_ELAPSED
#define GL_TIME_ELAPSED 0x88BF
#endif

// Declare missing OpenGL functions
extern "C" {
    void glGenQueries(int n, unsigned int* ids);
    void glDeleteQueries(int n, const unsigned int* ids);
    void glQueryCounter(unsigned int id, unsigned int target);
    void glBeginQuery(unsigned int target, unsigned int id);
    void glEndQuery(unsigned int target);
    void glGetQueryObjectui64v(unsigned int id, unsigned int pname, unsigned long long* params);
    int glGetQueryObjectiv(unsigned int id, unsigned int pname, int* params);
}

#ifndef GL_QUERY_RESULT_AVAILABLE
#define GL_QUERY_RESULT_AVAILABLE 0x8864
#endif

#ifndef GL_QUERY_RESULT
#define GL_QUERY_RESULT 0x8866
#endif

#endif

class GPUPerformanceMonitor {
public:
    struct PerformanceMetrics {
        double gpuComputeTime = 0.0;          // GPU compute shader execution time (ms)
        double gpuMemoryTransferTime = 0.0;   // CPU-GPU data transfer time (ms)
        double totalGPUTime = 0.0;            // Total GPU processing time (ms)
        double cpuComputeTime = 0.0;          // CPU equivalent processing time (ms)
        double speedupRatio = 0.0;            // GPU vs CPU performance ratio
        size_t gpuMemoryUsed = 0;             // GPU memory utilization (bytes)
        double memoryTransferOverhead = 0.0;  // Memory transfer overhead percentage
        double gpuUtilization = 0.0;          // GPU utilization percentage
        double efficiency = 0.0;              // Overall efficiency metric
    };

    GPUPerformanceMonitor();
    ~GPUPerformanceMonitor();

    // GPU timing operations
    void startGPUTiming();
    void endGPUTiming();
    void recordMemoryTransfer(size_t bytes, double transferTime);
    
    // Performance metrics access
    PerformanceMetrics getMetrics() const;
    PerformanceMetrics getAverageMetrics(int samples = 10) const;
    
    // Real-time monitoring
    void updateRealTimeMetrics();
    bool isPerformanceRegression() const;
    void logPerformanceData(const std::string& scenario);
    
    // Configuration
    void setRegressionThreshold(double threshold) { regressionThreshold_ = threshold; }
    void enableDetailedLogging(bool enable) { detailedLogging_ = enable; }
    
    // Reset and cleanup
    void reset();
    void clearHistory();

private:
    bool initialized_;
    bool detailedLogging_;
    double regressionThreshold_;
    
#ifdef USE_GPU
    unsigned int timerQueries_[4];  // Start/end pairs for compute and transfer
    bool queryActive_;
    std::chrono::high_resolution_clock::time_point cpuStartTime_;
#endif
    
    PerformanceMetrics currentMetrics_;
    std::deque<PerformanceMetrics> historicalMetrics_;
    static const size_t MAX_HISTORY_SIZE = 100;
    
    // Internal methods
    void initializeQueries();
    void cleanupQueries();
    bool waitForQueryResult(unsigned int queryId) const;
    double getQueryResultMs(unsigned int queryId) const;
    void calculateDerivedMetrics();
    void updateHistory();
};