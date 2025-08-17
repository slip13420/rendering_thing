#include "render/gpu_performance.h"
#include <iostream>
#include <algorithm>
#include <numeric>

GPUPerformanceMonitor::GPUPerformanceMonitor()
    : initialized_(false)
    , detailedLogging_(false)
    , regressionThreshold_(0.15) // 15% regression threshold
#ifdef USE_GPU
    , queryActive_(false)
#endif
{
    initializeQueries();
}

GPUPerformanceMonitor::~GPUPerformanceMonitor() {
    cleanupQueries();
}

void GPUPerformanceMonitor::initializeQueries() {
#ifdef USE_GPU
    // Initialize timer queries for GPU performance measurement
    glGenQueries(4, timerQueries_);
    initialized_ = true;
    
    if (detailedLogging_) {
        std::cout << "GPU Performance Monitor initialized with timer queries" << std::endl;
    }
#else
    initialized_ = false;
    if (detailedLogging_) {
        std::cout << "GPU Performance Monitor initialized in CPU-only mode" << std::endl;
    }
#endif
}

void GPUPerformanceMonitor::cleanupQueries() {
#ifdef USE_GPU
    if (initialized_) {
        glDeleteQueries(4, timerQueries_);
        initialized_ = false;
    }
#endif
}

void GPUPerformanceMonitor::startGPUTiming() {
#ifdef USE_GPU
    if (!initialized_) return;
    
    // Record CPU start time
    cpuStartTime_ = std::chrono::high_resolution_clock::now();
    
    // Start GPU compute timing
    glQueryCounter(timerQueries_[0], GL_TIMESTAMP);
    queryActive_ = true;
    
    if (detailedLogging_) {
        std::cout << "Started GPU performance timing" << std::endl;
    }
#endif
}

void GPUPerformanceMonitor::endGPUTiming() {
#ifdef USE_GPU
    if (!initialized_ || !queryActive_) return;
    
    // End GPU compute timing
    glQueryCounter(timerQueries_[1], GL_TIMESTAMP);
    
    // Calculate CPU time
    auto cpuEndTime = std::chrono::high_resolution_clock::now();
    auto cpuDuration = std::chrono::duration_cast<std::chrono::microseconds>(cpuEndTime - cpuStartTime_);
    currentMetrics_.cpuComputeTime = cpuDuration.count() / 1000.0; // Convert to ms
    
    // Wait for GPU queries to complete and get results
    if (waitForQueryResult(timerQueries_[1])) {
        unsigned long long startTime, endTime;
        glGetQueryObjectui64v(timerQueries_[0], GL_QUERY_RESULT, &startTime);
        glGetQueryObjectui64v(timerQueries_[1], GL_QUERY_RESULT, &endTime);
        
        // Convert nanoseconds to milliseconds
        currentMetrics_.gpuComputeTime = (endTime - startTime) / 1000000.0;
        currentMetrics_.totalGPUTime = currentMetrics_.gpuComputeTime + currentMetrics_.gpuMemoryTransferTime;
        
        calculateDerivedMetrics();
        updateHistory();
        
        if (detailedLogging_) {
            std::cout << "GPU compute time: " << currentMetrics_.gpuComputeTime << "ms, "
                      << "CPU time: " << currentMetrics_.cpuComputeTime << "ms, "
                      << "Speedup: " << currentMetrics_.speedupRatio << "x" << std::endl;
        }
    }
    
    queryActive_ = false;
#endif
}

void GPUPerformanceMonitor::recordMemoryTransfer(size_t bytes, double transferTime) {
    currentMetrics_.gpuMemoryTransferTime += transferTime;
    currentMetrics_.gpuMemoryUsed += bytes;
    
    // Update total GPU time
    currentMetrics_.totalGPUTime = currentMetrics_.gpuComputeTime + currentMetrics_.gpuMemoryTransferTime;
    
    // Calculate transfer overhead percentage
    if (currentMetrics_.totalGPUTime > 0) {
        currentMetrics_.memoryTransferOverhead = 
            (currentMetrics_.gpuMemoryTransferTime / currentMetrics_.totalGPUTime) * 100.0;
    }
    
    if (detailedLogging_) {
        std::cout << "Recorded memory transfer: " << bytes << " bytes in " << transferTime 
                  << "ms (overhead: " << currentMetrics_.memoryTransferOverhead << "%)" << std::endl;
    }
}

bool GPUPerformanceMonitor::waitForQueryResult(unsigned int queryId) const {
#ifdef USE_GPU
    int available = 0;
    int attempts = 0;
    const int maxAttempts = 1000; // Prevent infinite loops
    
    do {
        glGetQueryObjectiv(queryId, GL_QUERY_RESULT_AVAILABLE, &available);
        attempts++;
        if (attempts > maxAttempts) {
            if (detailedLogging_) {
                std::cout << "Warning: GPU query timeout after " << maxAttempts << " attempts" << std::endl;
            }
            return false;
        }
    } while (!available);
    
    return true;
#else
    return false;
#endif
}

double GPUPerformanceMonitor::getQueryResultMs(unsigned int queryId) const {
#ifdef USE_GPU
    unsigned long long result;
    glGetQueryObjectui64v(queryId, GL_QUERY_RESULT, &result);
    return result / 1000000.0; // Convert nanoseconds to milliseconds
#else
    return 0.0;
#endif
}

void GPUPerformanceMonitor::calculateDerivedMetrics() {
    // Calculate speedup ratio
    if (currentMetrics_.cpuComputeTime > 0 && currentMetrics_.gpuComputeTime > 0) {
        currentMetrics_.speedupRatio = currentMetrics_.cpuComputeTime / currentMetrics_.gpuComputeTime;
    } else {
        currentMetrics_.speedupRatio = 0.0;
    }
    
    // Calculate GPU utilization (simplified metric)
    // In practice, this would use more sophisticated GPU monitoring
    if (currentMetrics_.gpuComputeTime > 0 && currentMetrics_.totalGPUTime > 0) {
        currentMetrics_.gpuUtilization = 
            (currentMetrics_.gpuComputeTime / currentMetrics_.totalGPUTime) * 100.0;
    }
    
    // Calculate efficiency metric (speedup relative to memory overhead)
    if (currentMetrics_.speedupRatio > 0 && currentMetrics_.memoryTransferOverhead < 100.0) {
        currentMetrics_.efficiency = 
            currentMetrics_.speedupRatio * (1.0 - currentMetrics_.memoryTransferOverhead / 100.0);
    }
}

void GPUPerformanceMonitor::updateHistory() {
    historicalMetrics_.push_back(currentMetrics_);
    
    // Maintain maximum history size
    if (historicalMetrics_.size() > MAX_HISTORY_SIZE) {
        historicalMetrics_.pop_front();
    }
}

GPUPerformanceMonitor::PerformanceMetrics GPUPerformanceMonitor::getMetrics() const {
    return currentMetrics_;
}

GPUPerformanceMonitor::PerformanceMetrics GPUPerformanceMonitor::getAverageMetrics(int samples) const {
    if (historicalMetrics_.empty()) {
        return currentMetrics_;
    }
    
    int actualSamples = std::min(samples, static_cast<int>(historicalMetrics_.size()));
    auto startIt = historicalMetrics_.end() - actualSamples;
    
    PerformanceMetrics avg;
    
    for (auto it = startIt; it != historicalMetrics_.end(); ++it) {
        avg.gpuComputeTime += it->gpuComputeTime;
        avg.gpuMemoryTransferTime += it->gpuMemoryTransferTime;
        avg.totalGPUTime += it->totalGPUTime;
        avg.cpuComputeTime += it->cpuComputeTime;
        avg.speedupRatio += it->speedupRatio;
        avg.gpuMemoryUsed += it->gpuMemoryUsed;
        avg.memoryTransferOverhead += it->memoryTransferOverhead;
        avg.gpuUtilization += it->gpuUtilization;
        avg.efficiency += it->efficiency;
    }
    
    double factor = 1.0 / actualSamples;
    avg.gpuComputeTime *= factor;
    avg.gpuMemoryTransferTime *= factor;
    avg.totalGPUTime *= factor;
    avg.cpuComputeTime *= factor;
    avg.speedupRatio *= factor;
    avg.gpuMemoryUsed = static_cast<size_t>(avg.gpuMemoryUsed * factor);
    avg.memoryTransferOverhead *= factor;
    avg.gpuUtilization *= factor;
    avg.efficiency *= factor;
    
    return avg;
}

void GPUPerformanceMonitor::updateRealTimeMetrics() {
    calculateDerivedMetrics();
    
    if (detailedLogging_) {
        const auto& metrics = currentMetrics_;
        std::cout << "Real-time GPU metrics - "
                  << "Compute: " << metrics.gpuComputeTime << "ms, "
                  << "Transfer: " << metrics.gpuMemoryTransferTime << "ms, "
                  << "Speedup: " << metrics.speedupRatio << "x, "
                  << "Efficiency: " << metrics.efficiency << std::endl;
    }
}

bool GPUPerformanceMonitor::isPerformanceRegression() const {
    if (historicalMetrics_.size() < 2) {
        return false; // Need at least 2 samples to detect regression
    }
    
    // Compare current performance with recent average
    auto recentAvg = getAverageMetrics(5);
    
    // Check for regression in key metrics
    bool speedupRegression = (currentMetrics_.speedupRatio > 0 && recentAvg.speedupRatio > 0) &&
                            (currentMetrics_.speedupRatio < recentAvg.speedupRatio * (1.0 - regressionThreshold_));
    
    bool efficiencyRegression = (currentMetrics_.efficiency > 0 && recentAvg.efficiency > 0) &&
                               (currentMetrics_.efficiency < recentAvg.efficiency * (1.0 - regressionThreshold_));
    
    bool overheadRegression = currentMetrics_.memoryTransferOverhead > recentAvg.memoryTransferOverhead * (1.0 + regressionThreshold_);
    
    return speedupRegression || efficiencyRegression || overheadRegression;
}

void GPUPerformanceMonitor::logPerformanceData(const std::string& scenario) {
    const auto& metrics = currentMetrics_;
    
    std::cout << "=== GPU Performance Report - " << scenario << " ===" << std::endl;
    std::cout << "GPU Compute Time:    " << metrics.gpuComputeTime << " ms" << std::endl;
    std::cout << "Memory Transfer:     " << metrics.gpuMemoryTransferTime << " ms" << std::endl;
    std::cout << "Total GPU Time:      " << metrics.totalGPUTime << " ms" << std::endl;
    std::cout << "CPU Equivalent:      " << metrics.cpuComputeTime << " ms" << std::endl;
    std::cout << "Speedup Ratio:       " << metrics.speedupRatio << "x" << std::endl;
    std::cout << "Memory Used:         " << (metrics.gpuMemoryUsed / 1024 / 1024) << " MB" << std::endl;
    std::cout << "Transfer Overhead:   " << metrics.memoryTransferOverhead << "%" << std::endl;
    std::cout << "GPU Utilization:     " << metrics.gpuUtilization << "%" << std::endl;
    std::cout << "Overall Efficiency:  " << metrics.efficiency << std::endl;
    
    if (isPerformanceRegression()) {
        std::cout << "WARNING: Performance regression detected!" << std::endl;
    }
    
    std::cout << "================================================" << std::endl;
}

void GPUPerformanceMonitor::reset() {
    currentMetrics_ = PerformanceMetrics();
#ifdef USE_GPU
    queryActive_ = false;
#endif
}

void GPUPerformanceMonitor::clearHistory() {
    historicalMetrics_.clear();
}