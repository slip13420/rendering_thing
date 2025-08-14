#pragma once

#include "core/common.h"
#include <memory>

// Forward declarations
class SceneManager;
class RenderEngine;

class UIInput {
public:
    UIInput();
    ~UIInput();
    
    void processEvents();
    bool shouldQuit() const;
    
    // Set dependencies for camera control
    void set_scene_manager(std::shared_ptr<SceneManager> scene_manager);
    void set_render_engine(std::shared_ptr<RenderEngine> render_engine);
    
    // Camera position controls
    Vector3 get_camera_position() const;
    void set_camera_position(const Vector3& position);
    bool validate_camera_position(const Vector3& position) const;
    
private:
    void handle_camera_input();
    void print_camera_controls() const;
    
    bool quit_requested_;
    std::shared_ptr<SceneManager> scene_manager_;
    std::shared_ptr<RenderEngine> render_engine_;
    
    // Camera movement settings
    float camera_move_speed_;
};