#include "core/common.h"
#include "ui/ui_manager.h"
#include "render/render_engine.h"
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
        auto ui_manager = std::make_unique<UIManager>();
        
        // Get scene manager from render engine to connect components
        auto scene_manager = std::make_shared<SceneManager>();
        scene_manager->initialize();
        render_engine->set_scene_manager(scene_manager);
        
        // Connect UI to render engine and scene manager
        ui_manager->set_scene_manager(scene_manager);
        ui_manager->set_render_engine(render_engine);
        ui_manager->initialize();
        
        std::cout << "Application initialized successfully!" << std::endl;
        
        // Configure rendering settings for real-time interaction
        render_engine->set_render_size(320, 240);
        render_engine->set_samples_per_pixel(2); // Even lower for better refresh rate
        render_engine->set_max_depth(3);
        render_engine->set_camera_position(Vector3(0, 2, 3), Vector3(0, 0, 0), Vector3(0, 1, 0));
        
        std::cout << "Real-Time Camera Control Mode" << std::endl;
        std::cout << "=============================" << std::endl;
        
        // Do initial render and display
        std::cout << "Performing initial render..." << std::endl;
        render_engine->render();
        render_engine->display_image();
        
        std::cout << "SDL window opened! Use WASD+RF keys to move camera." << std::endl;
        std::cout << "Press H for help, Q or ESC to quit." << std::endl;
        
        // Real-time SDL event loop
        while (!ui_manager->should_quit()) {
            ui_manager->process_input();
            
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