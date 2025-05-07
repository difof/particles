#pragma once

#include <algorithm>
#include <cmath>
#include <imgui.h>
#include <random>
#include <raylib.h>
#include <string>
#include <vector>

#include <memory>

#include "../../mailbox/command/cmd_seedspec.hpp"
#include "../../undo/add_group_action.hpp"
#include "../../undo/clear_all_groups_action.hpp"
#include "../../undo/remove_group_action.hpp"
#include "../../undo/resize_group_action.hpp"
#include "../../undo/undo_manager.hpp"
#include "../../undo/value_action.hpp"
#include "../irenderer.hpp"
#include "../types/window.hpp"
#include "smart_randomizer.hpp"

/**
 * @brief UI component for editing particle groups and interaction rules.
 *
 * Provides a comprehensive interface for managing particle groups, their
 * properties, and the interaction rules between different groups. Supports
 * undo/redo functionality and live editing capabilities.
 */
class ParticleEditorUI : public IRenderer {
  public:
    ParticleEditorUI() = default;
    ~ParticleEditorUI() override = default;
    ParticleEditorUI(const ParticleEditorUI &) = delete;
    ParticleEditorUI(ParticleEditorUI &&) = delete;
    ParticleEditorUI &operator=(const ParticleEditorUI &) = delete;
    ParticleEditorUI &operator=(ParticleEditorUI &&) = delete;

    /**
     * @brief Renders the particle editor UI.
     * @param ctx The rendering context containing simulation state and
     * configuration.
     */
    void render(Context &ctx) override;

  protected:
    /**
     * @brief Renders the main UI components.
     * @param ctx The rendering context.
     */
    void render_ui(Context &ctx);

    /**
     * @brief Refreshes editor state from world snapshot.
     * @param ctx The rendering context.
     */
    void refresh_editor_from_world(Context &ctx);

    /**
     * @brief Renders group management controls (Add/Remove groups).
     * @param ctx The rendering context.
     */
    void render_group_management_controls(Context &ctx);

    /**
     * @brief Renders individual group editing interface.
     * @param ctx The rendering context.
     * @param group_index The index of the group being edited.
     */
    void render_group_editor(Context &ctx, int group_index);

    /**
     * @brief Renders group properties (enabled, color, radius).
     * @param ctx The rendering context.
     * @param group_index The index of the group.
     */
    void render_group_properties(Context &ctx, int group_index);

    /**
     * @brief Renders the enabled/disabled checkbox for a group.
     * @param ctx The rendering context.
     * @param group_index The index of the group.
     */
    void render_group_enabled_checkbox(Context &ctx, int group_index);

    /**
     * @brief Renders the color picker for a group.
     * @param ctx The rendering context.
     * @param group_index The index of the group.
     */
    void render_group_color_picker(Context &ctx, int group_index);

    /**
     * @brief Renders the radius slider for a group.
     * @param ctx The rendering context.
     * @param group_index The index of the group.
     */
    void render_group_radius_slider(Context &ctx, int group_index);

    /**
     * @brief Renders interaction rules for a group.
     * @param ctx The rendering context.
     * @param group_index The index of the group.
     */
    void render_group_rules(Context &ctx, int group_index);

    /**
     * @brief Renders a single group rule interaction.
     * @param ctx The rendering context.
     * @param group_index The source group index.
     * @param target_index The target group index.
     */
    void render_single_group_rule(Context &ctx, int group_index,
                                  int target_index);

    /**
     * @brief Renders apply controls and utility buttons.
     * @param ctx The rendering context.
     */
    void render_apply_controls(Context &ctx);

    /**
     * @brief Creates backup state for undo operations.
     * @param ctx The rendering context.
     * @return Backup state.
     */
    mailbox::command::SeedSpec create_backup_state(Context &ctx);

    /**
     * @brief Applies rule patch to simulation.
     * @param ctx The rendering context.
     * @param hot Whether to use hot apply (no reseed).
     */
    void apply_rule_patch(Context &ctx, bool hot);

  protected:
    struct EditorState {
        int m_group_count = 0;
        std::vector<float> m_radius_squared;
        std::vector<float> m_rules;
        std::vector<int> m_sizes;
        std::vector<Color> m_colors;
        std::vector<bool> m_enabled;
        bool m_live_apply = false;
        bool m_dirty = false;
        bool m_should_refresh_next_frame = false;
    } m_editor;
};
