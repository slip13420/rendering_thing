#pragma once

#include "core/common.h"
#include "core/camera.h"
#include <random>
#include <memory>
#include <functional>
#include <atomic>
#include <chrono>

// Forward declarations
class SceneManager;
class GPUComputePipeline;
class GPUMemoryManager;
class GPURandomGenerator;
struct GPUBuffer;

#ifdef USE_GPU
struct SDL_Window;
typedef void* SDL_GLContext;
#endif

// Progressive rendering configuration
struct ProgressiveConfig {
    int initialSamples = 1;      // Quick preview sample count
    int targetSamples = 2000;    // High quality sample count for GPU acceleration
    int progressiveSteps = 12;   // Number of progressive improvement passes
    float updateInterval = 0.3f; // Seconds between progressive updates
};

// Progressive rendering callback for intermediate results
using ProgressiveCallback = std::function<void(const std::vector<Color>&, int, int, int, int)>;

// Performance metrics for GPU vs CPU comparison
struct PerformanceMetrics {
    double gpuTime = 0.0;
    double cpuTime = 0.0;
    double speedupFactor = 0.0;
    int samplesPerPixel = 0;
    int imageWidth = 0;
    int imageHeight = 0;
};

class PathTracer {
public:
    enum class RenderMode {
        CPU_ONLY,
        GPU_ONLY,
        HYBRID_AUTO
    };

    PathTracer();
    ~PathTracer();
    
    // CPU rendering methods
    void trace(int width, int height);
    bool trace_interruptible(int width, int height);
    
    // Progressive rendering
    bool trace_progressive(int width, int height, const ProgressiveConfig& config, ProgressiveCallback callback);
    
#ifdef USE_GPU
    // GPU rendering methods
    bool trace_gpu(int width, int height);
    bool trace_hybrid(int width, int height, RenderMode mode = RenderMode::HYBRID_AUTO);
    bool trace_progressive_gpu(int width, int height, const ProgressiveConfig& config, ProgressiveCallback callback);
    
    // Non-blocking GPU operations
    bool start_gpu_async(int width, int height);  // Start GPU work without waiting
    bool is_gpu_complete();                       // Check if GPU work is done
    bool finalize_gpu_result(int width, int height); // Get result when ready
#endif
    
    void request_stop() { stop_requested_ = true; }
    void reset_stop_request() { stop_requested_ = false; }
    bool is_stop_requested() const { return stop_requested_; }
    
    // Configuration
    void set_scene_manager(std::shared_ptr<SceneManager> scene_manager);
    void set_camera(const Camera& camera);
    void set_max_depth(int depth) { max_depth_ = depth; }
    void set_samples_per_pixel(int samples) { samples_per_pixel_ = samples; }
    
#ifdef USE_GPU
    // GPU configuration
    bool initializeGPU();
    bool isGPUAvailable() const;
    void cleanupGPU();
    
    // Performance analysis
    PerformanceMetrics benchmarkGPUvsCPU(int width, int height);
    bool shouldUseGPU(int width, int height, int samples) const;
    void setRenderMode(RenderMode mode) { currentMode_ = mode; }
    RenderMode getRenderMode() const { return currentMode_; }
    
    // Accuracy validation
    bool validateGPUAccuracy(const std::vector<Color>& cpuResult, const std::vector<Color>& gpuResult, float tolerance = 0.01f);
    
    // Context management
    void captureOpenGLContext(); // Store current window/context for GPU operations
    bool activateGPUContext(); // Switch to the context where GPU resources were created
#endif
    
    // Get rendered data
    const std::vector<Color>& get_image_data() const { return image_data_; }
    
private:
    // CPU ray tracing methods
    Color ray_color(const Ray& ray, int depth) const;
    Vector3 random_in_unit_sphere() const;
    Vector3 random_unit_vector() const;
    Vector3 random_in_hemisphere(const Vector3& normal) const;
    Vector3 reflect(const Vector3& v, const Vector3& n) const;
    bool near_zero(const Vector3& v) const;
    
    // GPU ray tracing methods
    bool compileRayTracingShader();
    bool prepareGPUScene();
    bool dispatchGPUCompute(int width, int height, int samples);
    bool dispatchGPUComputeAsync(int width, int height, int samples);  // Non-blocking version
    bool readbackGPUResult(int width, int height);
    void updateGPUUniforms(int width, int height, int samples);
    
    // Scene data
    std::shared_ptr<SceneManager> scene_manager_;
    Camera camera_;
    std::vector<Color> image_data_;
    
    // CPU rendering state
    int max_depth_;
    int samples_per_pixel_;
    std::atomic<bool> stop_requested_;
    mutable std::mt19937 rng_;
    mutable std::uniform_real_distribution<float> uniform_dist_;
    
#ifdef USE_GPU
    // GPU rendering state
    std::shared_ptr<GPUComputePipeline> gpuPipeline_;
    std::shared_ptr<GPUMemoryManager> gpuMemory_;
    std::unique_ptr<GPURandomGenerator> gpuRNG_;
    RenderMode currentMode_;
    
    unsigned int rayTracingProgram_;
    unsigned int outputTexture_;
    std::shared_ptr<GPUBuffer> sceneBuffer_;
    
    // OpenGL context management for GPU operations
    SDL_Window* gl_window_;
    SDL_GLContext gl_context_;
    std::shared_ptr<GPUBuffer> rngBuffer_;
    
    // Asynchronous GPU state
    struct AsyncGPUState {
        bool active = false;
        int width = 0;
        int height = 0;
        std::chrono::steady_clock::time_point start_time;
    } async_gpu_state_;
#endif
};