#pragma once

#include "core/common.h"
#include <memory>
#include <functional>
#include <set>
#include <chrono>

// Forward declarations
class SceneManager;
class RenderEngine;
class UIManager;

class UIInput {
public:
    UIInput();
    ~UIInput();
    
    void processEvents();
    bool shouldQuit() const;
    
    // Set dependencies for camera control
    void set_scene_manager(std::shared_ptr<SceneManager> scene_manager);
    void set_render_engine(std::shared_ptr<RenderEngine> render_engine);
    
    // Set callback for save functionality
    void set_save_callback(std::function<void()> callback) { save_callback_ = callback; }
    
    // Camera position controls
    Vector3 get_camera_position() const;
    void set_camera_position(const Vector3& position);
    bool validate_camera_position(const Vector3& position) const;
    
private:
    void handle_camera_input();
    void handle_realtime_camera_input(int keycode);
    void handle_camera_key_release(int keycode);
    void handle_mouse_look(int delta_x, int delta_y);
    void handle_direct_mouse_input(); // Direct polling bypass
    void update_camera_target();
    void get_camera_vectors(Vector3& forward, Vector3& right, Vector3& up) const;
    void print_camera_controls() const;
    
    bool quit_requested_;
    std::shared_ptr<SceneManager> scene_manager_;
    std::shared_ptr<RenderEngine> render_engine_;
    std::function<void()> save_callback_;
    
    // Camera movement settings
    float camera_move_speed_;
    float mouse_sensitivity_;
    
    // Mouse look state
    bool mouse_captured_;
    int last_mouse_x_;
    int last_mouse_y_;
    float camera_yaw_;   // Horizontal rotation
    float camera_pitch_; // Vertical rotation
    bool use_raw_mouse_; // For raw mouse input handling
    std::chrono::steady_clock::time_point mouse_release_time_; // Track when mouse was released
    
    // Camera movement state tracking
    std::set<int> pressed_camera_keys_;
};