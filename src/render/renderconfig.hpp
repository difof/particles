#ifndef __RENDERCONFIG_HPP
#define __RENDERCONFIG_HPP

#include <cstdint>

struct RenderConfig {
    // Interpolation (render-only)
    bool interpolate = true;
    float interp_delay_ms = 50.0f;

    // Glow tunables
    float core_size = 1.5f;        // solid circle radius
    float outer_scale_mul = 14.0f; // halo size = core_size * outer_scale_mul
    float outer_rgb_gain = 0.60f;  // 0..1, scales RGB (alpha stays 255)
    float inner_scale_mul =
        3.0f; // small inner halo = core_size * inner_scale_mul
    float inner_rgb_gain = 0.18f; // 0..1, scales RGB (alpha stays 255)

    // Composition
    bool final_additive_blit = true; // DrawTextureRec with BLEND_ADDITIVE
};

#endif
