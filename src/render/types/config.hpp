#pragma once

#include <cmath>
#include <raylib.h>

struct CameraState {
    float x = 0.0f;
    float y = 0.0f;
    float zoom_log = 0.0f; // zoom = 2^zoom_log

    float zoom() const { return std::exp2f(zoom_log); }
};

struct Config {
    // ui
    bool show_ui = true;
    bool show_metrics_ui = false;
    bool show_editor = false;
    bool show_render_config = false;
    bool show_sim_config = false;
    bool show_history_ui = false;
    bool show_style_editor = false;

    // interpolation
    bool interpolate = true;
    float interp_delay_ms = 50.0f;

    // glow
    bool glow_enabled = true;
    float core_size = 1.5f;
    float outer_scale_mul = 14.0f;
    float outer_rgb_gain = 0.60f;
    float inner_scale_mul = 3.0f;
    float inner_rgb_gain = 0.18f;
    bool final_additive_blit = true;

    // background
    Color background_color = {0, 0, 0, 255}; // black background

    // border
    bool border_enabled = false;
    Color border_color = {255, 255, 255, 255}; // white border
    float border_width = 2.0f;                 // border thickness in pixels

    // overlays
    bool show_density_heat = false;
    float heat_alpha = 0.6f; // 0..1 overlay opacity
    bool show_velocity_field = false;
    float vel_scale = 0.75f;      // px per (avg speed unit)
    float vel_thickness = 1.0f;   // line thickness
    bool show_grid_lines = false; // debug cell grid

    // camera
    CameraState camera;
};
