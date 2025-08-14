#include "ui_manager.h"
#include "ui_input.h"
#include "core/scene_manager.h"
#include "render/render_engine.h"
#include <iostream>

UIManager::UIManager() : initialized_(false) {
    
}

UIManager::~UIManager() {
    if (initialized_) {
        shutdown();
    }
}

void UIManager::initialize() {
    ui_input_ = std::make_shared<UIInput>();
    
    // Set up input dependencies
    if (scene_manager_) {
        ui_input_->set_scene_manager(scene_manager_);
    }
    if (render_engine_) {
        ui_input_->set_render_engine(render_engine_);
    }
    
    std::cout << "UIManager initialized with camera controls" << std::endl;
    initialized_ = true;
}

void UIManager::update() {
    if (ui_input_) {
        ui_input_->processEvents();
    }
}

void UIManager::render() {
    // UI rendering would go here
}

void UIManager::shutdown() {
    ui_input_.reset();
    std::cout << "UIManager shutdown" << std::endl;
    initialized_ = false;
}

void UIManager::set_scene_manager(std::shared_ptr<SceneManager> scene_manager) {
    scene_manager_ = scene_manager;
    if (ui_input_) {
        ui_input_->set_scene_manager(scene_manager);
    }
}

void UIManager::set_render_engine(std::shared_ptr<RenderEngine> render_engine) {
    render_engine_ = render_engine;
    if (ui_input_) {
        ui_input_->set_render_engine(render_engine);
    }
}

bool UIManager::should_quit() const {
    return ui_input_ ? ui_input_->shouldQuit() : false;
}

void UIManager::process_input() {
    if (ui_input_) {
        ui_input_->processEvents();
    }
}