#ifndef __RENDERCONFIG_HPP
#define __RENDERCONFIG_HPP

struct RenderConfig {
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

    // overlays
    bool show_density_heat = false;
    float heat_alpha = 0.6f; // 0..1 overlay opacity
    bool show_velocity_field = false;
    float vel_scale = 0.75f;      // px per (avg speed unit)
    float vel_thickness = 1.0f;   // line thickness
    bool show_grid_lines = false; // debug cell grid
};

#endif
