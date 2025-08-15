#include "path_tracer.h"
#include "core/scene_manager.h"
#include "core/camera.h"
#include "render/gpu_compute.h"
#include "render/gpu_memory.h"
#include "render/gpu_rng.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <cmath>
#include <fstream>
#include <sstream>

#ifdef USE_GPU
#include <GL/gl.h>
#ifndef GL_COMPUTE_SHADER
#define GL_COMPUTE_SHADER 0x91B9
#endif
#ifndef GL_TEXTURE_2D
#define GL_TEXTURE_2D 0x0DE1
#endif
#ifndef GL_RGBA32F
#define GL_RGBA32F 0x8814
#endif
#endif

// PathTracer implementation
PathTracer::PathTracer() 
    : camera_(Vector3(0, 0, 0), Vector3(0, 0, -1), Vector3(0, 1, 0)),
      max_depth_(10), samples_per_pixel_(10), stop_requested_(false),
      rng_(std::random_device{}()), uniform_dist_(0.0f, 1.0f)
#ifdef USE_GPU
      , currentMode_(RenderMode::HYBRID_AUTO), rayTracingProgram_(0), outputTexture_(0)
#endif
{
}

PathTracer::~PathTracer() {
#ifdef USE_GPU
    cleanupGPU();
#endif
}

void PathTracer::set_scene_manager(std::shared_ptr<SceneManager> scene_manager) {
    scene_manager_ = scene_manager;
}

void PathTracer::set_camera(const Camera& camera) {
    camera_ = camera;
}

void PathTracer::trace(int width, int height) {
    trace_interruptible(width, height);
}

bool PathTracer::trace_interruptible(int width, int height) {
    image_data_.clear();
    image_data_.resize(width * height);
    
    reset_stop_request();
    
    std::cout << "Starting path tracing (" << width << "x" << height << ", " 
              << samples_per_pixel_ << " samples per pixel)" << std::endl;
    
    auto start_time = std::chrono::steady_clock::now();
    
    for (int j = height - 1; j >= 0; --j) {
        // Check for cancellation every few scanlines
        if (j % 5 == 0) {
            if (stop_requested_) {
                std::cout << "Path tracing cancelled at scanline " << j << std::endl;
                return false;
            }
            
            if (j % 10 == 0) {
                std::cout << "Scanlines remaining: " << j << std::endl;
            }
        }
        
        for (int i = 0; i < width; ++i) {
            Color pixel_color(0, 0, 0);
            
            for (int s = 0; s < samples_per_pixel_; ++s) {
                // Check for cancellation during expensive inner loop
                if (stop_requested_) {
                    std::cout << "Path tracing cancelled during sampling" << std::endl;
                    return false;
                }
                
                float u = (float(i) + uniform_dist_(rng_)) / float(width);
                float v = (float(j) + uniform_dist_(rng_)) / float(height);
                
                Ray ray = camera_.get_ray(u, v);
                pixel_color += ray_color(ray, max_depth_);
            }
            
            // Average the samples and gamma correct
            float scale = 1.0f / float(samples_per_pixel_);
            pixel_color *= scale;
            
            // Gamma correction (gamma=2.0)
            pixel_color.r = std::sqrt(pixel_color.r);
            pixel_color.g = std::sqrt(pixel_color.g);
            pixel_color.b = std::sqrt(pixel_color.b);
            
            image_data_[(height - 1 - j) * width + i] = pixel_color.clamped();
        }
    }
    
    if (stop_requested_) {
        std::cout << "Path tracing cancelled near completion" << std::endl;
        return false;
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    std::cout << "Path tracing completed in " << duration.count() << " ms" << std::endl;
    return true;
}

bool PathTracer::trace_progressive(int width, int height, const ProgressiveConfig& config, ProgressiveCallback callback) {
    image_data_.clear();
    image_data_.resize(width * height);
    
    // Initialize accumulated samples buffer
    std::vector<Color> accumulated_samples(width * height, Color(0, 0, 0));
    
    reset_stop_request();
    
    std::cout << "Starting progressive path tracing (" << width << "x" << height << ")" << std::endl;
    std::cout << "Progressive config: " << config.initialSamples << " -> " << config.targetSamples 
              << " samples, " << config.progressiveSteps << " steps, " 
              << config.updateInterval << "s intervals" << std::endl;
    
    auto start_time = std::chrono::steady_clock::now();
    auto last_update_time = start_time;
    
    // Calculate sample counts for each progressive step
    std::vector<int> step_samples;
    step_samples.push_back(config.initialSamples);
    
    for (int step = 1; step < config.progressiveSteps; ++step) {
        float progress = float(step) / float(config.progressiveSteps - 1);
        float log_initial = std::log(float(config.initialSamples));
        float log_target = std::log(float(config.targetSamples));
        int samples = int(std::exp(log_initial + progress * (log_target - log_initial)));
        step_samples.push_back(samples);
    }
    
    // Debug: Print sample progression
    std::cout << "Progressive sample plan: ";
    for (size_t i = 0; i < step_samples.size(); ++i) {
        std::cout << step_samples[i];
        if (i < step_samples.size() - 1) std::cout << " -> ";
    }
    std::cout << std::endl;
    
    int total_samples_rendered = 0;
    
    for (int step = 0; step < config.progressiveSteps; ++step) {
        if (stop_requested_) {
            std::cout << "Progressive rendering cancelled at step " << step << " due to stop request" << std::endl;
            return false;
        }
        
        int samples_this_step = step_samples[step];
        if (step > 0) {
            samples_this_step -= step_samples[step - 1];
        }
        
        std::cout << "Progressive step " << (step + 1) << "/" << config.progressiveSteps 
                  << ": rendering " << samples_this_step << " additional samples" << std::endl;
        
        // Render additional samples for this step
        for (int j = height - 1; j >= 0; --j) {
            if (stop_requested_) {
                std::cout << "Progressive rendering cancelled during step " << step << std::endl;
                return false;
            }
            
            for (int i = 0; i < width; ++i) {
                Color pixel_samples(0, 0, 0);
                
                for (int s = 0; s < samples_this_step; ++s) {
                    if (stop_requested_) {
                        std::cout << "Progressive rendering cancelled during sampling" << std::endl;
                        return false;
                    }
                    
                    float u = (float(i) + uniform_dist_(rng_)) / float(width);
                    float v = (float(j) + uniform_dist_(rng_)) / float(height);
                    
                    Ray ray = camera_.get_ray(u, v);
                    pixel_samples += ray_color(ray, max_depth_);
                }
                
                // Accumulate samples
                int pixel_index = (height - 1 - j) * width + i;
                accumulated_samples[pixel_index] += pixel_samples;
            }
        }
        
        total_samples_rendered = step_samples[step];
        
        // Generate intermediate result by averaging accumulated samples
        for (int i = 0; i < width * height; ++i) {
            Color averaged = accumulated_samples[i] * (1.0f / float(total_samples_rendered));
            
            // Gamma correction (gamma=2.0)
            averaged.r = std::sqrt(averaged.r);
            averaged.g = std::sqrt(averaged.g);
            averaged.b = std::sqrt(averaged.b);
            
            image_data_[i] = averaged.clamped();
        }
        
        // Call callback with intermediate result
        if (callback) {
            std::cout << "Calling callback with " << total_samples_rendered << "/" << config.targetSamples << " samples" << std::endl;
            callback(image_data_, width, height, total_samples_rendered, config.targetSamples);
        }
        
        // Wait for update interval (except on last step) with adaptive timing
        if (step < config.progressiveSteps - 1) {
            auto current_time = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration<float>(current_time - last_update_time).count();
            
            // Adaptive update interval: reduce frequency for very small images or early steps
            float adaptive_interval = config.updateInterval;
            if (width * height < 100000) { // For images smaller than 100K pixels
                adaptive_interval *= 0.5f; // Update more frequently
            }
            if (step < config.progressiveSteps / 4) { // For early steps
                adaptive_interval *= 0.75f; // Update more frequently when changes are most visible
            }
            
            if (elapsed < adaptive_interval) {
                float sleep_time = adaptive_interval - elapsed;
                std::this_thread::sleep_for(std::chrono::duration<float>(sleep_time));
            }
            
            last_update_time = std::chrono::steady_clock::now();
        }
    }
    
    if (stop_requested_) {
        std::cout << "Progressive rendering cancelled near completion" << std::endl;
        return false;
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    std::cout << "Progressive path tracing completed in " << duration.count() << " ms" << std::endl;
    return true;
}


Color PathTracer::ray_color(const Ray& ray, int depth) const {
    if (depth <= 0) {
        return Color::black();
    }
    
    if (!scene_manager_) {
        // No scene manager, return background
        Vector3 unit_direction = ray.direction.normalized();
        float t = 0.5f * (unit_direction.y + 1.0f);
        return Color(1.0f, 1.0f, 1.0f) * (1.0f - t) + Color(0.5f, 0.7f, 1.0f) * t;
    }
    
    HitRecord rec;
    if (scene_manager_->hit_scene(ray, 0.001f, std::numeric_limits<float>::infinity(), rec)) {
        // Handle emissive materials
        if (rec.material.is_emissive()) {
            return rec.material.albedo * rec.material.emission;
        }
        
        // Diffuse reflection with some randomness
        Vector3 target;
        if (rec.material.roughness > 0.5f) {
            // More diffuse
            target = rec.point + rec.normal + random_unit_vector();
        } else {
            // More reflective
            Vector3 reflected = reflect(ray.direction, rec.normal);
            target = rec.point + reflected + random_in_unit_sphere() * rec.material.roughness;
        }
        
        Ray scattered(rec.point, target - rec.point);
        return rec.material.albedo * ray_color(scattered, depth - 1);
    }
    
    // Use scene manager's background color
    return scene_manager_->get_background_color(ray);
}

Vector3 PathTracer::random_in_unit_sphere() const {
    Vector3 p;
    do {
        p = Vector3(uniform_dist_(rng_), uniform_dist_(rng_), uniform_dist_(rng_)) * 2.0f - Vector3(1, 1, 1);
    } while (p.dot(p) >= 1.0f);
    return p;
}

Vector3 PathTracer::random_unit_vector() const {
    return random_in_unit_sphere().normalized();
}

Vector3 PathTracer::random_in_hemisphere(const Vector3& normal) const {
    Vector3 in_unit_sphere = random_in_unit_sphere();
    if (in_unit_sphere.dot(normal) > 0.0f) {
        return in_unit_sphere;
    } else {
        return in_unit_sphere * -1.0f;
    }
}

Vector3 PathTracer::reflect(const Vector3& v, const Vector3& n) const {
    return v - n * (2.0f * v.dot(n));
}

bool PathTracer::near_zero(const Vector3& v) const {
    const float s = 1e-8f;
    return (std::abs(v.x) < s) && (std::abs(v.y) < s) && (std::abs(v.z) < s);
}

#ifdef USE_GPU
// GPU Implementation
bool PathTracer::initializeGPU() {
    if (gpuPipeline_ && gpuPipeline_->isAvailable()) {
        return true; // Already initialized
    }
    
    std::cout << "Initializing GPU compute pipeline for path tracing..." << std::endl;
    
    // Initialize GPU compute pipeline
    gpuPipeline_ = std::make_shared<GPUComputePipeline>();
    if (!gpuPipeline_->initialize()) {
        std::cerr << "Failed to initialize GPU compute pipeline: " << gpuPipeline_->getErrorMessage() << std::endl;
        return false;
    }
    
    // Initialize GPU memory manager
    gpuMemory_ = std::make_shared<GPUMemoryManager>();
    if (!gpuMemory_->initialize()) {
        std::cerr << "Failed to initialize GPU memory manager: " << gpuMemory_->getErrorMessage() << std::endl;
        return false;
    }
    
    // Initialize GPU RNG
    gpuRNG_ = std::make_unique<GPURandomGenerator>();
    
    // Compile ray tracing shader
    if (!compileRayTracingShader()) {
        std::cerr << "Failed to compile ray tracing shader" << std::endl;
        return false;
    }
    
    std::cout << "GPU path tracing initialized successfully" << std::endl;
    std::cout << "GPU Info: " << gpuPipeline_->getDriverInfo() << std::endl;
    return true;
#else
    std::cerr << "GPU support not compiled in (USE_GPU not defined)" << std::endl;
    return false;
#endif
}

bool PathTracer::isGPUAvailable() const {
    return gpuPipeline_ && gpuPipeline_->isAvailable();
}

void PathTracer::cleanupGPU() {
#ifdef USE_GPU
    if (outputTexture_ != 0) {
        glDeleteTextures(1, &outputTexture_);
        outputTexture_ = 0;
    }
    
    if (rayTracingProgram_ != 0) {
        glDeleteProgram(rayTracingProgram_);
        rayTracingProgram_ = 0;
    }
    
    if (gpuRNG_) {
        gpuRNG_->cleanup();
        gpuRNG_.reset();
    }
    
    if (gpuMemory_) {
        gpuMemory_->cleanup();
        gpuMemory_.reset();
    }
    
    if (gpuPipeline_) {
        gpuPipeline_->cleanup();
        gpuPipeline_.reset();
    }
#endif
}

bool PathTracer::compileRayTracingShader() {
#ifdef USE_GPU
    if (!gpuPipeline_) {
        return false;
    }
    
    // Read shader source from file
    std::ifstream shaderFile("/home/chad/git/new_renderer/src/render/shaders/ray_tracing.comp");
    if (!shaderFile.is_open()) {
        std::cerr << "Failed to open ray tracing shader file" << std::endl;
        return false;
    }
    
    std::stringstream shaderStream;
    shaderStream << shaderFile.rdbuf();
    std::string shaderSource = shaderStream.str();
    shaderFile.close();
    
    // Compile shader
    if (!gpuPipeline_->compileShader(shaderSource)) {
        std::cerr << "Failed to compile ray tracing shader: " << gpuPipeline_->getErrorMessage() << std::endl;
        return false;
    }
    
    // Link program
    if (!gpuPipeline_->linkProgram()) {
        std::cerr << "Failed to link ray tracing program: " << gpuPipeline_->getErrorMessage() << std::endl;
        return false;
    }
    
    // Store the program handle for later use
#ifdef USE_GPU
    // We need to extract the program from the pipeline - this is a design issue
    // For now, we'll assume the pipeline exposes the program ID
    // rayTracingProgram_ = gpuPipeline_->getProgramID(); // This method doesn't exist yet
    rayTracingProgram_ = 0; // Placeholder - we'll fix this with proper GPU pipeline integration
#endif
    
    std::cout << "Ray tracing shader compiled and linked successfully" << std::endl;
    return true;
#else
    return false;
#endif
}

bool PathTracer::trace_gpu(int width, int height) {
#ifdef USE_GPU
    if (!isGPUAvailable()) {
        std::cerr << "GPU not available for ray tracing" << std::endl;
        return false;
    }
    
    std::cout << "Starting GPU path tracing (" << width << "x" << height << ", " 
              << samples_per_pixel_ << " samples per pixel)" << std::endl;
    
    auto start_time = std::chrono::steady_clock::now();
    
    // Initialize GPU RNG for this image size
    if (!gpuRNG_->initialize(width, height)) {
        std::cerr << "Failed to initialize GPU RNG" << std::endl;
        return false;
    }
    
    // Prepare scene data for GPU
    if (!prepareGPUScene()) {
        std::cerr << "Failed to prepare GPU scene data" << std::endl;
        return false;
    }
    
    // Dispatch GPU compute
    if (!dispatchGPUCompute(width, height, samples_per_pixel_)) {
        std::cerr << "Failed to dispatch GPU compute" << std::endl;
        return false;
    }
    
    // Read back results
    if (!readbackGPUResult(width, height)) {
        std::cerr << "Failed to read back GPU results" << std::endl;
        return false;
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    std::cout << "GPU path tracing completed in " << duration.count() << " ms" << std::endl;
    
    return true;
#else
    std::cerr << "GPU support not compiled in" << std::endl;
    return false;
#endif
}

bool PathTracer::trace_hybrid(int width, int height, RenderMode mode) {
    if (mode == RenderMode::CPU_ONLY) {
        return trace_interruptible(width, height);
    } else if (mode == RenderMode::GPU_ONLY) {
        return trace_gpu(width, height);
    } else {
        // HYBRID_AUTO - decide based on performance heuristics
        if (shouldUseGPU(width, height, samples_per_pixel_)) {
            std::cout << "Hybrid mode: Using GPU for rendering" << std::endl;
            return trace_gpu(width, height);
        } else {
            std::cout << "Hybrid mode: Using CPU for rendering" << std::endl;
            return trace_interruptible(width, height);
        }
    }
}

bool PathTracer::shouldUseGPU(int width, int height, int samples) const {
    if (!isGPUAvailable()) {
        return false;
    }
    
    // Simple heuristic: use GPU for larger images or higher sample counts
    int totalWork = width * height * samples;
    const int GPU_THRESHOLD = 100000; // Threshold where GPU becomes worthwhile
    
    return totalWork > GPU_THRESHOLD;
}

bool PathTracer::prepareGPUScene() {
#ifdef USE_GPU
    if (!scene_manager_ || !gpuMemory_) {
        return false;
    }
    
    // Get scene primitives - this is a simplified implementation
    // In a real implementation, we'd need to extract sphere data from SceneManager
    std::vector<float> sceneData;
    
    // For now, create some test spheres
    // Format: [center.x, center.y, center.z, radius, albedo.r, albedo.g, albedo.b, material_flags]
    sceneData = {
        // Sphere 1: center=(0,0,-1), radius=0.5, red albedo
        0.0f, 0.0f, -1.0f, 0.5f,
        0.8f, 0.3f, 0.3f, 0.0f,
        
        // Sphere 2: center=(0,-100.5,-1), radius=100, green albedo (ground)
        0.0f, -100.5f, -1.0f, 100.0f,
        0.3f, 0.8f, 0.3f, 0.0f,
        
        // Sphere 3: center=(1,0,-1), radius=0.5, blue albedo
        1.0f, 0.0f, -1.0f, 0.5f,
        0.3f, 0.3f, 0.8f, 0.0f
    };
    
    // Allocate scene buffer
    size_t sceneBufferSize = sceneData.size() * sizeof(float);
    sceneBuffer_ = gpuMemory_->allocateBuffer(
        sceneBufferSize,
        GPUBufferType::SHADER_STORAGE,
        GPUUsagePattern::STATIC,
        "scene_data"
    );
    
    if (!sceneBuffer_) {
        std::cerr << "Failed to allocate scene buffer" << std::endl;
        return false;
    }
    
    // Transfer scene data to GPU
    if (!gpuMemory_->transferToGPU(sceneBuffer_, sceneData.data(), sceneBufferSize)) {
        std::cerr << "Failed to transfer scene data to GPU" << std::endl;
        return false;
    }
    
    std::cout << "Scene data prepared for GPU (" << sceneData.size() / 8 << " spheres)" << std::endl;
    return true;
#else
    return false;
#endif
}

bool PathTracer::dispatchGPUCompute(int width, int height, int samples) {
#ifdef USE_GPU
    if (!gpuPipeline_ || !sceneBuffer_ || !gpuRNG_) {
        return false;
    }
    
    // Create output texture
    if (outputTexture_ != 0) {
        glDeleteTextures(1, &outputTexture_);
    }
    
    glGenTextures(1, &outputTexture_);
    glBindTexture(GL_TEXTURE_2D, outputTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    // Bind compute shader program
    glUseProgram(rayTracingProgram_);
    
    // Bind output texture
    glBindImageTexture(0, outputTexture_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
    
    // Bind scene data buffer
    gpuMemory_->bindBuffer(sceneBuffer_, 1);
    
    // Bind RNG buffer
    gpuMemory_->bindBuffer(gpuRNG_->getRNGBuffer(), 2);
    
    // Set uniforms
    updateGPUUniforms(width, height, samples);
    
    // Calculate work group sizes
    const int LOCAL_SIZE = 16;
    int workGroupsX = (width + LOCAL_SIZE - 1) / LOCAL_SIZE;
    int workGroupsY = (height + LOCAL_SIZE - 1) / LOCAL_SIZE;
    
    // Dispatch compute shader
    if (!gpuPipeline_->dispatch(workGroupsX, workGroupsY, 1)) {
        std::cerr << "Failed to dispatch GPU compute: " << gpuPipeline_->getErrorMessage() << std::endl;
        return false;
    }
    
    // Wait for completion
    gpuPipeline_->synchronize();
    
    std::cout << "GPU compute dispatched: " << workGroupsX << "x" << workGroupsY << " work groups" << std::endl;
    return true;
#else
    return false;
#endif
}

void PathTracer::updateGPUUniforms(int width, int height, int samples) {
#ifdef USE_GPU
    // Set uniform values
    glUniform1i(glGetUniformLocation(rayTracingProgram_, "imageWidth"), width);
    glUniform1i(glGetUniformLocation(rayTracingProgram_, "imageHeight"), height);
    glUniform1i(glGetUniformLocation(rayTracingProgram_, "samplesPerPixel"), samples);
    glUniform1i(glGetUniformLocation(rayTracingProgram_, "maxDepth"), max_depth_);
    glUniform1i(glGetUniformLocation(rayTracingProgram_, "sphereCount"), 3); // Hardcoded for now
    
    // Camera uniforms
    Vector3 pos = camera_.get_position();
    glUniform3f(glGetUniformLocation(rayTracingProgram_, "cameraPosition"), pos.x, pos.y, pos.z);
    
    // For now, use identity transform - in real implementation we'd calculate proper camera transform
    float transform[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };
    glUniformMatrix4fv(glGetUniformLocation(rayTracingProgram_, "cameraTransform"), 1, GL_FALSE, transform);
#endif
}

bool PathTracer::readbackGPUResult(int width, int height) {
#ifdef USE_GPU
    if (outputTexture_ == 0) {
        return false;
    }
    
    // Resize image data buffer
    image_data_.resize(width * height);
    std::vector<float> tempData(width * height * 4); // RGBA
    
    // Read texture data
    glBindTexture(GL_TEXTURE_2D, outputTexture_);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, tempData.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    
    // Convert RGBA float to Color
    for (int i = 0; i < width * height; ++i) {
        float r = tempData[i * 4 + 0];
        float g = tempData[i * 4 + 1];
        float b = tempData[i * 4 + 2];
        image_data_[i] = Color(r, g, b);
    }
    
    std::cout << "GPU result read back successfully" << std::endl;
    return true;
#else
    return false;
#endif
}

PerformanceMetrics PathTracer::benchmarkGPUvsCPU(int width, int height) {
    PerformanceMetrics metrics;
    metrics.imageWidth = width;
    metrics.imageHeight = height;
    metrics.samplesPerPixel = samples_per_pixel_;
    
    // Benchmark CPU
    std::cout << "Benchmarking CPU performance..." << std::endl;
    auto cpuStart = std::chrono::high_resolution_clock::now();
    bool cpuSuccess = trace_interruptible(width, height);
    auto cpuEnd = std::chrono::high_resolution_clock::now();
    
    if (cpuSuccess) {
        metrics.cpuTime = std::chrono::duration<double, std::milli>(cpuEnd - cpuStart).count();
        std::cout << "CPU time: " << metrics.cpuTime << " ms" << std::endl;
    }
    
    // Benchmark GPU
    if (isGPUAvailable()) {
        std::cout << "Benchmarking GPU performance..." << std::endl;
        auto gpuStart = std::chrono::high_resolution_clock::now();
        bool gpuSuccess = trace_gpu(width, height);
        auto gpuEnd = std::chrono::high_resolution_clock::now();
        
        if (gpuSuccess) {
            metrics.gpuTime = std::chrono::duration<double, std::milli>(gpuEnd - gpuStart).count();
            std::cout << "GPU time: " << metrics.gpuTime << " ms" << std::endl;
            
            if (metrics.cpuTime > 0 && metrics.gpuTime > 0) {
                metrics.speedupFactor = metrics.cpuTime / metrics.gpuTime;
                std::cout << "GPU speedup: " << metrics.speedupFactor << "x" << std::endl;
            }
        }
    }
    
    return metrics;
}

bool PathTracer::validateGPUAccuracy(const std::vector<Color>& cpuResult, const std::vector<Color>& gpuResult, float tolerance) {
    if (cpuResult.size() != gpuResult.size()) {
        std::cerr << "GPU accuracy validation failed: result sizes don't match" << std::endl;
        return false;
    }
    
    double totalError = 0.0;
    int pixelCount = cpuResult.size();
    int errorPixels = 0;
    
    for (int i = 0; i < pixelCount; ++i) {
        float errorR = std::abs(cpuResult[i].r - gpuResult[i].r);
        float errorG = std::abs(cpuResult[i].g - gpuResult[i].g);
        float errorB = std::abs(cpuResult[i].b - gpuResult[i].b);
        
        float pixelError = std::max({errorR, errorG, errorB});
        totalError += pixelError;
        
        if (pixelError > tolerance) {
            errorPixels++;
        }
    }
    
    double averageError = totalError / pixelCount;
    double errorRate = static_cast<double>(errorPixels) / pixelCount;
    
    std::cout << "GPU accuracy validation:" << std::endl;
    std::cout << "  Average error: " << averageError << std::endl;
    std::cout << "  Error pixels: " << errorPixels << "/" << pixelCount << " (" << (errorRate * 100) << "%)" << std::endl;
    
    // Pass if average error is within tolerance and error rate is low
    bool passed = (averageError <= tolerance) && (errorRate <= 0.05); // Allow 5% error pixels
    
    if (passed) {
        std::cout << "  Result: PASSED" << std::endl;
    } else {
        std::cout << "  Result: FAILED" << std::endl;
    }
    
    return passed;
}
#endif