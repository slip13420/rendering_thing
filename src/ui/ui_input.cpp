#include "ui/ui_input.h"
#include "ui/ui_manager.h"
#include "core/scene_manager.h"
#include "render/render_engine.h"
#include "render/path_tracer.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <chrono>
#include <random>

#ifdef USE_SDL
#include <SDL.h>
#endif

UIInput::UIInput() : quit_requested_(false), camera_move_speed_(0.5f), 
                     mouse_sensitivity_(0.004f), mouse_captured_(false), 
                     last_mouse_x_(0), last_mouse_y_(0), 
                     camera_yaw_(-1.57f), camera_pitch_(0.0f), use_raw_mouse_(false) {  // Start looking forward (-Z)
    
#ifdef USE_SDL
    // Set SDL hints for better mouse responsiveness
    SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "1");
    SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_WARP_MOTION, "1");
    SDL_SetHint(SDL_HINT_MOUSE_AUTO_CAPTURE, "0");
    SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_SCALING, "0"); // Disable scaling
    SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_SPEED_SCALE, "1.0"); // No speed scaling
#endif
    
    print_camera_controls();
}

UIInput::~UIInput() {
    
}

void UIInput::processEvents() {
#ifdef USE_SDL
    SDL_Event event;
    bool has_events = false;
    
    // FIRST: Handle direct mouse state polling (bypass event queue entirely)
    handle_direct_mouse_input();
    
    // Process all non-mouse events in the queue
    while (SDL_PollEvent(&event)) {
        has_events = true;
        if (event.type == SDL_QUIT) {
            quit_requested_ = true;
        } else if (event.type == SDL_KEYDOWN) {
            handle_realtime_camera_input(event.key.keysym.sym);
        } else if (event.type == SDL_KEYUP) {
            handle_camera_key_release(event.key.keysym.sym);
        } else if (event.type == SDL_MOUSEBUTTONDOWN) {
            if (event.button.button == SDL_BUTTON_RIGHT) {
                mouse_captured_ = true;
                // Clear ALL mouse events - we're using direct polling now
                SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
                SDL_SetRelativeMouseMode(SDL_TRUE);
                
                // Get initial mouse position for delta calculation
                SDL_GetMouseState(&last_mouse_x_, &last_mouse_y_);
                
                // Mouse look enable logging removed for cleaner output
            }
        } else if (event.type == SDL_MOUSEBUTTONUP) {
            if (event.button.button == SDL_BUTTON_RIGHT) {
                // IMMEDIATE STOP
                mouse_captured_ = false;
                SDL_SetRelativeMouseMode(SDL_FALSE);
                
                // Nuclear clear - we don't want ANY events
                for (int i = 0; i < 10; i++) {
                    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
                    SDL_PumpEvents();
                }
                
                // Mouse look disable logging removed for cleaner output
            }
        }
        // NOTE: We're completely ignoring SDL_MOUSEMOTION events now
    }
    
    // ALWAYS update display if there were any events, 
    // BUT avoid interfering with active progressive GPU rendering
    if (has_events && render_engine_) {
        // Don't call display_image() during progressive GPU rendering as it interferes
        // with the progressive display updates and can cause image flipping
        if (!render_engine_->is_progressive_gpu_active()) {
            render_engine_->display_image();
        }
        // If progressive rendering is active, let it handle its own display updates
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
        
        // Camera position logging removed for cleaner output
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
        case 'g':
        case 'G':
            if (render_engine_) {
                std::cout << "Starting render..." << std::endl;
                render_engine_->start_render();
            }
            break;
        case 't':
        case 'T':
            if (render_engine_) {
                std::cout << "Stopping render..." << std::endl;
                render_engine_->stop_render();
            }
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
    bool is_camera_key = false;
    
    // Calculate camera-relative movement vectors
    Vector3 forward, right, up;
    get_camera_vectors(forward, right, up);
    
    Vector3 new_pos = current_pos;
    
    switch (keycode) {
        case SDLK_w:
            new_pos = current_pos + forward * camera_move_speed_;
            moved = true;
            is_camera_key = true;
            break;
        case SDLK_s:
            new_pos = current_pos - forward * camera_move_speed_;
            moved = true;
            is_camera_key = true;
            break;
        case SDLK_a:
            new_pos = current_pos - right * camera_move_speed_;
            moved = true;
            is_camera_key = true;
            break;
        case SDLK_d:
            new_pos = current_pos + right * camera_move_speed_;
            moved = true;
            is_camera_key = true;
            break;
        case SDLK_r:
            new_pos = current_pos + up * camera_move_speed_;
            moved = true;
            is_camera_key = true;
            break;
        case SDLK_f:
            new_pos = current_pos - up * camera_move_speed_;
            moved = true;
            is_camera_key = true;
            break;
        case SDLK_q:
        case SDLK_ESCAPE:
            quit_requested_ = true;
            break;
        // TAB key removed - now using right-click for mouse capture
        case SDLK_p:
            std::cout << "Camera position: (" << std::fixed << std::setprecision(2)
                      << current_pos.x << ", " << current_pos.y << ", " << current_pos.z << ")" << std::endl;
            break;
        case SDLK_o:
            // Reset camera position and orientation
            camera_yaw_ = -1.57f;  // Look forward (-Z)
            camera_pitch_ = 0.0f;
            set_camera_position(Vector3(0, 2, 3));
            update_camera_target();
            // Display existing image without triggering render
            // BUT avoid interfering with progressive GPU rendering
            if (render_engine_ && !render_engine_->is_progressive_gpu_active()) {
                render_engine_->display_image();
            }
            std::cout << "Camera reset to origin" << std::endl;
            break;
        case SDLK_h:
            print_camera_controls();
            break;
        case SDLK_g:
            std::cout << "G key pressed!" << std::endl;
            if (render_engine_) {
                std::cout << "Starting standard render..." << std::endl;
                render_engine_->start_render();
            } else {
                std::cout << "No render engine available!" << std::endl;
            }
            break;
        case SDLK_u:
            std::cout << "U key pressed - Testing GPU Main Thread Rendering!" << std::endl;
            if (render_engine_) {
                std::cout << "Attempting GPU rendering in main thread..." << std::endl;
                bool success = render_engine_->render_gpu_main_thread();
                if (success) {
                    std::cout << "GPU main thread rendering completed successfully!" << std::endl;
                    render_engine_->display_image();
                } else {
                    std::cout << "GPU main thread rendering failed!" << std::endl;
                }
            } else {
                std::cout << "No render engine available!" << std::endl;
            }
            break;
        case SDLK_m:
            std::cout << "M key pressed - Starting Progressive Render!" << std::endl;
            if (render_engine_) {
                ProgressiveConfig config;
                config.initialSamples = 1;
                config.targetSamples = 500;   // Balanced quality/performance for responsive rendering
                config.progressiveSteps = 10;  // Optimal steps for gradual quality improvement
                config.updateInterval = 0.2f;
                
                // Try GPU progressive rendering first (non-blocking)
                if (render_engine_->is_gpu_available()) {
                    std::cout << "Starting non-blocking GPU progressive rendering (1->500 samples, 10 steps)..." << std::endl;
                    bool success = render_engine_->start_progressive_gpu_non_blocking(config);
                    if (success) {
                        std::cout << "GPU progressive rendering started (non-blocking mode)" << std::endl;
                    } else {
                        std::cout << "GPU progressive rendering failed, falling back to CPU..." << std::endl;
                        // Fallback to CPU progressive
                        config.targetSamples = 300;   // CPU gets lower target for performance
                        config.progressiveSteps = 10;
                        config.updateInterval = 0.3f;
                        std::cout << "Starting CPU progressive render (1->300 samples, 10 steps)..." << std::endl;
                        render_engine_->start_progressive_render(config);
                    }
                } else {
                    // No GPU available, use CPU progressive
                    config.targetSamples = 300;
                    config.progressiveSteps = 10;
                    config.updateInterval = 0.3f;
                    std::cout << "Starting CPU progressive render (1->300 samples, 10 steps)..." << std::endl;
                    render_engine_->start_progressive_render(config);
                }
            } else {
                std::cout << "No render engine available!" << std::endl;
            }
            break;
        case SDLK_t:
            if (render_engine_) {
                std::cout << "Stopping render..." << std::endl;
                render_engine_->stop_render();
                // Also cancel progressive GPU rendering if active
                if (render_engine_->is_progressive_gpu_active()) {
                    render_engine_->cancel_progressive_gpu();
                }
            }
            break;
        case SDLK_x:
            if (render_engine_ && render_engine_->is_progressive_gpu_active()) {
                std::cout << "X key pressed - Cancelling progressive rendering..." << std::endl;
                render_engine_->cancel_progressive_gpu();
            } else {
                std::cout << "X key pressed - No progressive rendering to cancel" << std::endl;
            }
            break;
        case SDLK_v:
            std::cout << "V key pressed - Save Image!" << std::endl;
            if (save_callback_) {
                save_callback_();
            } else {
                std::cout << "Save functionality not available" << std::endl;
            }
            break;
        case SDLK_1:
            std::cout << "1 key pressed - Add Sphere!" << std::endl;
            if (scene_manager_) {
                PrimitiveID id = scene_manager_->addPrimitive(PrimitiveType::SPHERE, Vector3(0, 0, -2), Color::red(), generate_random_material(Color::red()));
                std::cout << "Added Sphere with ID " << id << std::endl;
                // Trigger immediate render to show the change
                if (render_engine_ && !render_engine_->is_progressive_gpu_active()) {
                    render_engine_->render();
                    render_engine_->display_image();
                }
            } else {
                std::cout << "No scene manager available!" << std::endl;
            }
            break;
        case SDLK_2:
            std::cout << "2 key pressed - Add Cube!" << std::endl;
            if (scene_manager_) {
                PrimitiveID id = scene_manager_->addPrimitive(PrimitiveType::CUBE, Vector3(1, 0, -2), Color::green(), generate_random_material(Color::green()));
                std::cout << "Added Cube with ID " << id << std::endl;
                // Trigger immediate render to show the change
                if (render_engine_ && !render_engine_->is_progressive_gpu_active()) {
                    render_engine_->render();
                    render_engine_->display_image();
                }
            } else {
                std::cout << "No scene manager available!" << std::endl;
            }
            break;
        case SDLK_3:
            std::cout << "3 key pressed - Add Torus!" << std::endl;
            if (scene_manager_) {
                PrimitiveID id = scene_manager_->addPrimitive(PrimitiveType::TORUS, Vector3(-1, 0, -2), Color::blue(), generate_random_material(Color::blue()));
                std::cout << "Added Torus with ID " << id << std::endl;
                // Trigger immediate render to show the change
                if (render_engine_ && !render_engine_->is_progressive_gpu_active()) {
                    render_engine_->render();
                    render_engine_->display_image();
                }
            } else {
                std::cout << "No scene manager available!" << std::endl;
            }
            break;
        case SDLK_4:
            std::cout << "4 key pressed - Add Pyramid!" << std::endl;
            if (scene_manager_) {
                PrimitiveID id = scene_manager_->addPrimitive(PrimitiveType::PYRAMID, Vector3(0, 1, -2), Color::white(), generate_random_material(Color::white()));
                std::cout << "Added Pyramid with ID " << id << std::endl;
                // Trigger immediate render to show the change
                if (render_engine_ && !render_engine_->is_progressive_gpu_active()) {
                    render_engine_->render();
                    render_engine_->display_image();
                }
            } else {
                std::cout << "No scene manager available!" << std::endl;
            }
            break;
        case SDLK_l:
            std::cout << "L key pressed - List Primitives!" << std::endl;
            if (ui_manager_) {
                ui_manager_->show_primitive_list();
            } else {
                std::cout << "No UI manager available!" << std::endl;
            }
            break;
        case SDLK_DELETE:
        case SDLK_BACKSPACE:
            std::cout << "Delete key pressed - Remove Last Primitive!" << std::endl;
            if (scene_manager_) {
                auto objects = scene_manager_->get_objects();
                if (!objects.empty()) {
                    // Find the primitive ID for the last object and remove it
                    auto last_object = objects.back();
                    scene_manager_->remove_object(last_object);
                    std::cout << "Removed last primitive from scene" << std::endl;
                    // Trigger immediate render to show the change
                    if (render_engine_ && !render_engine_->is_progressive_gpu_active()) {
                        render_engine_->render();
                        render_engine_->display_image();
                    }
                } else {
                    std::cout << "No primitives to remove!" << std::endl;
                }
            } else {
                std::cout << "No scene manager available!" << std::endl;
            }
            break;
    }
    
    // Track camera key presses for movement detection
    if (is_camera_key) {
        pressed_camera_keys_.insert(keycode);
    }
    
    if (moved) {
        set_camera_position(new_pos);
        update_camera_target(); // Update target to maintain current view direction
        
        // Only display existing image, don't trigger new render
        // BUT avoid interfering with progressive GPU rendering
        if (render_engine_ && !render_engine_->is_progressive_gpu_active()) {
            render_engine_->display_image();
        }
    }
#endif
}

void UIInput::handle_direct_mouse_input() {
#ifdef USE_SDL
    // Only poll if mouse is captured
    if (!mouse_captured_) return;
    
    // Get current mouse position directly (no event queue)
    int current_x, current_y;
    SDL_GetMouseState(&current_x, &current_y);
    
    // Calculate deltas from last position
    int delta_x = current_x - last_mouse_x_;
    int delta_y = current_y - last_mouse_y_;
    
    // Update last position immediately
    last_mouse_x_ = current_x;
    last_mouse_y_ = current_y;
    
    // Skip tiny movements
    if (abs(delta_x) <= 1 && abs(delta_y) <= 1) return;
    
    // Cap deltas to prevent jumps
    const int max_delta = 10;
    delta_x = std::max(-max_delta, std::min(max_delta, delta_x));
    delta_y = std::max(-max_delta, std::min(max_delta, delta_y));
    
    // Apply mouse look
    camera_yaw_ += delta_x * mouse_sensitivity_;
    camera_pitch_ -= delta_y * mouse_sensitivity_;
    
    // Clamp pitch
    const float max_pitch = 1.5f;
    if (camera_pitch_ > max_pitch) camera_pitch_ = max_pitch;
    if (camera_pitch_ < -max_pitch) camera_pitch_ = -max_pitch;
    
    // Update camera and display immediately
    // BUT avoid interfering with progressive GPU rendering
    update_camera_target();
    if (render_engine_ && !render_engine_->is_progressive_gpu_active()) {
        render_engine_->display_image();
    }
#endif
}

void UIInput::handle_mouse_look(int delta_x, int delta_y) {
#ifdef USE_SDL
    // Legacy event-based mouse handler - now unused since we switched to direct polling
    // Keeping for compatibility but this should never be called
    return;
#endif
}

void UIInput::update_camera_target() {
    if (!scene_manager_) return;
    
    Vector3 current_pos = scene_manager_->get_camera_position();
    
    // Calculate new direction based on yaw and pitch (spherical coordinates)
    // Fixed coordinate system: yaw rotates around Y, pitch around local X
    Vector3 direction;
    direction.x = cos(camera_pitch_) * cos(camera_yaw_);
    direction.y = sin(camera_pitch_);
    direction.z = cos(camera_pitch_) * sin(camera_yaw_);
    
    Vector3 target = current_pos + direction.normalized();
    Vector3 up = Vector3(0, 1, 0);
    
    // Update the render engine with new camera orientation
    if (render_engine_) {
        render_engine_->set_camera_position(current_pos, target, up);
        // Update camera preview to show movement
        render_engine_->update_camera_preview(current_pos, target);
    }
}

void UIInput::get_camera_vectors(Vector3& forward, Vector3& right, Vector3& up) const {
    // Calculate forward direction from yaw and pitch (consistent with update_camera_target)
    forward.x = cos(camera_pitch_) * cos(camera_yaw_);
    forward.y = sin(camera_pitch_);
    forward.z = cos(camera_pitch_) * sin(camera_yaw_);
    forward = forward.normalized();
    
    // World up vector
    Vector3 world_up = Vector3(0, 1, 0);
    
    // Right vector is cross product of forward and world up
    right = forward.cross(world_up).normalized();
    
    // Up vector is cross product of right and forward
    up = right.cross(forward).normalized();
}

Material UIInput::generate_random_material(const Color& base_color) const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<float> roughness_dist(0.0f, 1.0f);
    static std::uniform_real_distribution<float> metallic_dist(0.0f, 1.0f);
    static std::uniform_int_distribution<int> material_type(0, 4);
    
    int type = material_type(gen);
    
    switch (type) {
        case 0: { // Shiny metal (low roughness, high metallic)
            float rough = roughness_dist(gen) * 0.2f;
            float metal = 0.8f + metallic_dist(gen) * 0.2f;
            std::cout << "  → Generated: Shiny Metal surface (roughness=" << rough << ", metallic=" << metal << ")" << std::endl;
            return Material(base_color, rough, metal, 0.0f);
        }
        case 1: { // Rough metal (high roughness, high metallic)  
            float rough = 0.6f + roughness_dist(gen) * 0.4f;
            float metal = 0.7f + metallic_dist(gen) * 0.3f;
            std::cout << "  → Generated: Rough Metal surface (roughness=" << rough << ", metallic=" << metal << ")" << std::endl;
            return Material(base_color, rough, metal, 0.0f);
        }
        case 2: { // Glossy plastic (low roughness, low metallic)
            float rough = roughness_dist(gen) * 0.3f;
            float metal = metallic_dist(gen) * 0.1f;
            std::cout << "  → Generated: Glossy Plastic surface (roughness=" << rough << ", metallic=" << metal << ")" << std::endl;
            return Material(base_color, rough, metal, 0.0f);
        }
        case 3: { // Matte surface (high roughness, low metallic)
            float rough = 0.7f + roughness_dist(gen) * 0.3f;
            float metal = metallic_dist(gen) * 0.1f;
            std::cout << "  → Generated: Matte Diffuse surface (roughness=" << rough << ", metallic=" << metal << ")" << std::endl;
            return Material(base_color, rough, metal, 0.0f);
        }
        default: { // Mirror-like (very low roughness, high metallic)
            float rough = roughness_dist(gen) * 0.1f;
            float metal = 0.9f + metallic_dist(gen) * 0.1f;
            std::cout << "  → Generated: Mirror-like surface (roughness=" << rough << ", metallic=" << metal << ")" << std::endl;
            return Material(base_color, rough, metal, 0.0f);
        }
    }
}

void UIInput::print_camera_controls() const {
    std::cout << "\n=== REAL-TIME CAMERA CONTROLS ===" << std::endl;
    std::cout << "W/S - Move Forward/Backward" << std::endl;
    std::cout << "A/D - Move Right/Left" << std::endl;
    std::cout << "R/F - Move Up/Down" << std::endl;
    std::cout << "RIGHT MOUSE - Hold to look around" << std::endl;
    std::cout << "P   - Print current position" << std::endl;
    std::cout << "O   - Reset to origin (0 0 3)" << std::endl;
    std::cout << "H   - Show this help" << std::endl;
    std::cout << "Q/ESC - Quit application" << std::endl;
    std::cout << "\n=== PRIMITIVE MANAGEMENT ===" << std::endl;
    std::cout << "1   - Add Red Sphere (random material)" << std::endl;
    std::cout << "2   - Add Green Cube (random material)" << std::endl;
    std::cout << "3   - Add Blue Torus (random material)" << std::endl;
    std::cout << "4   - Add White Pyramid (random material)" << std::endl;
    std::cout << "L   - List all primitives in scene" << std::endl;
    std::cout << "DEL/BACKSPACE - Remove last primitive" << std::endl;
    std::cout << "\n=== RENDER CONTROLS ===" << std::endl;
    std::cout << "G   - Quick render (4 samples GPU, 1 sample CPU)" << std::endl;
    std::cout << "U   - GPU rendering in main thread (test)" << std::endl;
    std::cout << "M   - Progressive high-quality render (1->2000 samples)" << std::endl;
    std::cout << "T   - Stop/cancel rendering" << std::endl;
    std::cout << "X   - Cancel progressive rendering" << std::endl;
    std::cout << "V   - Save rendered image (after completion)" << std::endl;
    std::cout << "\nAdd primitives with 1-4, then use G for quick render, M for quality!" << std::endl;
    std::cout << "==================================" << std::endl;
}

void UIInput::handle_camera_key_release(int keycode) {
#ifdef USE_SDL
    // Check if this is a camera movement key
    bool is_camera_key = (keycode == SDLK_w || keycode == SDLK_s || 
                         keycode == SDLK_a || keycode == SDLK_d || 
                         keycode == SDLK_r || keycode == SDLK_f);
    
    if (is_camera_key) {
        // Remove from pressed keys set
        pressed_camera_keys_.erase(keycode);
        
        // If no camera keys are pressed, signal camera movement stopped
        if (pressed_camera_keys_.empty() && render_engine_) {
            render_engine_->stop_camera_movement();
        }
    }
#endif
}