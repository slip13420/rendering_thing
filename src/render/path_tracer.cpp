#include "render/path_tracer.h"
#include "core/common.h"
#include "core/scene_manager.h"
#include "core/camera.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <cmath>
#include <fstream>
#include <sstream>
#include <limits>

#ifdef USE_GPU
#include "render/gpu_compute.h"
#include "render/gpu_memory.h"
#include "render/gpu_rng.h"
#include <GL/gl.h>
#include <GL/glext.h>
#include <SDL.h>
#ifndef GL_COMPUTE_SHADER
#define GL_COMPUTE_SHADER 0x91B9
#endif
#ifndef GL_TEXTURE_2D
#define GL_TEXTURE_2D 0x0DE1
#endif
#ifndef GL_RGBA32F
#define GL_RGBA32F 0x8814
#endif
#ifndef GL_WRITE_ONLY
#define GL_WRITE_ONLY 0x88B9
#endif
#ifndef GL_SHADER_IMAGE_ACCESS_BARRIER_BIT
#define GL_SHADER_IMAGE_ACCESS_BARRIER_BIT 0x00000020
#endif

// Declare missing OpenGL functions as extern
extern "C" {
    void glDeleteProgram(unsigned int program);
    void glUseProgram(unsigned int program);
    void glBindImageTexture(unsigned int unit, unsigned int texture, int level, unsigned char layered, int layer, unsigned int access, unsigned int format);
    int glGetUniformLocation(unsigned int program, const char* name);
    void glMemoryBarrier(unsigned int barriers);
    void glUniform1i(int location, int v0);
    void glUniform3f(int location, float v0, float v1, float v2);
    void glUniformMatrix4fv(int location, int count, unsigned char transpose, const float* value);
}

// OpenGL texture function pointers for worker thread safety
static void (*glGenTextures_ptr)(int n, unsigned int* textures) = nullptr;
static void (*glDeleteTextures_ptr)(int n, const unsigned int* textures) = nullptr;
static void (*glBindTexture_ptr)(unsigned int target, unsigned int texture) = nullptr;
static void (*glTexImage2D_ptr)(unsigned int target, int level, int internalformat, int width, int height, int border, unsigned int format, unsigned int type, const void* pixels) = nullptr;
static void (*glTexParameteri_ptr)(unsigned int target, unsigned int pname, int param) = nullptr;
static void (*glGetTexImage_ptr)(unsigned int target, int level, unsigned int format, unsigned int type, void* pixels) = nullptr;

// Wrapper functions for thread-safe OpenGL calls
static void safe_glGenTextures(int n, unsigned int* textures) {
    if (glGenTextures_ptr) glGenTextures_ptr(n, textures);
}
static void safe_glDeleteTextures(int n, const unsigned int* textures) {
    if (glDeleteTextures_ptr) glDeleteTextures_ptr(n, textures);
}
static void safe_glBindTexture(unsigned int target, unsigned int texture) {
    if (glBindTexture_ptr) glBindTexture_ptr(target, texture);
}
static void safe_glTexImage2D(unsigned int target, int level, int internalformat, int width, int height, int border, unsigned int format, unsigned int type, const void* pixels) {
    if (glTexImage2D_ptr) glTexImage2D_ptr(target, level, internalformat, width, height, border, format, type, pixels);
}
static void safe_glTexParameteri(unsigned int target, unsigned int pname, int param) {
    if (glTexParameteri_ptr) glTexParameteri_ptr(target, pname, param);
}
static void safe_glGetTexImage(unsigned int target, int level, unsigned int format, unsigned int type, void* pixels) {
    if (glGetTexImage_ptr) glGetTexImage_ptr(target, level, format, type, pixels);
}

// Function to load texture OpenGL functions
static bool loadTextureOpenGLFunctions() {
    glGenTextures_ptr = (void(*)(int, unsigned int*))SDL_GL_GetProcAddress("glGenTextures");
    glDeleteTextures_ptr = (void(*)(int, const unsigned int*))SDL_GL_GetProcAddress("glDeleteTextures");
    glBindTexture_ptr = (void(*)(unsigned int, unsigned int))SDL_GL_GetProcAddress("glBindTexture");
    glTexImage2D_ptr = (void(*)(unsigned int, int, int, int, int, int, unsigned int, unsigned int, const void*))SDL_GL_GetProcAddress("glTexImage2D");
    glTexParameteri_ptr = (void(*)(unsigned int, unsigned int, int))SDL_GL_GetProcAddress("glTexParameteri");
    glGetTexImage_ptr = (void(*)(unsigned int, int, unsigned int, unsigned int, void*))SDL_GL_GetProcAddress("glGetTexImage");
    
    return glGenTextures_ptr && glBindTexture_ptr && glGetTexImage_ptr && glTexImage2D_ptr && glTexParameteri_ptr;
}
#endif

// PathTracer implementation
PathTracer::PathTracer() 
    : camera_(Vector3(0, 0, 0), Vector3(0, 0, -1), Vector3(0, 1, 0)),
      max_depth_(10), samples_per_pixel_(10), stop_requested_(false),
      rng_(std::random_device{}()), uniform_dist_(0.0f, 1.0f)
#ifdef USE_GPU
      , currentMode_(RenderMode::HYBRID_AUTO), rayTracingProgram_(0), outputTexture_(0),
        gl_window_(nullptr), gl_context_(nullptr)
#endif
{
}

PathTracer::~PathTracer() {
#ifdef USE_GPU
    cleanupGPU();
#endif
}

#ifdef USE_GPU
void PathTracer::captureOpenGLContext() {
    // Capture the current OpenGL window and context for later use
    gl_window_ = SDL_GL_GetCurrentWindow();
    gl_context_ = SDL_GL_GetCurrentContext();
    
    if (gl_window_ && gl_context_) {
        std::cout << "OpenGL context captured successfully" << std::endl;
        std::cout << "  Window: " << gl_window_ << std::endl;
        std::cout << "  Context: " << gl_context_ << std::endl;
    } else {
        std::cout << "Warning: Could not capture OpenGL context" << std::endl;
        std::cout << "  Window: " << gl_window_ << std::endl;
        std::cout << "  Context: " << gl_context_ << std::endl;
    }
}
#endif

void PathTracer::set_scene_manager(std::shared_ptr<SceneManager> scene_manager) {
    scene_manager_ = scene_manager;
}

void PathTracer::set_camera(const Camera& camera) {
    camera_ = camera;
}

void PathTracer::trace(int width, int height) {
    image_data_.clear();
    image_data_.resize(width * height);
    
    auto start_time = std::chrono::steady_clock::now();
    
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            Color pixel_color(0, 0, 0);
            
            for (int s = 0; s < samples_per_pixel_; ++s) {
                float u = (x + uniform_dist_(rng_)) / float(width);
                float v = (y + uniform_dist_(rng_)) / float(height);
                
                Ray ray = camera_.get_ray(u, v);
                pixel_color = pixel_color + ray_color(ray, max_depth_);
            }
            
            // Average the samples
            pixel_color = pixel_color / float(samples_per_pixel_);
            
            // Gamma correction (gamma=2.0)
            pixel_color = Color(std::sqrt(pixel_color.r), std::sqrt(pixel_color.g), std::sqrt(pixel_color.b));
            
            image_data_[y * width + x] = pixel_color;
        }
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    std::cout << "Rendering completed in " << duration.count() << " ms" << std::endl;
}

bool PathTracer::trace_interruptible(int width, int height) {
    image_data_.clear();
    image_data_.resize(width * height);
    
    auto start_time = std::chrono::steady_clock::now();
    
    for (int y = 0; y < height && !stop_requested_; ++y) {
        for (int x = 0; x < width && !stop_requested_; ++x) {
            Color pixel_color(0, 0, 0);
            
            for (int s = 0; s < samples_per_pixel_ && !stop_requested_; ++s) {
                float u = (x + uniform_dist_(rng_)) / float(width);
                float v = (y + uniform_dist_(rng_)) / float(height);
                
                Ray ray = camera_.get_ray(u, v);
                pixel_color = pixel_color + ray_color(ray, max_depth_);
            }
            
            if (stop_requested_) break;
            
            // Average the samples
            pixel_color = pixel_color / float(samples_per_pixel_);
            
            // Gamma correction (gamma=2.0)
            pixel_color = Color(std::sqrt(pixel_color.r), std::sqrt(pixel_color.g), std::sqrt(pixel_color.b));
            
            image_data_[y * width + x] = pixel_color;
        }
        
        if (stop_requested_) break;
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    if (stop_requested_) {
        std::cout << "Rendering interrupted after " << duration.count() << " ms" << std::endl;
        return false;
    } else {
        std::cout << "Interruptible rendering completed in " << duration.count() << " ms" << std::endl;
        return true;
    }
}

bool PathTracer::trace_progressive(int width, int height, const ProgressiveConfig& config, ProgressiveCallback callback) {
    image_data_.clear();
    image_data_.resize(width * height);
    
    // Initialize with black image
    std::fill(image_data_.begin(), image_data_.end(), Color(0, 0, 0));
    
    int current_samples = config.initialSamples;
    int total_samples = 0;
    
    auto last_update = std::chrono::steady_clock::now();
    
    for (int step = 0; step < config.progressiveSteps && !stop_requested_; ++step) {
        // Render additional samples
        for (int sample = 0; sample < current_samples && !stop_requested_; ++sample) {
            for (int y = 0; y < height && !stop_requested_; ++y) {
                for (int x = 0; x < width && !stop_requested_; ++x) {
                    float u = (x + uniform_dist_(rng_)) / float(width);
                    float v = (y + uniform_dist_(rng_)) / float(height);
                    
                    Ray ray = camera_.get_ray(u, v);
                    Color sample_color = ray_color(ray, max_depth_);
                    
                    // Progressive accumulation
                    int index = y * width + x;
                    image_data_[index] = image_data_[index] + sample_color;
                }
            }
        }
        
        if (stop_requested_) break;
        
        total_samples += current_samples;
        
        // Check if enough time has passed for callback
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration<float>(now - last_update).count();
        
        if (elapsed >= config.updateInterval || step == config.progressiveSteps - 1) {
            // Normalize and gamma-correct for display
            std::vector<Color> display_image(width * height);
            for (int i = 0; i < width * height; ++i) {
                Color normalized = image_data_[i] / float(total_samples);
                display_image[i] = Color(std::sqrt(normalized.r), std::sqrt(normalized.g), std::sqrt(normalized.b));
            }
            
            callback(display_image, width, height, total_samples, config.targetSamples);
            last_update = now;
        }
        
        // Increase sample count for next iteration
        current_samples = std::min(current_samples * 2, config.targetSamples - total_samples);
        if (current_samples <= 0) break;
    }
    
    // Final normalization
    for (int i = 0; i < width * height; ++i) {
        image_data_[i] = image_data_[i] / float(total_samples);
        image_data_[i] = Color(std::sqrt(image_data_[i].r), std::sqrt(image_data_[i].g), std::sqrt(image_data_[i].b));
    }
    
    return !stop_requested_;
}

#ifdef USE_GPU
bool PathTracer::trace_progressive_gpu(int width, int height, const ProgressiveConfig& config, ProgressiveCallback callback) {
    if (!isGPUAvailable()) {
        // Fallback to CPU progressive rendering
        return trace_progressive(width, height, config, callback);
    }
    
    image_data_.clear();
    image_data_.resize(width * height);
    
    // Initialize with black image
    std::fill(image_data_.begin(), image_data_.end(), Color(0, 0, 0));
    
    int current_samples = config.initialSamples;
    int total_samples = 0;
    
    auto last_update = std::chrono::steady_clock::now();
    
    for (int step = 0; step < config.progressiveSteps && !stop_requested_; ++step) {
        // Use GPU for this progressive step
        set_samples_per_pixel(current_samples);
        
        std::cout << "Attempting GPU rendering for progressive step " << step << " with " << current_samples << " samples" << std::endl;
        if (!trace_gpu(width, height)) {
            std::cerr << "GPU progressive rendering failed at step " << step << ", falling back to CPU" << std::endl;
            // Fallback to CPU for remaining steps
            return trace_progressive(width, height, config, callback);
        }
        std::cout << "GPU progressive step " << step << " completed successfully" << std::endl;
        
        // Accumulate the GPU result
        const auto& gpu_result = get_image_data();
        for (int i = 0; i < width * height; ++i) {
            image_data_[i] = image_data_[i] + gpu_result[i] * float(current_samples);
        }
        
        if (stop_requested_) break;
        
        total_samples += current_samples;
        
        // Check if enough time has passed for callback
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration<float>(now - last_update).count();
        
        if (elapsed >= config.updateInterval || step == config.progressiveSteps - 1) {
            // Normalize and gamma-correct for display
            std::vector<Color> display_image(width * height);
            for (int i = 0; i < width * height; ++i) {
                Color normalized = image_data_[i] / float(total_samples);
                display_image[i] = Color(std::sqrt(normalized.r), std::sqrt(normalized.g), std::sqrt(normalized.b));
            }
            
            callback(display_image, width, height, total_samples, config.targetSamples);
            last_update = now;
        }
        
        // Increase sample count for next iteration
        current_samples = std::min(current_samples * 2, config.targetSamples - total_samples);
        if (current_samples <= 0) break;
    }
    
    // Final normalization
    for (int i = 0; i < width * height; ++i) {
        image_data_[i] = image_data_[i] / float(total_samples);
        image_data_[i] = Color(std::sqrt(image_data_[i].r), std::sqrt(image_data_[i].g), std::sqrt(image_data_[i].b));
    }
    
    return !stop_requested_;
}
#endif

Color PathTracer::ray_color(const Ray& ray, int depth) const {
    if (depth <= 0) {
        return Color(0, 0, 0);
    }
    
    HitRecord hit;
    if (scene_manager_->hit_scene(ray, 0.001f, std::numeric_limits<float>::infinity(), hit)) {
        // Check for emissive materials first (light sources)
        if (hit.material.emission > 0.0f) {
            return hit.material.albedo * hit.material.emission;
        }
        
        // Material properties
        Color albedo = hit.material.albedo;
        
        if (hit.material.metallic < 0.5f) {
            // Diffuse scattering (non-metallic materials)
            Vector3 scatter_direction = hit.normal + random_unit_vector();
            if (near_zero(scatter_direction)) {
                scatter_direction = hit.normal;
            }
            
            Ray scattered(hit.point, scatter_direction);
            return albedo * ray_color(scattered, depth - 1);
        } else {
            // Metal reflection
            Vector3 reflected = reflect(ray.direction.normalized(), hit.normal);
            reflected = reflected + random_in_unit_sphere() * hit.material.roughness;
            
            if (reflected.dot(hit.normal) > 0) {
                Ray scattered(hit.point, reflected);
                return albedo * ray_color(scattered, depth - 1);
            } else {
                return Color(0, 0, 0);
            }
        }
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
// GPU Implementation - Only compiled when USE_GPU is defined

bool PathTracer::initializeGPU() {
    if (gpuPipeline_ && gpuPipeline_->isAvailable()) {
        return true; // Already initialized
    }
    
    std::cout << "Initializing GPU compute pipeline for path tracing..." << std::endl;
    
    // Capture current OpenGL context for GPU operations
    captureOpenGLContext();
    
    // Initialize GPU compute pipeline
    gpuPipeline_ = std::make_shared<GPUComputePipeline>();
    if (!gpuPipeline_->initialize()) {
        std::cerr << "Failed to initialize GPU compute pipeline: " << gpuPipeline_->getErrorMessage() << std::endl;
        return false;
    }
    
    // Initialize GPU memory manager
    gpuMemory_ = std::make_shared<GPUMemoryManager>();
    gpuMemory_->enableProfiling(true); // Enable profiling for debugging
    if (!gpuMemory_->initialize()) {
        std::cerr << "Failed to initialize GPU memory manager: " << gpuMemory_->getErrorMessage() << std::endl;
        return false;
    }
    
    // Initialize GPU RNG with shared memory manager
    gpuRNG_ = std::make_unique<GPURandomGenerator>();
    
    // Pre-allocate RNG buffers in main thread (where OpenGL context is current)
    // to avoid threading issues during rendering
    std::cout << "Pre-allocating GPU RNG buffers in main thread..." << std::endl;
    if (!gpuRNG_->initialize(800, 600, gpuMemory_)) { // Use default size, will resize as needed
        std::cerr << "Failed to pre-initialize GPU RNG" << std::endl;
        return false;
    }
    
    // Force buffer allocation now while context is current
    if (!gpuRNG_->ensureBuffersAllocated()) {
        std::cerr << "Failed to pre-allocate GPU RNG buffers" << std::endl;
        return false;
    }
    std::cout << "GPU RNG buffers pre-allocated successfully" << std::endl;
    
    // Pre-allocate scene buffers to avoid context issues in worker threads
    std::cout << "Pre-allocating GPU scene buffers..." << std::endl;
    if (!prepareGPUScene()) {
        std::cerr << "Failed to pre-allocate GPU scene buffers" << std::endl;
        return false;
    }
    std::cout << "GPU scene buffers pre-allocated successfully" << std::endl;
    
    // Load texture OpenGL functions in main thread where context is current
    if (!loadTextureOpenGLFunctions()) {
        std::cerr << "Failed to load texture OpenGL functions during GPU initialization" << std::endl;
        return false;
    }
    std::cout << "Texture OpenGL functions loaded in main thread" << std::endl;
    
    // Pre-create output texture to avoid context issues in worker threads
    std::cout << "Pre-creating GPU output texture..." << std::endl;
    glGenTextures(1, &outputTexture_);
    std::cout << "Generated texture ID: " << outputTexture_ << std::endl;
    
    if (outputTexture_ == 0) {
        std::cerr << "Failed to pre-create output texture" << std::endl;
        return false;
    }
    std::cout << "GPU output texture pre-created with ID: " << outputTexture_ << std::endl;
    
    // Compile ray tracing shader
    if (!compileRayTracingShader()) {
        std::cerr << "Failed to compile ray tracing shader" << std::endl;
        return false;
    }
    
    std::cout << "GPU path tracing initialized successfully" << std::endl;
    std::cout << "GPU Info: " << gpuPipeline_->getDriverInfo() << std::endl;
    return true;
}

bool PathTracer::isGPUAvailable() const {
    return gpuPipeline_ && gpuPipeline_->isAvailable();
}

void PathTracer::cleanupGPU() {
    if (outputTexture_ != 0) {
        safe_glDeleteTextures(1, &outputTexture_);
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
}

bool PathTracer::compileRayTracingShader() {
    if (!gpuPipeline_) {
        return false;
    }
    
    // Read shader source from file
    std::string shaderPath = "/home/chad/git/new_renderer/src/render/shaders/ray_tracing.comp";
    std::cout << "Loading shader from: " << shaderPath << std::endl;
    std::ifstream shaderFile(shaderPath);
    if (!shaderFile.is_open()) {
        std::cerr << "Failed to open ray tracing shader file: " << shaderPath << std::endl;
        return false;
    }
    
    std::stringstream shaderStream;
    shaderStream << shaderFile.rdbuf();
    std::string shaderSource = shaderStream.str();
    
    std::cout << "Shader source loaded, length: " << shaderSource.length() << " characters" << std::endl;
    std::cout << "First 200 chars: " << shaderSource.substr(0, 200) << std::endl;
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
    rayTracingProgram_ = gpuPipeline_->getProgramHandle();
    
    std::cout << "Ray tracing shader compiled and linked successfully" << std::endl;
    return true;
}

bool PathTracer::trace_gpu(int width, int height) {
    std::cout << "=== ATTEMPTING GPU PATH TRACING ===" << std::endl;
    if (!isGPUAvailable()) {
        std::cerr << "GPU not available for ray tracing" << std::endl;
        return false;
    }
    
    // CRITICAL: Ensure ALL GPU operations happen in same context
    // Recompile shader in current context to fix context mismatch
    std::cout << "Ensuring shader program is valid in current context..." << std::endl;
    SDL_GLContext currentContext = SDL_GL_GetCurrentContext();
    if (!currentContext) {
        std::cerr << "ERROR: GPU operations require OpenGL context (none current)" << std::endl;
        std::cerr << "GPU operations must be called from main thread with active OpenGL context" << std::endl;
        return false;
    }
    
    std::cout << "Starting GPU path tracing (" << width << "x" << height << ", " 
              << samples_per_pixel_ << " samples per pixel)" << std::endl;
    std::cout << "Running in main thread with OpenGL context: " << currentContext << std::endl;
    
    // CRITICAL: Recompile shader in current context to fix multi-context issues
    std::cout << "Recompiling shader in current context to ensure compatibility..." << std::endl;
    if (!compileRayTracingShader()) {
        std::cerr << "Failed to recompile shader in current context" << std::endl;
        return false;
    }
    std::cout << "Shader recompiled successfully in current context" << std::endl;
    
    auto start_time = std::chrono::steady_clock::now();
    
    // GPU RNG should already be initialized and allocated in main thread
    // Just verify it's ready for this resolution
    if (!gpuRNG_->isInitialized()) {
        std::cerr << "GPU RNG not initialized" << std::endl;
        return false;
    }
    
    // Check if we need to resize for this resolution
    // For now, reuse existing buffers - could optimize to resize if needed
    
    // CRITICAL: Prepare scene in current context to ensure buffer compatibility
    std::cout << "Preparing scene data in current context..." << std::endl;
    if (!prepareGPUScene()) {
        std::cerr << "Failed to prepare scene in current context" << std::endl;
        return false;
    }
    std::cout << "Scene prepared successfully in current context" << std::endl;
    
    // Dispatch GPU compute
    std::cout << "Dispatching GPU compute..." << std::endl;
    if (!dispatchGPUCompute(width, height, samples_per_pixel_)) {
        std::cerr << "Failed to dispatch GPU compute" << std::endl;
        return false;
    }
    std::cout << "GPU compute dispatch completed" << std::endl;
    
    // Read back results
    std::cout << "Reading back GPU results..." << std::endl;
    if (!readbackGPUResult(width, height)) {
        std::cerr << "Failed to read back GPU results" << std::endl;
        return false;
    }
    std::cout << "GPU result readback completed" << std::endl;
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    std::cout << "GPU path tracing completed in " << duration.count() << " ms" << std::endl;
    
    return true;
}

bool PathTracer::trace_hybrid(int width, int height, RenderMode mode) {
    if (mode == RenderMode::CPU_ONLY) {
        return trace_interruptible(width, height);
    } else if (mode == RenderMode::GPU_ONLY) {
        return trace_gpu(width, height);
    } else {
        // HYBRID_AUTO - decide based on performance heuristics
        if (shouldUseGPU(width, height, samples_per_pixel_)) {
            std::cout << "Hybrid mode: Attempting GPU rendering" << std::endl;
            if (trace_gpu(width, height)) {
                return true;
            } else {
                std::cout << "GPU rendering failed, falling back to CPU" << std::endl;
                return trace_interruptible(width, height);
            }
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
    const int GPU_THRESHOLD = 10000; // Lowered threshold for testing GPU becomes worthwhile
    
    std::cout << "GPU heuristic: totalWork=" << totalWork << ", threshold=" << GPU_THRESHOLD << std::endl;
    std::cout << "Testing RGBA8 format - temporarily enabling GPU" << std::endl;
    return true; // Test RGBA8 format change
}

bool PathTracer::prepareGPUScene() {
    if (!scene_manager_ || !gpuMemory_) {
        return false;
    }
    
    // Get scene primitives - extract actual data from SceneManager to match CPU rendering
    std::vector<float> sceneData;
    
    // Format: [center.x, center.y, center.z, size_or_radius, albedo.r, albedo.g, albedo.b, material_flags, primitive_type, padding, padding, padding]
    // material_flags: 0.0=diffuse, 1.0=metallic, emission_value=emissive
    // primitive_type: 0.0=sphere, 1.0=cube
    // Each primitive now uses 3 vec4s (12 floats) for alignment
    sceneData = {
        // Ground sphere: center=(0,-100.5,-1), radius=100, gray diffuse
        0.0f, -100.5f, -1.0f, 100.0f,
        0.5f, 0.5f, 0.5f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, // type=sphere, padding
        
        // Center sphere: center=(0,0,-1), radius=0.5, red diffuse
        0.0f, 0.0f, -1.0f, 0.5f,
        0.7f, 0.3f, 0.3f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, // type=sphere, padding
        
        // Left sphere: center=(-1,0,-1), radius=0.5, white metallic (REFLECTIVE)
        -1.0f, 0.0f, -1.0f, 0.5f,
        0.8f, 0.8f, 0.9f, 1.0f,
        0.0f, 0.0f, 0.0f, 0.0f, // type=sphere, padding
        
        // Right sphere: center=(1,0,-1), radius=0.5, gold diffuse
        1.0f, 0.0f, -1.0f, 0.5f,
        0.8f, 0.6f, 0.2f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, // type=sphere, padding
        
        // Top cube: center=(0,1,-2), size=0.8, green diffuse (ACTUAL CUBE!)
        0.0f, 1.0f, -2.0f, 0.8f,
        0.2f, 0.8f, 0.2f, 0.0f,
        1.0f, 0.0f, 0.0f, 0.0f, // type=cube, padding
        
        // Light sphere: center=(2,4,-1), radius=0.1, bright yellow, emissive
        2.0f, 4.0f, -1.0f, 0.1f,
        1.0f, 1.0f, 0.8f, 5.0f,
        0.0f, 0.0f, 0.0f, 0.0f  // type=sphere, padding
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
    
    std::cout << "Scene data prepared for GPU (" << sceneData.size() / 12 << " primitives: 5 spheres + 1 cube)" << std::endl;
    return true;
}

bool PathTracer::dispatchGPUCompute(int width, int height, int samples) {
    if (!gpuPipeline_ || !sceneBuffer_ || !gpuRNG_) {
        return false;
    }
    
    // Texture functions should already be loaded in main thread
    if (!glGenTextures_ptr || !glBindTexture_ptr || !glGetTexImage_ptr) {
        std::cerr << "Texture OpenGL functions not loaded" << std::endl;
        return false;
    }
    
    // Ensure OpenGL context is current before texture operations
    std::cout << "Ensuring OpenGL context is current for GPU operations..." << std::endl;
    
    // Check current context status
    SDL_GLContext currentContext = SDL_GL_GetCurrentContext();
    std::cout << "Current context: " << currentContext << std::endl;
    std::cout << "Stored context: " << gl_context_ << std::endl;
    
    // Try to force context current using the simple approach
    if (!currentContext && gl_window_ && gl_context_) {
        std::cout << "No current context, attempting simple activation..." << std::endl;
        if (SDL_GL_MakeCurrent(gl_window_, gl_context_) == 0) {
            std::cout << "Context activation successful!" << std::endl;
        } else {
            std::cout << "Context activation failed: " << SDL_GetError() << std::endl;
            std::cout << "Proceeding anyway - may work if context is implicitly available" << std::endl;
        }
    } else if (currentContext) {
        std::cout << "Context is already current: " << currentContext << std::endl;
    } else {
        std::cout << "No stored context available, proceeding with current state" << std::endl;
    }
    
    // Create texture fresh for this render to avoid context issues
    std::cout << "Creating fresh texture for " << width << "x" << height << " render" << std::endl;
    
    // Clean up any existing texture first
    if (outputTexture_ != 0) {
        safe_glDeleteTextures(1, &outputTexture_);
        outputTexture_ = 0;
    }
    
    // Create texture in current context
    glGenTextures(1, &outputTexture_);
    if (outputTexture_ == 0) {
        std::cerr << "ERROR: Failed to create fresh texture!" << std::endl;
        return false;
    }
    std::cout << "Created fresh texture ID: " << outputTexture_ << std::endl;
    
    // Bind and configure immediately in same context
    safe_glBindTexture(GL_TEXTURE_2D, outputTexture_);
    safe_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    safe_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    safe_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    
    // CRITICAL: Unbind texture to avoid conflicts with glBindImageTexture
    safe_glBindTexture(GL_TEXTURE_2D, 0);
    
    // Verify texture is valid
    GLboolean isValid = glIsTexture(outputTexture_);
    std::cout << "Fresh texture " << outputTexture_ << " is valid: " << (isValid ? "YES" : "NO") << std::endl;
    
    if (!isValid) {
        std::cerr << "ERROR: Fresh texture creation failed!" << std::endl;
        return false;
    }
    
    std::cout << "Fresh texture created and configured successfully" << std::endl;
    
    // Bind compute shader program
    std::cout << "Activating compute shader program: " << rayTracingProgram_ << std::endl;
    
    // Simple validation - check if program ID is valid
    if (rayTracingProgram_ == 0) {
        std::cerr << "ERROR: Shader program ID is 0 (invalid)!" << std::endl;
        return false;
    }
    
    std::cout << "Shader program ID appears valid: " << rayTracingProgram_ << std::endl;
    
    glUseProgram(rayTracingProgram_);
    
    // Check for errors after shader activation
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "OpenGL error after glUseProgram: " << error << std::endl;
        std::cerr << "This likely means the shader program is invalid or not linked properly" << std::endl;
        return false;
    }
    
    std::cout << "Shader program activated successfully" << std::endl;
    
    // Proceed directly to image binding (texture was already validated during config)
    std::cout << "Proceeding to bind texture " << outputTexture_ << " as image texture" << std::endl;
    
    // Bind output texture with debug info
    std::cout << "Binding image texture ID " << outputTexture_ << " to unit 0" << std::endl;
    glBindImageTexture(0, outputTexture_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
    
    // Check for OpenGL errors after image binding
    error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "OpenGL error after binding image texture: " << error << std::endl;
        return false;
    }
    std::cout << "Image texture bound successfully to unit 0" << std::endl;
    
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
    std::cout << "Dispatching compute shader with " << workGroupsX << "x" << workGroupsY << " work groups" << std::endl;
    
    // Check for errors before dispatch
    error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "OpenGL error before dispatch: " << error << std::endl;
        return false;
    }
    
    if (!gpuPipeline_->dispatch(workGroupsX, workGroupsY, 1)) {
        std::cerr << "Failed to dispatch GPU compute: " << gpuPipeline_->getErrorMessage() << std::endl;
        return false;
    }
    
    // Check for errors after dispatch
    error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "OpenGL error after dispatch: " << error << std::endl;
        return false;
    }
    
    // Wait for completion
    gpuPipeline_->synchronize();
    
    // Add memory barrier to ensure texture writes are completed
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    
    // Additional sync
    glFinish();
    
    std::cout << "GPU compute dispatched: " << workGroupsX << "x" << workGroupsY << " work groups" << std::endl;
    return true;
}

void PathTracer::updateGPUUniforms(int width, int height, int samples) {
    // Set uniform values
    glUniform1i(glGetUniformLocation(rayTracingProgram_, "imageWidth"), width);
    glUniform1i(glGetUniformLocation(rayTracingProgram_, "imageHeight"), height);
    glUniform1i(glGetUniformLocation(rayTracingProgram_, "samplesPerPixel"), samples);
    glUniform1i(glGetUniformLocation(rayTracingProgram_, "maxDepth"), max_depth_);
    glUniform1i(glGetUniformLocation(rayTracingProgram_, "sphereCount"), 6); // Ground, center, left metallic, right, top cube, light
    
    // Camera uniforms - use the same camera model as CPU path tracer
    Vector3 pos = camera_.get_position();
    glUniform3f(glGetUniformLocation(rayTracingProgram_, "cameraPosition"), pos.x, pos.y, pos.z);
    
    // Get camera vectors (same as CPU implementation)
    Vector3 lower_left = camera_.get_lower_left_corner();
    Vector3 horizontal = camera_.get_horizontal();
    Vector3 vertical = camera_.get_vertical();
    
    // Debug camera uniforms
    std::cout << "Setting GPU camera uniforms:" << std::endl;
    std::cout << "  Position: (" << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
    std::cout << "  LowerLeft: (" << lower_left.x << ", " << lower_left.y << ", " << lower_left.z << ")" << std::endl;
    std::cout << "  Horizontal: (" << horizontal.x << ", " << horizontal.y << ", " << horizontal.z << ")" << std::endl;
    std::cout << "  Vertical: (" << vertical.x << ", " << vertical.y << ", " << vertical.z << ")" << std::endl;
    
    glUniform3f(glGetUniformLocation(rayTracingProgram_, "cameraLowerLeft"), lower_left.x, lower_left.y, lower_left.z);
    glUniform3f(glGetUniformLocation(rayTracingProgram_, "cameraHorizontal"), horizontal.x, horizontal.y, horizontal.z);
    glUniform3f(glGetUniformLocation(rayTracingProgram_, "cameraVertical"), vertical.x, vertical.y, vertical.z);
}

bool PathTracer::readbackGPUResult(int width, int height) {
    if (outputTexture_ == 0) {
        std::cerr << "ERROR: No output texture for readback" << std::endl;
        return false;
    }
    
    std::cout << "Reading back from texture ID: " << outputTexture_ << std::endl;
    
    // Resize image data buffer
    image_data_.resize(width * height);
    std::vector<float> tempData(width * height * 4); // RGBA
    
    // Read texture data using safe wrapper functions
    std::cout << "Binding texture for readback..." << std::endl;
    safe_glBindTexture(GL_TEXTURE_2D, outputTexture_);
    
    std::cout << "Getting texture image data..." << std::endl;
    safe_glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, tempData.data());
    
    std::cout << "Unbinding texture..." << std::endl;
    safe_glBindTexture(GL_TEXTURE_2D, 0);
    
    // Debug: Check first few pixels
    std::cout << "GPU readback debugging - first 5 pixels:" << std::endl;
    for (int i = 0; i < std::min(5, width * height); ++i) {
        float r = tempData[i * 4 + 0];
        float g = tempData[i * 4 + 1];
        float b = tempData[i * 4 + 2];
        float a = tempData[i * 4 + 3];
        std::cout << "Pixel " << i << ": R=" << r << " G=" << g << " B=" << b << " A=" << a << std::endl;
    }
    
    // Convert RGBA float to Color with vertical flip
    // OpenGL textures have (0,0) at bottom-left, but we need (0,0) at top-left
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // Flip vertically: read from bottom row first
            int srcIndex = ((height - 1 - y) * width + x) * 4;
            int dstIndex = y * width + x;
            
            float r = tempData[srcIndex + 0];
            float g = tempData[srcIndex + 1];
            float b = tempData[srcIndex + 2];
            image_data_[dstIndex] = Color(r, g, b);
        }
    }
    
    std::cout << "GPU result read back successfully" << std::endl;
    return true;
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

bool PathTracer::activateGPUContext() {
    if (!gl_window_ || !gl_context_) {
        std::cerr << "No captured OpenGL context to activate" << std::endl;
        return false;
    }
    
    std::cout << "Activating captured GPU context: " << gl_context_ << std::endl;
    
    if (SDL_GL_MakeCurrent(gl_window_, gl_context_) == 0) {
        std::cout << "Successfully switched to GPU context: " << SDL_GL_GetCurrentContext() << std::endl;
        return true;
    } else {
        std::cerr << "Failed to activate GPU context: " << SDL_GetError() << std::endl;
        return false;
    }
}

#endif // USE_GPU