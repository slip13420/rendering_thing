#pragma once

#include "core/common.h"
#include <memory>

// Forward declarations
class UIInput;
class SceneManager;
class RenderEngine;

class UIManager {
public:
    UIManager();
    ~UIManager();
    
    void initialize();
    void update();
    void render();
    void shutdown();
    
    // Set dependencies
    void set_scene_manager(std::shared_ptr<SceneManager> scene_manager);
    void set_render_engine(std::shared_ptr<RenderEngine> render_engine);
    
    // Input handling
    bool should_quit() const;
    void process_input();
    
private:
    bool initialized_;
    std::shared_ptr<UIInput> ui_input_;
    std::shared_ptr<SceneManager> scene_manager_;
    std::shared_ptr<RenderEngine> render_engine_;
};