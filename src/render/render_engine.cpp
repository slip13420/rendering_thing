#include "render_engine.h"
#include "path_tracer.h"
#include "core/scene_manager.h"
#include "core/camera.h"
#include "image_output.h"
#include <iostream>

RenderEngine::RenderEngine() 
    : initialized_(false), render_width_(800), render_height_(600) {
    initialize();
}

RenderEngine::~RenderEngine() {
    if (initialized_) {
        shutdown();
    }
}

void RenderEngine::initialize() {
    // Create default components
    path_tracer_ = std::make_shared<PathTracer>();
    scene_manager_ = std::make_shared<SceneManager>();
    image_output_ = std::make_shared<ImageOutput>();
    
    // Initialize scene manager
    scene_manager_->initialize();
    
    // Configure path tracer with scene manager
    path_tracer_->set_scene_manager(scene_manager_);
    path_tracer_->set_max_depth(10);
    path_tracer_->set_samples_per_pixel(10);
    
    // Use camera from scene manager
    if (scene_manager_->get_camera()) {
        path_tracer_->set_camera(*scene_manager_->get_camera());
    }
    
    initialized_ = true;
    std::cout << "RenderEngine initialized with path tracing support" << std::endl;
}

void RenderEngine::render() {
    if (!initialized_) {
        std::cerr << "RenderEngine not initialized!" << std::endl;
        return;
    }
    
    std::cout << "Starting render (" << render_width_ << "x" << render_height_ << ")" << std::endl;
    
    // Execute path tracing
    path_tracer_->trace(render_width_, render_height_);
    
    // Get the rendered image data and pass it to image output
    const auto& image_data = path_tracer_->get_image_data();
    image_output_->set_image_data(image_data, render_width_, render_height_);
    
    std::cout << "Render completed successfully" << std::endl;
}

void RenderEngine::shutdown() {
    if (scene_manager_) {
        scene_manager_->shutdown();
    }
    path_tracer_.reset();
    scene_manager_.reset();
    image_output_.reset();
    initialized_ = false;
    std::cout << "RenderEngine shutdown" << std::endl;
}

void RenderEngine::set_scene_manager(std::shared_ptr<SceneManager> scene_manager) {
    scene_manager_ = scene_manager;
    if (path_tracer_) {
        path_tracer_->set_scene_manager(scene_manager_);
    }
}

void RenderEngine::set_image_output(std::shared_ptr<ImageOutput> image_output) {
    image_output_ = image_output;
}

void RenderEngine::set_render_size(int width, int height) {
    render_width_ = width;
    render_height_ = height;
    
    // Update camera aspect ratio in scene manager's camera
    if (scene_manager_ && scene_manager_->get_camera()) {
        auto camera = scene_manager_->get_camera();
        camera->set_aspect_ratio(float(width) / float(height));
        if (path_tracer_) {
            path_tracer_->set_camera(*camera);
        }
    }
}

void RenderEngine::set_max_depth(int depth) {
    if (path_tracer_) {
        path_tracer_->set_max_depth(depth);
    }
}

void RenderEngine::set_samples_per_pixel(int samples) {
    if (path_tracer_) {
        path_tracer_->set_samples_per_pixel(samples);
    }
}

void RenderEngine::set_camera_position(const Vector3& position, const Vector3& target, const Vector3& up) {
    if (scene_manager_) {
        // Update the camera in scene manager
        scene_manager_->set_camera_position(position);
        scene_manager_->set_camera_target(target);
        scene_manager_->set_camera_up(up);
        
        // Update path tracer with the modified camera
        if (path_tracer_ && scene_manager_->get_camera()) {
            path_tracer_->set_camera(*scene_manager_->get_camera());
        }
    }
}

void RenderEngine::save_image(const std::string& filename) {
    if (image_output_) {
        image_output_->save_to_file(filename);
    } else {
        std::cerr << "No image output component available" << std::endl;
    }
}

void RenderEngine::display_image() {
    if (image_output_) {
        image_output_->display_to_screen();
    } else {
        std::cerr << "No image output component available" << std::endl;
    }
}