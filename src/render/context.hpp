#ifndef __RENDER_CONTEXT_HPP
#define __RENDER_CONTEXT_HPP

#include "../simulation/simulation.hpp"
#include "../types.hpp"
#include "renderconfig.hpp"

// Minimal per-frame context passed to renderers
struct RenderContext {
    Simulation &sim;
    RenderConfig &rcfg;
    mailbox::DrawBuffer::ReadView &view;
    const WindowConfig &wcfg;

    // interpolation
    bool can_interpolate = false;
    float interp_alpha = 1.0f;

    bool should_exit = false;
};

#endif
