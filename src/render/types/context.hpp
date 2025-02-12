#pragma once

#include "../../mailbox/data_snapshot.hpp"
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

    // World snapshot for safe access
    mailbox::WorldSnapshot world_snapshot;

    // Managers for UI operations
    SaveManager &save;
    UndoManager &undo;

    bool can_interpolate = false;
    float interp_alpha = 1.0f;

    bool should_exit = false;

    Context(Simulation &sim, Config &rcfg, mailbox::render::ReadView &view,
            const WindowConfig &wcfg, bool can_interpolate, float interp_alpha,
            mailbox::WorldSnapshot world_snapshot, SaveManager &save,
            UndoManager &undo)
        : sim(sim), rcfg(rcfg), view(view), wcfg(wcfg),
          can_interpolate(can_interpolate), interp_alpha(interp_alpha),
          world_snapshot(world_snapshot), save(save), undo(undo) {}
};
