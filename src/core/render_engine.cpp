#include "render_engine.h"

RenderEngine::RenderEngine() : initialized_(false) {
    initialize();
}

RenderEngine::~RenderEngine() {
    if (initialized_) {
        shutdown();
    }
}

void RenderEngine::initialize() {
    std::cout << "RenderEngine initialized" << std::endl;
    initialized_ = true;
}

void RenderEngine::render() {
    
}

void RenderEngine::shutdown() {
    std::cout << "RenderEngine shutdown" << std::endl;
    initialized_ = false;
}