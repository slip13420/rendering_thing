#include "ui_manager.h"

UIManager::UIManager() : initialized_(false) {
    initialize();
}

UIManager::~UIManager() {
    if (initialized_) {
        shutdown();
    }
}

void UIManager::initialize() {
    std::cout << "UIManager initialized" << std::endl;
    initialized_ = true;
}

void UIManager::update() {
    
}

void UIManager::render() {
    
}

void UIManager::shutdown() {
    std::cout << "UIManager shutdown" << std::endl;
    initialized_ = false;
}