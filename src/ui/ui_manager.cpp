#include "ui_manager.h"
#include "ui_input.h"
#include "core/scene_manager.h"
#include "render/render_engine.h"
#include <iostream>

UIManager::UIManager() : initialized_(false), current_render_state_(RenderState::IDLE) {
    
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
    // Display render controls
    render_status_display();
    render_start_button();
    render_stop_button();
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
    
    // Set up state change callback
    if (render_engine_) {
        render_engine_->set_state_change_callback(
            [this](RenderState state) { on_render_state_changed(state); });
        current_render_state_ = render_engine_->get_render_state();
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

void UIManager::render_start_button() {
    if (!render_engine_) {
        std::cout << "[Start Render Button: DISABLED - No render engine]" << std::endl;
        return;
    }
    
    bool enabled = (current_render_state_ == RenderState::IDLE || 
                    current_render_state_ == RenderState::COMPLETED ||
                    current_render_state_ == RenderState::STOPPED ||
                    current_render_state_ == RenderState::ERROR);
    
    std::cout << "[Start Render Button: " << (enabled ? "ENABLED" : "DISABLED") << "]" << std::endl;
    
    if (enabled) {
        std::cout << "  Click to start rendering" << std::endl;
    }
}

void UIManager::render_stop_button() {
    if (!render_engine_) {
        std::cout << "[Stop Render Button: DISABLED - No render engine]" << std::endl;
        return;
    }
    
    bool enabled = (current_render_state_ == RenderState::RENDERING);
    
    std::cout << "[Stop Render Button: " << (enabled ? "ENABLED" : "DISABLED") << "]" << std::endl;
    
    if (enabled) {
        std::cout << "  Click to stop rendering" << std::endl;
    }
}

void UIManager::render_status_display() {
    std::string status_text = get_render_state_text(current_render_state_);
    std::cout << "[Render Status: " << status_text << "]" << std::endl;
}

std::string UIManager::get_render_state_text(RenderState state) const {
    switch (state) {
        case RenderState::IDLE:      return "Ready to render";
        case RenderState::RENDERING: return "Rendering in progress...";
        case RenderState::COMPLETED: return "Render completed successfully";
        case RenderState::STOPPED:   return "Render stopped by user";
        case RenderState::ERROR:     return "Render failed with error";
        default:                     return "Unknown state";
    }
}

void UIManager::on_render_state_changed(RenderState state) {
    current_render_state_ = state;
    std::cout << "Render state changed to: " << get_render_state_text(state) << std::endl;
}