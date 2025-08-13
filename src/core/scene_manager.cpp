#include "scene_manager.h"

SceneManager::SceneManager() : initialized_(false) {
    
}

SceneManager::~SceneManager() {
    
}

void SceneManager::initialize() {
    initialized_ = true;
}

void SceneManager::update() {
    
}

void SceneManager::shutdown() {
    initialized_ = false;
}