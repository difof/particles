#pragma once

#include "key_manager.hpp"

#include "render/types/config.hpp"
#include "render/types/context.hpp"
#include "render/ui/menu_bar_ui.hpp"
#include "save_manager.hpp"
#include "simulation/simulation.hpp"
#include "undo/undo_manager.hpp"

/**
 * @brief Sets up all keyboard shortcuts for the application.
 *
 * Registers handlers for simulation controls, UI toggles, camera movement,
 * file operations, and undo/redo functionality.
 *
 * @param key_manager The KeyManager instance to register handlers with
 * @param sim The simulation instance for simulation controls
 * @param rcfg The render configuration for UI toggles and camera controls
 * @param save_manager The save manager for file operations
 * @param undo_manager The undo manager for undo/redo operations
 * @param menu_bar The menu bar UI for file operations
 * @param should_exit Reference to boolean flag to set when exit is requested
 */
void setup_keys(KeyManager &key_manager, Simulation &sim, Config &rcfg,
                SaveManager &save_manager, UndoManager &undo_manager,
                MenuBarUI &menu_bar, bool &should_exit);
