#include "ui_input.h"
#include "core/scene_manager.h"
#include "render/render_engine.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cmath>

#ifdef USE_SDL
#include <SDL.h>
#endif

UIInput::UIInput() : quit_requested_(false), camera_move_speed_(0.5f), 
                     mouse_sensitivity_(0.0005f), mouse_captured_(false), 
                     last_mouse_x_(0), last_mouse_y_(0), 
                     camera_yaw_(0.0f), camera_pitch_(0.0f) {
    print_camera_controls();
}

UIInput::~UIInput() {
    
}

void UIInput::processEvents() {
#ifdef USE_SDL
    SDL_Event event;
    bool has_events = false;
    
    while (SDL_PollEvent(&event)) {
        has_events = true;
        if (event.type == SDL_QUIT) {
            quit_requested_ = true;
        } else if (event.type == SDL_KEYDOWN) {
            handle_realtime_camera_input(event.key.keysym.sym);
        } else if (event.type == SDL_MOUSEMOTION) {
            handle_mouse_look(event.motion.x, event.motion.y);
        }
    }
    
    // Always refresh the display if there were any events (including mouse)
    // This ensures the window stays responsive to mouse/window manager events
    if (has_events && render_engine_) {
        render_engine_->display_image();
    }
#else
    // Console-based input for non-SDL builds
    handle_camera_input();
#endif
}

bool UIInput::shouldQuit() const {
    return quit_requested_;
}

void UIInput::set_scene_manager(std::shared_ptr<SceneManager> scene_manager) {
    scene_manager_ = scene_manager;
}

void UIInput::set_render_engine(std::shared_ptr<RenderEngine> render_engine) {
    render_engine_ = render_engine;
}

Vector3 UIInput::get_camera_position() const {
    if (scene_manager_) {
        return scene_manager_->get_camera_position();
    }
    return Vector3(0, 0, 0);
}

void UIInput::set_camera_position(const Vector3& position) {
    if (scene_manager_ && validate_camera_position(position)) {
        Vector3 current_pos = scene_manager_->get_camera_position();
        Vector3 target = Vector3(0, 0, 0); // Look at origin
        Vector3 up = Vector3(0, 1, 0);
        
        scene_manager_->set_camera_position(position);
        
        // Update render engine
        if (render_engine_) {
            render_engine_->set_camera_position(position, target, up);
        }
        
        std::cout << "Camera moved to: (" << std::fixed << std::setprecision(2) 
                  << position.x << ", " << position.y << ", " << position.z << ")" << std::endl;
    } else {
        std::cout << "Invalid camera position!" << std::endl;
    }
}

bool UIInput::validate_camera_position(const Vector3& position) const {
    if (scene_manager_) {
        return scene_manager_->is_valid_camera_position(position);
    }
    return false;
}

void UIInput::handle_camera_input() {
    std::cout << "\nCamera Controls - Enter command (q to quit, h for help): ";
    std::string input;
    std::getline(std::cin, input);
    
    if (input.empty()) return;
    
    char command = input[0];
    Vector3 current_pos = get_camera_position();
    Vector3 new_pos = current_pos;
    
    switch (command) {
        case 'q':
        case 'Q':
            quit_requested_ = true;
            break;
        case 'h':
        case 'H':
            print_camera_controls();
            break;
        case 'w':
        case 'W':
            new_pos.z -= camera_move_speed_;
            set_camera_position(new_pos);
            break;
        case 's':
        case 'S':
            new_pos.z += camera_move_speed_;
            set_camera_position(new_pos);
            break;
        case 'a':
        case 'A':
            new_pos.x -= camera_move_speed_;
            set_camera_position(new_pos);
            break;
        case 'd':
        case 'D':
            new_pos.x += camera_move_speed_;
            set_camera_position(new_pos);
            break;
        case 'r':
        case 'R':
            new_pos.y += camera_move_speed_;
            set_camera_position(new_pos);
            break;
        case 'f':
        case 'F':
            new_pos.y -= camera_move_speed_;
            set_camera_position(new_pos);
            break;
        case 'p':
        case 'P':
            std::cout << "Current camera position: (" << std::fixed << std::setprecision(2)
                      << current_pos.x << ", " << current_pos.y << ", " << current_pos.z << ")" << std::endl;
            break;
        case 'c':
        case 'C':
            {
                std::cout << "Enter new camera position (x y z): ";
                std::string pos_input;
                std::getline(std::cin, pos_input);
                std::istringstream iss(pos_input);
                float x, y, z;
                if (iss >> x >> y >> z) {
                    set_camera_position(Vector3(x, y, z));
                } else {
                    std::cout << "Invalid input format. Use: x y z" << std::endl;
                }
            }
            break;
        case 'o':
        case 'O':
            set_camera_position(Vector3(0, 0, 3)); // Reset to default
            break;
        default:
            std::cout << "Unknown command. Press 'h' for help." << std::endl;
            break;
    }
}

void UIInput::handle_realtime_camera_input(int keycode) {
#ifdef USE_SDL
    Vector3 current_pos = get_camera_position();
    bool moved = false;
    
    // Calculate camera-relative movement vectors
    Vector3 forward, right, up;
    get_camera_vectors(forward, right, up);
    
    Vector3 new_pos = current_pos;
    
    switch (keycode) {
        case SDLK_w:
            new_pos = current_pos + forward * camera_move_speed_;
            moved = true;
            break;
        case SDLK_s:
            new_pos = current_pos - forward * camera_move_speed_;
            moved = true;
            break;
        case SDLK_a:
            new_pos = current_pos - right * camera_move_speed_;
            moved = true;
            break;
        case SDLK_d:
            new_pos = current_pos + right * camera_move_speed_;
            moved = true;
            break;
        case SDLK_r:
            new_pos = current_pos + up * camera_move_speed_;
            moved = true;
            break;
        case SDLK_f:
            new_pos = current_pos - up * camera_move_speed_;
            moved = true;
            break;
        case SDLK_q:
        case SDLK_ESCAPE:
            quit_requested_ = true;
            break;
        case SDLK_p:
            std::cout << "Camera position: (" << std::fixed << std::setprecision(2)
                      << current_pos.x << ", " << current_pos.y << ", " << current_pos.z << ")" << std::endl;
            break;
        case SDLK_o:
            // Reset camera position and orientation
            camera_yaw_ = 0.0f;
            camera_pitch_ = 0.0f;
            set_camera_position(Vector3(0, 2, 3));
            update_camera_target();
            std::cout << "Camera reset to origin" << std::endl;
            break;
        case SDLK_h:
            print_camera_controls();
            break;
    }
    
    if (moved) {
        set_camera_position(new_pos);
        update_camera_target(); // Update target to maintain current view direction
        
        // Trigger automatic re-render
        if (render_engine_) {
            render_engine_->render();
            render_engine_->display_image();
        }
    }
#endif
}

void UIInput::handle_mouse_look(int mouse_x, int mouse_y) {
#ifdef USE_SDL
    if (!mouse_captured_) {
        // Initialize mouse position on first capture
        last_mouse_x_ = mouse_x;
        last_mouse_y_ = mouse_y;
        mouse_captured_ = true;
        return;
    }
    
    // Calculate mouse movement delta
    int delta_x = mouse_x - last_mouse_x_;
    int delta_y = mouse_y - last_mouse_y_;
    
    // Update camera rotation angles
    camera_yaw_ += delta_x * mouse_sensitivity_;
    camera_pitch_ -= delta_y * mouse_sensitivity_; // Invert Y for natural feel
    
    // Clamp pitch to prevent camera flipping
    const float max_pitch = 1.5f; // ~85 degrees
    if (camera_pitch_ > max_pitch) camera_pitch_ = max_pitch;
    if (camera_pitch_ < -max_pitch) camera_pitch_ = -max_pitch;
    
    // Update camera target based on new angles
    update_camera_target();
    
    // Store current mouse position for next frame
    last_mouse_x_ = mouse_x;
    last_mouse_y_ = mouse_y;
    
    // Trigger re-render only for significant movement to improve performance
    if (render_engine_ && (abs(delta_x) > 1 || abs(delta_y) > 1)) {
        render_engine_->render();
        render_engine_->display_image();
    }
#endif
}

void UIInput::update_camera_target() {
    if (!scene_manager_) return;
    
    Vector3 current_pos = scene_manager_->get_camera_position();
    
    // Calculate new target based on yaw and pitch (spherical coordinates)
    Vector3 direction;
    direction.x = cos(camera_pitch_) * sin(camera_yaw_);
    direction.y = sin(camera_pitch_);
    direction.z = cos(camera_pitch_) * cos(camera_yaw_);
    
    Vector3 target = current_pos + direction.normalized();
    Vector3 up = Vector3(0, 1, 0);
    
    // Update the render engine with new camera orientation
    if (render_engine_) {
        render_engine_->set_camera_position(current_pos, target, up);
    }
}

void UIInput::get_camera_vectors(Vector3& forward, Vector3& right, Vector3& up) const {
    // Calculate forward direction from yaw and pitch
    forward.x = cos(camera_pitch_) * sin(camera_yaw_);
    forward.y = sin(camera_pitch_);
    forward.z = cos(camera_pitch_) * cos(camera_yaw_);
    forward = forward.normalized();
    
    // World up vector
    Vector3 world_up = Vector3(0, 1, 0);
    
    // Right vector is cross product of forward and world up
    right = forward.cross(world_up).normalized();
    
    // Up vector is cross product of right and forward
    up = right.cross(forward).normalized();
}

void UIInput::print_camera_controls() const {
    std::cout << "\n=== REAL-TIME CAMERA CONTROLS ===" << std::endl;
    std::cout << "W/S - Move Forward/Backward" << std::endl;
    std::cout << "A/D - Move Right/Left" << std::endl;
    std::cout << "R/F - Move Up/Down" << std::endl;
    std::cout << "MOUSE - Look around (first-person view)" << std::endl;
    std::cout << "P   - Print current position" << std::endl;
    std::cout << "O   - Reset to origin (0 0 3)" << std::endl;
    std::cout << "H   - Show this help" << std::endl;
    std::cout << "Q/ESC - Quit application" << std::endl;
    std::cout << "Move camera with keys and mouse for live preview!" << std::endl;
    std::cout << "==================================" << std::endl;
}