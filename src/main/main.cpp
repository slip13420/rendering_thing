#include "core/common.h"
#include "ui/ui_manager.h"
#include "render/render_engine.h"
#include <iostream>
#include <memory>

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
        auto render_engine = std::make_unique<RenderEngine>();
        auto ui_manager = std::make_unique<UIManager>();
        
        std::cout << "Application initialized successfully!" << std::endl;
        std::cout << "Press any key to exit..." << std::endl;
        std::cin.get();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}