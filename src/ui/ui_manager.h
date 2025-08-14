#pragma once

#include "core/common.h"
#include "render/render_engine.h"
#include <memory>
#include <string>

// Forward declarations
class UIInput;
class SceneManager;

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
    
    // Render controls
    void render_start_button();
    void render_stop_button();
    void render_status_display();
    std::string get_render_state_text(RenderState state) const;
    
private:
    void on_render_state_changed(RenderState state);
    
    bool initialized_;
    std::shared_ptr<UIInput> ui_input_;
    std::shared_ptr<SceneManager> scene_manager_;
    std::shared_ptr<RenderEngine> render_engine_;
    RenderState current_render_state_;
};