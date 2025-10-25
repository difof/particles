#include "particle_editor_ui.hpp"
#include "../../utility/logger.hpp"

void ParticleEditorUI::render(Context &ctx) {
    if (!ctx.rcfg.show_ui || !ctx.rcfg.show_editor) {
        return;
    }
    render_ui(ctx);
}

void ParticleEditorUI::render_ui(Context &ctx) {
    auto &sim = ctx.sim;
    mailbox::SimulationStatsSnapshot stats = sim.get_stats();

    ImGui::Begin("[2] Particle & Rule Editor", &ctx.rcfg.show_editor);
    ImGui::SetWindowSize(ImVec2{600, 700}, ImGuiCond_FirstUseEver);

    // Refresh if group/particle counts changed (structural changes) or if we
    // should refresh next frame
    if (m_editor.m_group_count != stats.groups ||
        m_editor.m_group_count != stats.particles ||
        m_editor.m_should_refresh_next_frame) {
        refresh_editor_from_world(ctx);
    }

    ImGui::Text("Groups: %d", stats.groups);
    ImGui::SameLine();
    ImGui::Text("| negative forces attract, positive repels");
    ImGui::Separator();

    render_group_management_controls(ctx);

    ImGui::Separator();

    // Calculate available space for layout
    ImVec2 available_space = ImGui::GetContentRegionAvail();

    // Define height for the randomizer buttons
    float button_height = 30.0f; // Height for each button row
    float button_spacing = ImGui::GetStyle().ItemSpacing.y;

    // Calculate total height needed for buttons (2 rows of buttons + spacing)
    float total_button_height = (button_height * 2) + button_spacing;

    // Calculate height for the group editor (remaining space minus button area)
    float group_editor_height = available_space.y - total_button_height;

    // Ensure minimum height for group editor
    group_editor_height = std::max(group_editor_height, 200.0f);

    // Position the group editor to fill available space
    ImGui::BeginChild("GroupsRulesChild", ImVec2(0, group_editor_height), true,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar);
    for (int g = 0; g < m_editor.m_group_count; ++g) {
        render_group_editor(ctx, g);
    }
    ImGui::EndChild();

    // Add spacing between group editor and buttons
    ImGui::Dummy(ImVec2(0, button_spacing));

    // Position the randomizer buttons at the bottom
    render_randomize_controls(ctx);

    // Always apply changes live
    bool can_hot_apply = (m_editor.m_group_count == stats.groups);
    if (m_editor.m_dirty) {
        apply_rule_patch(ctx, can_hot_apply);
    }

    ImGui::End();
}

void ParticleEditorUI::refresh_editor_from_world(Context &ctx) {
    const auto &world = ctx.world_snapshot;
    const int group_count = world.get_groups_size();

    m_editor.m_group_count = group_count;
    m_editor.m_radius_squared.resize(group_count);
    m_editor.m_rules.resize(group_count * group_count);
    m_editor.m_sizes.resize(group_count);
    m_editor.m_colors.resize(group_count);
    m_editor.m_enabled.resize(group_count);

    for (int g = 0; g < group_count; ++g) {
        m_editor.m_radius_squared[g] = world.r2_of(g);
        m_editor.m_colors[g] = world.get_group_color(g);
        m_editor.m_sizes[g] = world.get_group_end(g) - world.get_group_start(g);
        m_editor.m_enabled[g] = world.is_group_enabled(g);

        const auto rowv = world.rules_of(g);
        if (!rowv.row) {
            for (int j = 0; j < group_count; ++j) {
                m_editor.m_rules[g * group_count + j] = 0.f;
            }
        } else {
            for (int j = 0; j < group_count; ++j) {
                m_editor.m_rules[g * group_count + j] = rowv.get(j);
            }
        }
    }
    m_editor.m_dirty = false;
    m_editor.m_should_refresh_next_frame = false;
}

void ParticleEditorUI::render_group_management_controls(Context &ctx) {
    auto &sim = ctx.sim;
    const auto &world = ctx.world_snapshot;
    const int defaultGroupSize = 1000;
    const float defaultR2 = 4096.f;

    if (ImGui::Button("Add Group")) {
        LOG_DEBUG("Add Group button pressed");
        static std::mt19937 rng{std::random_device{}()};
        std::uniform_real_distribution<float> color_dist(0.2f, 1.0f);
        Color random_color = {(unsigned char)(color_dist(rng) * 255),
                              (unsigned char)(color_dist(rng) * 255),
                              (unsigned char)(color_dist(rng) * 255), 255};

        auto backup_state = create_backup_state(ctx);
        auto undo_action = std::make_unique<AddGroupAction>(
            defaultGroupSize, random_color, defaultR2, world.get_groups_size());

        undo_action->set_apply_func(
            [&sim, random_color, defaultGroupSize, defaultR2]() {
                LOG_DEBUG("Undo action: Adding group with size " +
                          std::to_string(defaultGroupSize) + ", r2 " +
                          std::to_string(defaultR2));
                sim.push_command(mailbox::command::AddGroup{
                    defaultGroupSize, random_color, defaultR2});
                sim.force_stats_publish();
            });

        undo_action->set_unapply_func([&sim, backup_state]() {
            LOG_DEBUG("Redo action: Restoring world state from backup");
            sim.push_command(mailbox::command::SeedWorld{backup_state});
            sim.force_stats_publish();
        });

        ctx.undo.push(std::move(undo_action));
        sim.push_command(mailbox::command::AddGroup{defaultGroupSize,
                                                    random_color, defaultR2});
        sim.force_stats_publish();
    }

    ImGui::SameLine();
    if (ImGui::Button("Remove All Groups")) {
        LOG_DEBUG("Remove All Groups button pressed");
        auto backup_state = create_backup_state(ctx);
        auto undo_action = std::make_unique<ClearAllGroupsAction>(backup_state);
        undo_action->set_apply_func([&sim]() {
            LOG_DEBUG("Undo action: Removing all groups");
            sim.push_command(mailbox::command::RemoveAllGroups{});
            sim.force_stats_publish();
        });
        undo_action->set_unapply_func([&sim, backup_state]() {
            LOG_DEBUG("Redo action: Restoring world state from backup");
            sim.push_command(mailbox::command::SeedWorld{backup_state});
            sim.force_stats_publish();
        });

        ctx.undo.push(std::move(undo_action));
        sim.push_command(mailbox::command::RemoveAllGroups{});
        sim.force_stats_publish();
    }

    if (ImGui::Button("Disable All")) {
        LOG_DEBUG("Disable All Groups button pressed");
        // Create a rule patch to disable all groups
        mailbox::command::RulePatch patch;
        patch.groups = m_editor.m_group_count;
        patch.r2 = m_editor.m_radius_squared;
        patch.rules = m_editor.m_rules;
        patch.colors = m_editor.m_colors;
        patch.enabled = std::vector<bool>(m_editor.m_group_count, false);
        patch.hot = true;

        // Apply the patch
        sim.push_command(mailbox::command::ApplyRules{patch});
        sim.force_stats_publish();

        // Update local state
        for (int i = 0; i < m_editor.m_group_count; ++i) {
            m_editor.m_enabled[i] = false;
        }
        m_editor.m_dirty = true;
    }

    ImGui::SameLine();
    if (ImGui::Button("Enable All")) {
        LOG_DEBUG("Enable All Groups button pressed");
        // Create a rule patch to enable all groups
        mailbox::command::RulePatch patch;
        patch.groups = m_editor.m_group_count;
        patch.r2 = m_editor.m_radius_squared;
        patch.rules = m_editor.m_rules;
        patch.colors = m_editor.m_colors;
        patch.enabled = std::vector<bool>(m_editor.m_group_count, true);
        patch.hot = true;

        // Apply the patch
        sim.push_command(mailbox::command::ApplyRules{patch});
        sim.force_stats_publish();

        // Update local state
        for (int i = 0; i < m_editor.m_group_count; ++i) {
            m_editor.m_enabled[i] = true;
        }
        m_editor.m_dirty = true;
    }
}

void ParticleEditorUI::render_group_editor(Context &ctx, int group_index) {
    ImGui::PushID(group_index);
    ImGui::SeparatorText(
        (std::string("Group ") + std::to_string(group_index)).c_str());

    ImGui::BeginGroup();
    if (ImGui::Button("Remove")) {
        LOG_DEBUG("Remove group " + std::to_string(group_index) +
                  " button pressed");
        auto backup_state = create_backup_state(ctx);
        auto undo_action =
            std::make_unique<RemoveGroupAction>(group_index, backup_state);
        undo_action->set_apply_func([&sim = ctx.sim, group_index]() {
            LOG_DEBUG("Undo action: Removing group " +
                      std::to_string(group_index));
            sim.push_command(mailbox::command::RemoveGroup{group_index});
            sim.force_stats_publish();
        });
        undo_action->set_unapply_func([&sim = ctx.sim, backup_state]() {
            LOG_DEBUG("Redo action: Restoring world state from backup");
            sim.push_command(mailbox::command::SeedWorld{backup_state});
            sim.force_stats_publish();
        });

        ctx.undo.push(std::move(undo_action));
        ctx.sim.push_command(mailbox::command::RemoveGroup{group_index});
        ctx.sim.force_stats_publish();
    }

    ImGui::SameLine();
    int new_size = m_editor.m_sizes[group_index];
    if (ImGui::InputInt("Size", &new_size, 100, 1000)) {
        new_size = std::max(0, new_size);
        if (new_size != m_editor.m_sizes[group_index]) {
            LOG_DEBUG("Group " + std::to_string(group_index) + " size change:");
            LOG_DEBUG("  - Local editor: " +
                      std::to_string(m_editor.m_sizes[group_index]) + " -> " +
                      std::to_string(new_size));

            int old_size = m_editor.m_sizes[group_index];
            auto backup_state = create_backup_state(ctx);
            auto undo_action = std::make_unique<ResizeGroupAction>(
                group_index, old_size, new_size);
            undo_action->set_apply_func(
                [&sim = ctx.sim, group_index, new_size]() {
                    LOG_DEBUG("Undo action: Resizing group " +
                              std::to_string(group_index) + " to size " +
                              std::to_string(new_size));
                    sim.push_command(
                        mailbox::command::ResizeGroup{group_index, new_size});
                    sim.force_stats_publish();
                });
            undo_action->set_unapply_func([&sim = ctx.sim, backup_state]() {
                LOG_DEBUG("Redo action: Restoring world state from backup");
                sim.push_command(mailbox::command::SeedWorld{backup_state});
                sim.force_stats_publish();
            });

            ctx.undo.push(std::move(undo_action));

            // Update local state immediately
            m_editor.m_sizes[group_index] = new_size;
            m_editor.m_dirty = true;

            // Always apply immediately
            ctx.sim.push_command(
                mailbox::command::ResizeGroup{group_index, new_size});
            ctx.sim.force_stats_publish();
            m_editor.m_should_refresh_next_frame = true;

            LOG_DEBUG("  - Updated local editor to: " +
                      std::to_string(m_editor.m_sizes[group_index]));
        }
    }
    ImGui::EndGroup();

    render_group_properties(ctx, group_index);
    render_group_rules(ctx, group_index);

    ImGui::PopID();
}

void ParticleEditorUI::render_group_properties(Context &ctx, int group_index) {
    render_group_enabled_checkbox(ctx, group_index);
    render_group_color_picker(ctx, group_index);
    render_group_radius_slider(ctx, group_index);
}

void ParticleEditorUI::render_single_group_rule(Context &ctx, int group_index,
                                                int target_index) {
    // Create unique ID by combining source and target indices
    int unique_id = group_index * 1000 + target_index;
    ImGui::PushID(unique_id);

    auto to_imvec4 = [](Color c) -> ImVec4 {
        return ImVec4(c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f);
    };

    ImVec4 csrc = to_imvec4(m_editor.m_colors[group_index]);
    ImVec4 cdst = to_imvec4(m_editor.m_colors[target_index]);
    ImGui::ColorButton("src", csrc,
                       ImGuiColorEditFlags_NoTooltip |
                           ImGuiColorEditFlags_NoPicker |
                           ImGuiColorEditFlags_NoDragDrop,
                       ImVec2(12, 12));
    ImGui::SameLine();
    ImGui::Text("g%d  \xE2\x86\x92  g%d", group_index, target_index);
    ImGui::SameLine();
    ImGui::ColorButton("dst", cdst,
                       ImGuiColorEditFlags_NoTooltip |
                           ImGuiColorEditFlags_NoPicker |
                           ImGuiColorEditFlags_NoDragDrop,
                       ImVec2(12, 12));

    float &v =
        m_editor.m_rules[group_index * m_editor.m_group_count + target_index];
    float before_v = v;

    ImGui::SameLine();
    if (ImGui::SliderFloat("", &v, -3.14f, 3.14f, "%.3f")) {
        LOG_DEBUG("Rule strength g" + std::to_string(group_index) + "->g" +
                  std::to_string(target_index) + " changed from " +
                  std::to_string(before_v) + " to " + std::to_string(v));
        float before = before_v;
        float after = v;
        ImGuiID id = ImGui::GetItemID();
        if (ImGui::IsItemActivated()) {
            ctx.undo.beginInteraction(id);
        }
        const int gi = group_index;
        const int gj = target_index;
        auto &sim_ref = ctx.sim; // Capture the simulation reference directly
        ctx.undo.push(std::unique_ptr<IAction>(new ValueAction<float>(
            (std::string("m_editor.rule.") + std::to_string(gi) + "." +
             std::to_string(gj))
                .c_str(),
            "Rule strength",
            []() {
                return 0.f;
            },
            [&sim_ref, gi, gj, this](const float &val) {
                LOG_DEBUG("Undo/Redo action: Setting rule strength g" +
                          std::to_string(gi) + "->g" + std::to_string(gj) +
                          " to " + std::to_string(val));
                m_editor.m_rules[gi * m_editor.m_group_count + gj] = val;
                m_editor.m_dirty = true;

                // Always apply live
                mailbox::command::RulePatch patch;
                patch.groups = m_editor.m_group_count;
                patch.r2 = m_editor.m_radius_squared;
                patch.rules = m_editor.m_rules;
                patch.colors = m_editor.m_colors;
                patch.enabled = m_editor.m_enabled;
                patch.hot = true;
                sim_ref.push_command(mailbox::command::ApplyRules{patch});
                m_editor.m_should_refresh_next_frame = true;
            },
            before, after)));
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            ctx.undo.endInteraction(id);
        }
        m_editor.m_dirty = true;
    }
    ImGui::PopID();
}

void ParticleEditorUI::render_group_rules(Context &ctx, int group_index) {
    if (ImGui::TreeNode("Rules Row")) {
        render_single_group_rule(ctx, group_index, group_index);
        ImGui::Separator();

        for (int j = 0; j < m_editor.m_group_count; ++j) {
            if (j != group_index) {
                render_single_group_rule(ctx, group_index, j);
                render_single_group_rule(ctx, j, group_index);
                ImGui::Separator();
            }
        }
        ImGui::TreePop();
    }
}

void ParticleEditorUI::render_randomize_controls(Context &ctx) {
    if (ImGui::Button("Make symmetric (w_ij = w_ji)")) {
        LOG_DEBUG("Make symmetric button pressed");
        for (int i = 0; i < m_editor.m_group_count; ++i) {
            for (int j = i + 1; j < m_editor.m_group_count; ++j) {
                float m =
                    0.5f * (m_editor.m_rules[i * m_editor.m_group_count + j] +
                            m_editor.m_rules[j * m_editor.m_group_count + i]);
                m_editor.m_rules[i * m_editor.m_group_count + j] =
                    m_editor.m_rules[j * m_editor.m_group_count + i] = m;
            }
        }
        m_editor.m_dirty = true;
    }

    ImGui::SameLine();
    if (ImGui::Button("Zero self (w_ii = 0)")) {
        LOG_DEBUG("Zero self button pressed");
        for (int i = 0; i < m_editor.m_group_count; ++i) {
            m_editor.m_rules[i * m_editor.m_group_count + i] = 0.f;
        }
        m_editor.m_dirty = true;
    }

    if (ImGui::Button("Randomize rules")) {
        LOG_DEBUG("Randomize rules button pressed");
        static std::mt19937 rng{std::random_device{}()};
        std::uniform_real_distribution<float> dist(-3.14f, 3.14f);
        for (int i = 0; i < m_editor.m_group_count; ++i) {
            for (int j = 0; j < m_editor.m_group_count; ++j) {
                m_editor.m_rules[i * m_editor.m_group_count + j] = dist(rng);
            }
        }
        m_editor.m_dirty = true;
    }

    ImGui::SameLine();
    if (ImGui::Button("Smart Randomize")) {
        LOG_DEBUG("Smart Randomize button pressed");
        SmartRandomizer randomizer;
        m_editor.m_rules = randomizer.generate_rules(
            m_editor.m_colors, m_editor.m_sizes, m_editor.m_group_count);
        m_editor.m_dirty = true;
    }
}

mailbox::command::SeedSpec ParticleEditorUI::create_backup_state(Context &ctx) {
    const auto &world = ctx.world_snapshot;
    mailbox::command::SeedSpec backup_state;
    const int G = world.get_groups_size();

    backup_state.sizes.resize(G);
    backup_state.colors.resize(G);
    backup_state.r2.resize(G);
    backup_state.enabled.resize(G);
    backup_state.rules.resize(G * G);

    for (int g = 0; g < G; ++g) {
        backup_state.sizes[g] =
            world.get_group_end(g) - world.get_group_start(g);
        backup_state.colors[g] = world.get_group_color(g);
        backup_state.r2[g] = world.r2_of(g);
        backup_state.enabled[g] = world.is_group_enabled(g);
        const auto rowv = world.rules_of(g);
        for (int j = 0; j < G; ++j) {
            backup_state.rules[g * G + j] = rowv.get(j);
        }
    }

    return backup_state;
}

void ParticleEditorUI::apply_rule_patch(Context &ctx, bool hot) {
    auto &sim = ctx.sim;

    // Apply the rule patch
    mailbox::command::RulePatch patch;
    patch.groups = m_editor.m_group_count;
    patch.r2 = m_editor.m_radius_squared;
    patch.rules = m_editor.m_rules;
    patch.colors = m_editor.m_colors;
    patch.enabled = m_editor.m_enabled;
    patch.hot = hot;
    sim.push_command(mailbox::command::ApplyRules{patch});

    // Clear dirty flag and mark for refresh on next frame
    m_editor.m_dirty = false;
    m_editor.m_should_refresh_next_frame = true;
}

void ParticleEditorUI::render_group_enabled_checkbox(Context &ctx,
                                                     int group_index) {
    bool enabled = m_editor.m_enabled[group_index];
    if (ImGui::Checkbox("Enabled", &enabled)) {
        LOG_DEBUG("Group " + std::to_string(group_index) +
                  " enabled changed from " +
                  (m_editor.m_enabled[group_index] ? "true" : "false") +
                  " to " + (enabled ? "true" : "false"));
        bool before = m_editor.m_enabled[group_index];
        m_editor.m_enabled[group_index] = enabled;
        bool after = m_editor.m_enabled[group_index];
        ImGuiID id = ImGui::GetItemID();
        if (ImGui::IsItemActivated()) {
            ctx.undo.beginInteraction(id);
        }

        const int gi = group_index;
        auto &sim_ref = ctx.sim; // Capture the simulation reference directly
        ctx.undo.push(std::unique_ptr<IAction>(new ValueAction<bool>(
            (std::string("m_editor.enabled.") + std::to_string(gi)).c_str(),
            "Group enabled",
            []() {
                return false;
            },
            [&sim_ref, gi, this](const bool &e) {
                LOG_DEBUG("Undo/Redo action: Setting group " +
                          std::to_string(gi) + " enabled to " +
                          (e ? "true" : "false"));
                m_editor.m_enabled[gi] = e;
                m_editor.m_dirty = true;

                // Always apply live
                mailbox::command::RulePatch patch;
                patch.groups = m_editor.m_group_count;
                patch.r2 = m_editor.m_radius_squared;
                patch.rules = m_editor.m_rules;
                patch.colors = m_editor.m_colors;
                patch.enabled = m_editor.m_enabled;
                patch.hot = true;
                sim_ref.push_command(mailbox::command::ApplyRules{patch});
                m_editor.m_should_refresh_next_frame = true;
            },
            before, after)));
        if (ImGui::IsItemDeactivatedAfterEdit())
            ctx.undo.endInteraction(id);
        m_editor.m_dirty = true;
    }
}

void ParticleEditorUI::render_group_color_picker(Context &ctx,
                                                 int group_index) {
    float col[4] = {m_editor.m_colors[group_index].r / 255.f,
                    m_editor.m_colors[group_index].g / 255.f,
                    m_editor.m_colors[group_index].b / 255.f,
                    m_editor.m_colors[group_index].a / 255.f};
    if (ImGui::ColorEdit4("Color", col, ImGuiColorEditFlags_NoInputs)) {
        LOG_DEBUG("Group " + std::to_string(group_index) +
                  " color changed from (" +
                  std::to_string(m_editor.m_colors[group_index].r) + "," +
                  std::to_string(m_editor.m_colors[group_index].g) + "," +
                  std::to_string(m_editor.m_colors[group_index].b) + "," +
                  std::to_string(m_editor.m_colors[group_index].a) + ") to (" +
                  std::to_string(
                      (unsigned char)std::clamp(int(col[0] * 255.f), 0, 255)) +
                  "," +
                  std::to_string(
                      (unsigned char)std::clamp(int(col[1] * 255.f), 0, 255)) +
                  "," +
                  std::to_string(
                      (unsigned char)std::clamp(int(col[2] * 255.f), 0, 255)) +
                  "," +
                  std::to_string(
                      (unsigned char)std::clamp(int(col[3] * 255.f), 0, 255)) +
                  ")");
        Color before = m_editor.m_colors[group_index];
        m_editor.m_colors[group_index] =
            Color{(unsigned char)std::clamp(int(col[0] * 255.f), 0, 255),
                  (unsigned char)std::clamp(int(col[1] * 255.f), 0, 255),
                  (unsigned char)std::clamp(int(col[2] * 255.f), 0, 255),
                  (unsigned char)std::clamp(int(col[3] * 255.f), 0, 255)};
        Color after = m_editor.m_colors[group_index];
        ImGuiID id = ImGui::GetItemID();
        if (ImGui::IsItemActivated())
            ctx.undo.beginInteraction(id);
        const int gi = group_index;
        auto &sim_ref = ctx.sim; // Capture the simulation reference directly
        ctx.undo.push(std::unique_ptr<IAction>(new ValueAction<Color>(
            (std::string("m_editor.color.") + std::to_string(gi)).c_str(),
            "Group color",
            []() {
                return Color{};
            },
            [&sim_ref, gi, this](const Color &c) {
                LOG_DEBUG("Undo/Redo action: Setting group " +
                          std::to_string(gi) + " color to (" +
                          std::to_string(c.r) + "," + std::to_string(c.g) +
                          "," + std::to_string(c.b) + "," +
                          std::to_string(c.a) + ")");
                m_editor.m_colors[gi] = c;
                m_editor.m_dirty = true;

                // Always apply live
                mailbox::command::RulePatch patch;
                patch.groups = m_editor.m_group_count;
                patch.r2 = m_editor.m_radius_squared;
                patch.rules = m_editor.m_rules;
                patch.colors = m_editor.m_colors;
                patch.enabled = m_editor.m_enabled;
                patch.hot = true;
                sim_ref.push_command(mailbox::command::ApplyRules{patch});
                m_editor.m_should_refresh_next_frame = true;
            },
            before, after)));
        if (ImGui::IsItemDeactivatedAfterEdit())
            ctx.undo.endInteraction(id);
        m_editor.m_dirty = true;
    }
}

void ParticleEditorUI::render_group_radius_slider(Context &ctx,
                                                  int group_index) {
    float r = std::sqrt(std::max(0.f, m_editor.m_radius_squared[group_index]));
    if (ImGui::SliderFloat("Radius (r)", &r, 0.f, 300.f, "%.1f")) {
        LOG_DEBUG("Group " + std::to_string(group_index) +
                  " radius changed from " +
                  std::to_string(std::sqrt(
                      std::max(0.f, m_editor.m_radius_squared[group_index]))) +
                  " to " + std::to_string(r));
        float before = m_editor.m_radius_squared[group_index];
        m_editor.m_radius_squared[group_index] = r * r;
        float after = m_editor.m_radius_squared[group_index];
        ImGuiID id = ImGui::GetItemID();
        if (ImGui::IsItemActivated())
            ctx.undo.beginInteraction(id);
        const int gi = group_index;
        auto &sim_ref = ctx.sim; // Capture the simulation reference directly
        ctx.undo.push(std::unique_ptr<IAction>(new ValueAction<float>(
            (std::string("m_editor.r2.") + std::to_string(gi)).c_str(),
            "Radius^2",
            []() {
                return 0.f;
            },
            [&sim_ref, gi, this](const float &v) {
                LOG_DEBUG("Undo/Redo action: Setting group " +
                          std::to_string(gi) + " radius^2 to " +
                          std::to_string(v) +
                          " (radius = " + std::to_string(std::sqrt(v)) + ")");
                m_editor.m_radius_squared[gi] = v;
                m_editor.m_dirty = true;

                // Always apply live
                mailbox::command::RulePatch patch;
                patch.groups = m_editor.m_group_count;
                patch.r2 = m_editor.m_radius_squared;
                patch.rules = m_editor.m_rules;
                patch.colors = m_editor.m_colors;
                patch.enabled = m_editor.m_enabled;
                patch.hot = true;
                sim_ref.push_command(mailbox::command::ApplyRules{patch});
                m_editor.m_should_refresh_next_frame = true;
            },
            before, after)));
        if (ImGui::IsItemDeactivatedAfterEdit())
            ctx.undo.endInteraction(id);
        m_editor.m_dirty = true;
    }
}