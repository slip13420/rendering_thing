#include "core/common.h"
#include "ui/ui_manager.h"
#include "ui/ui_input.h"
#include "render/render_engine.h"
#include "render/image_output.h"
#include "core/scene_manager.h"
#include <iostream>
#include <memory>
#include <chrono>
#include <thread>

#ifdef USE_SDL
#include <SDL.h>
#endif

#ifdef USE_GLFW
#include <GLFW/glfw3.h>
#endif

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    std::cout << "Path Tracer Renderer v1.0.0" << std::endl;
    std::cout << "Built with C++17" << std::endl;
    
#ifdef USE_SDL
    std::cout << "Using SDL for windowing" << std::endl;
#endif

#ifdef USE_GLFW
    std::cout << "Using GLFW for windowing" << std::endl;
#endif

    try {
        auto render_engine = std::make_shared<RenderEngine>();
        auto ui_manager = std::make_shared<UIManager>();
        auto image_output = std::make_shared<ImageOutput>();
        
        // Get scene manager from render engine to connect components
        auto scene_manager = std::make_shared<SceneManager>();
        scene_manager->initialize();
        render_engine->set_scene_manager(scene_manager);
        render_engine->set_image_output(image_output);
        
        // Connect UI to render engine, scene manager, and image output
        ui_manager->set_scene_manager(scene_manager);
        ui_manager->set_render_engine(render_engine);
        ui_manager->set_image_output(image_output);
        ui_manager->initialize();
        
        // Setup primitive management after UIManager is created as shared_ptr
        ui_manager->setup_primitive_management();
        
        // Connect progress callback from render engine to UI manager
        render_engine->set_progress_callback([&ui_manager](int w, int h, int current, int target) {
            ui_manager->update_progress(w, h, current, target);
        });
        
        // Connect save callback from UI input to UI manager
        if (auto ui_input = ui_manager->get_ui_input()) {
            ui_input->set_save_callback([&ui_manager]() {
                ui_manager->trigger_save_dialog();
            });
        }
        
        std::cout << "Application initialized successfully!" << std::endl;
        
        // Configure rendering settings based on GPU availability
        if (render_engine->is_gpu_available()) {
            std::cout << "GPU detected - using HD resolution (1280x720)" << std::endl;
            render_engine->set_render_size(1280, 720);
            render_engine->set_samples_per_pixel(4); // Reduced from 10 for faster live operations
        } else {
            std::cout << "No GPU - using low resolution for CPU rendering (320x240)" << std::endl;
            render_engine->set_render_size(320, 240);
            render_engine->set_samples_per_pixel(1); // Single sample for fast CPU performance
        }
        render_engine->set_max_depth(3);
        render_engine->set_camera_position(Vector3(0, 2, 3), Vector3(0, 0, 0), Vector3(0, 1, 0));
        
        std::cout << "Real-Time Camera Control Mode" << std::endl;
        std::cout << "=============================" << std::endl;
        
        // Create a quick preview render at moderate resolution that scales to fill window
        std::cout << "Creating quick preview render..." << std::endl;
        int original_samples = (render_engine->is_gpu_available()) ? 4 : 1;
        int target_width = render_engine->is_gpu_available() ? 1280 : 320;
        int target_height = render_engine->is_gpu_available() ? 720 : 240;
        
        // Use target resolution for preview to avoid window size conflicts
        render_engine->set_render_size(target_width, target_height);
        render_engine->set_samples_per_pixel(1);   // Single sample for speed
        render_engine->set_max_depth(2);           // Shallow depth for speed
        render_engine->render();
        render_engine->display_image();
        
        // Restore target resolution settings for responsive operation
        render_engine->set_render_size(target_width, target_height);
        render_engine->set_samples_per_pixel(original_samples);  // Restore samples
        render_engine->set_max_depth(3);           // Restore normal depth
        
        // Now that OpenGL context is available, do a quick GPU update if possible
        if (render_engine->is_gpu_available()) {
            std::cout << "OpenGL context available - updating with GPU render..." << std::endl;
            render_engine->set_samples_per_pixel(2);  // Low samples for quick update
            bool gpu_success = render_engine->render_gpu_main_thread();
            if (gpu_success) {
                render_engine->display_image();
                std::cout << "Frame updated with GPU render!" << std::endl;
            } else {
                std::cout << "GPU update failed, keeping CPU preview" << std::endl;
            }
            render_engine->set_samples_per_pixel(original_samples);  // Restore settings
        }
        
        std::cout << "Application ready - press G for full quality render!" << std::endl;
        
        std::cout << "SDL window opened! Use WASD+RF keys to move camera." << std::endl;
        std::cout << "Press H for help, Q or ESC to quit." << std::endl;
        
        // Real-time SDL event loop with non-blocking progressive rendering support
        while (!ui_manager->should_quit()) {
            ui_manager->process_input();
            ui_manager->update();
            ui_manager->render(); // Show progress feedback
            
            // Process non-blocking progressive GPU rendering steps
            if (render_engine->is_progressive_gpu_active()) {
                render_engine->step_progressive_gpu();
            }
            
            // Small delay to prevent 100% CPU usage
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
        }
        
        std::cout << "Application shutting down..." << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}