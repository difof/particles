#pragma once

#include "../../save_manager.hpp"
#include "../../simulation/simulation.hpp"
#include "../../undo.hpp"
#include "../../window_config.hpp"
#include "config.hpp"

// per-frame context passed to renderers
struct Context {
    Simulation &sim;
    Config &rcfg;
    mailbox::render::ReadView &view;
    const WindowConfig &wcfg;
    SaveManager &save;
    UndoManager &undo;

    // interpolation
    bool can_interpolate = false;
    float interp_alpha = 1.0f;

    bool should_exit = false;

    Context(Simulation &sim, Config &rcfg, mailbox::render::ReadView &view,
            const WindowConfig &wcfg, bool can_interpolate, float interp_alpha,
            UndoManager &undo, SaveManager &save)
        : sim(sim), rcfg(rcfg), view(view), wcfg(wcfg), save(save), undo(undo),
          can_interpolate(can_interpolate), interp_alpha(interp_alpha) {}
};
