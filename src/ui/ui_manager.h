#pragma once

#include "core/common.h"
#include <memory>
#include <string>
#include <chrono>

// Forward declarations
class UIInput;
class SceneManager;
class RenderEngine;
class ImageOutput;
enum class RenderState;
enum class ImageFormat;
struct SaveOptions;

// Progressive rendering progress data
struct ProgressData {
    int current_samples = 0;
    int target_samples = 0;
    int width = 0;
    int height = 0;
    float progress_percentage = 0.0f;
    float samples_per_second = 0.0f;
    float estimated_time_remaining = 0.0f; // seconds
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point last_update_time;
};

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
    void set_image_output(std::shared_ptr<ImageOutput> image_output);
    
    // Input handling
    bool should_quit() const;
    void process_input();
    
    // Progress tracking
    void update_progress(int width, int height, int current_samples, int target_samples);
    void reset_progress();
    
    // Save functionality
    void trigger_save_dialog();
    bool is_save_enabled() const;
    
    // Get UI components for configuration
    std::shared_ptr<UIInput> get_ui_input() { return ui_input_; }
    
private:
    void render_start_button();
    void render_stop_button();
    void render_save_button();
    void render_status_display();
    void render_progress_display();
    void render_progressive_controls();
    std::string get_render_state_text(RenderState state) const;
    void on_render_state_changed(RenderState state);
    std::string format_time(float seconds) const;
    
    // Save functionality
    void show_save_dialog();
    bool save_image_with_options(const std::string& filename, ImageFormat format, int quality = 90);
    std::string get_default_filename() const;
    ImageFormat get_format_from_user_input() const;
    
    bool initialized_;
    std::shared_ptr<UIInput> ui_input_;
    std::shared_ptr<SceneManager> scene_manager_;
    std::shared_ptr<RenderEngine> render_engine_;
    std::shared_ptr<ImageOutput> image_output_;
    
    RenderState current_render_state_;
    ProgressData progress_data_;
    bool show_progress_details_;
};