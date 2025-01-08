#ifndef __UI_HPP
#define __UI_HPP

#include <imgui.h>
#include <mutex>
#include <raylib.h>
#include <thread>

#include "../mailbox/mailbox.hpp"
#include "../simulation/multicore.hpp"
#include "../simulation/world.hpp"
#include "../types.hpp"

void render_ui(const WindowConfig &wcfg, World &world,
               mailbox::SimulationConfig &scfgb,
               mailbox::SimulationStats &statsb,
               mailbox::command::Queue &cmdq) {

    mailbox::SimulationConfig::Snapshot scfg = scfgb.acquire();
    mailbox::SimulationStats::Snapshot stats = statsb.acquire();

    bool scfg_updated = false;
    auto check_scfg_update = [&scfg_updated](bool s) {
        if (s) {
            scfg_updated = true;
        }
    };

    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.);
    ImGui::Begin("main", NULL,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDecoration |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoTitleBar);
    ImGui::SetWindowPos(ImVec2{0.f, 0.f}, ImGuiCond_Always);
    ImGui::SetWindowSize(
        ImVec2{(float)wcfg.panel_width, (float)wcfg.screen_height},
        ImGuiCond_Always);

    {

        ImGui::SeparatorText("Stats");
        { /// MARK: STATS
            ImGui::Text("FPS: %d", GetFPS());
            ImGui::SameLine();
            ImGui::Text("TPS: %d", stats.effective_tps);
            ImGui::Text("Last step: %.3f ms", stats.last_step_ns / 1e6);
            ImGui::Text("Particles: %d  Groups: %d  Threads: %d",
                        stats.particles, stats.groups, stats.sim_threads);
            ImGui::Text("Sim Bounds: %.0f x %.0f", scfg.bounds_width,
                        scfg.bounds_height);

            ImGui::SeparatorText("Controls");
            if (ImGui::Button("Reset world")) {
                cmdq.push({mailbox::command::Command::Kind::ResetWorld});
            }
            ImGui::SameLine();
            if (ImGui::Button("Quit sim")) {
                cmdq.push({mailbox::command::Command::Kind::Quit});
            }
        }

        ImGui::SeparatorText("Sim Config");
        { /// MARK: SIM CONFIG
            check_scfg_update(ImGui::SliderInt("Target TPS", &scfg.target_tps,
                                               0, 240, "%d",
                                               ImGuiSliderFlags_AlwaysClamp));
            check_scfg_update(
                ImGui::Checkbox("Interpolate", &scfg.interpolate));
            if (scfg.interpolate) {
                check_scfg_update(ImGui::SliderFloat("Interp delay (ms)",
                                                     &scfg.interp_delay_ms,
                                                     0.0f, 50.0f, "%.1f"));
            }
            check_scfg_update(ImGui::SliderFloat("Time Scale", &scfg.time_scale,
                                                 0.01f, 2.0f, "%.3f",
                                                 ImGuiSliderFlags_Logarithmic));
            check_scfg_update(ImGui::SliderFloat("Viscosity", &scfg.viscosity,
                                                 0.0f, 1.0f, "%.3f"));
            check_scfg_update(ImGui::SliderFloat(
                "Wall Repel (px)", &scfg.wallRepel, 0.0f, 200.0f, "%.1f"));
            check_scfg_update(ImGui::SliderFloat(
                "Wall Strength", &scfg.wallStrength, 0.0f, 1.0f, "%.3f"));
        }

        ImGui::SeparatorText("Parallelism");
        { /// MARK: MULTICORE
            unsigned hc = std::thread::hardware_concurrency();
            int max_threads =
                std::max(1, (int)hc - 2); // keep 2 free: render + OS

            ImGui::Text("HW threads: %u", hc ? hc : 1);
            bool auto_mode = (scfg.sim_threads <= 0);
            if (ImGui::Checkbox("Auto (HW-2)", &auto_mode)) {
                scfg.sim_threads = auto_mode ? -1 : std::min(1, max_threads);
                check_scfg_update(true);
            }
            if (!auto_mode) {
                check_scfg_update(ImGui::SliderInt(
                    "Sim threads", &scfg.sim_threads, 1, max_threads, "%d",
                    ImGuiSliderFlags_AlwaysClamp));
            } else {
                ImGui::BeginDisabled();
                int auto_val = std::max(1, (int)compute_sim_threads());
                ImGui::SliderInt("Sim threads", &auto_val, 1, max_threads,
                                 "%d");
                ImGui::EndDisabled();
            }
        }

        ImGui::SeparatorText("Groups & Rules");
        { /// MARK: PARTICLE UPDATE

            struct EditorState {
                int G = 0;
                std::vector<float> r2;    // size G
                std::vector<float> rules; // size G*G row-major
                std::vector<int> sizes;   // per-group particle counts (for add)
                std::vector<Color> colors; // per-group color
                bool dirty = false;
            };
            static EditorState editor;

            auto refresh_from_world = [&]() {
                const int G = world.get_groups_size();
                editor.G = G;
                editor.r2.resize(G);
                editor.rules.resize(G * G);
                editor.sizes.resize(G);
                editor.colors.resize(G);

                for (int g = 0; g < G; ++g) {
                    editor.r2[g] = world.r2_of(g);
                    editor.colors[g] = *world.get_group_color(g);
                    editor.sizes[g] =
                        world.get_group_end(g) - world.get_group_start(g);

                    const float *row = world.rules_row(g);
                    if (!row) {
                        // transient: rules not ready yet
                        for (int j = 0; j < G; ++j)
                            editor.rules[g * G + j] = 0.f;
                    } else {
                        for (int j = 0; j < G; ++j)
                            editor.rules[g * G + j] = row[j];
                    }
                }
                editor.dirty = false;
            };

            // Initialize or resync when sim-side group count changed
            static int last_seen_groups = -1;
            if (last_seen_groups != stats.groups) {
                refresh_from_world();
                last_seen_groups = stats.groups;
            }

            // Scrollable child so the whole editor can be long
            ImGui::BeginChild("GroupsRulesChild", ImVec2(0, 260), true,
                              ImGuiWindowFlags_AlwaysVerticalScrollbar);

            // per-group controls
            for (int g = 0; g < editor.G; ++g) {
                ImGui::PushID(g);
                ImGui::SeparatorText(
                    (std::string("Group ") + std::to_string(g)).c_str());

                // color
                float col[4] = {
                    editor.colors[g].r / 255.f, editor.colors[g].g / 255.f,
                    editor.colors[g].b / 255.f, editor.colors[g].a / 255.f};
                if (ImGui::ColorEdit4("Color", col,
                                      ImGuiColorEditFlags_NoInputs)) {
                    editor.colors[g] = Color{
                        (unsigned char)std::clamp(int(col[0] * 255.f), 0, 255),
                        (unsigned char)std::clamp(int(col[1] * 255.f), 0, 255),
                        (unsigned char)std::clamp(int(col[2] * 255.f), 0, 255),
                        (unsigned char)std::clamp(int(col[3] * 255.f), 0, 255)};
                    editor.dirty = true;
                }

                // size (display-only unless you implement hot resize)
                int sz = editor.sizes[g];
                ImGui::InputInt("Size (info)", &sz, 0, 0,
                                ImGuiInputTextFlags_ReadOnly);

                // interaction radius^2
                float r = std::sqrt(std::max(0.f, editor.r2[g]));
                if (ImGui::SliderFloat("Radius (r)", &r, 0.f, 300.f, "%.1f")) {
                    editor.r2[g] = r * r;
                    editor.dirty = true;
                }

                // row of rule sliders (g affects j)
                if (ImGui::TreeNode("Rules Row")) {
                    for (int j = 0; j < editor.G; ++j) {
                        ImGui::PushID(j);
                        float &v = editor.rules[g * editor.G + j];
                        if (ImGui::SliderFloat("##w", &v, -3.14f, 3.14f)) {
                            editor.dirty = true;
                        }
                        ImGui::SameLine();
                        ImGui::Text("to %d", j);
                        ImGui::PopID();
                    }
                    ImGui::TreePop();
                }

                // remove button (forces reseed on sim side)
                if (ImGui::Button("Remove Group")) {
                    auto cmd = mailbox::command::Command{};
                    cmd.kind = mailbox::command::Command::Kind::RemoveGroup;
                    cmd.rem_group =
                        std::make_shared<mailbox::command::RemoveGroupCmd>(
                            mailbox::command::RemoveGroupCmd{g});
                    cmdq.push(cmd);
                }

                ImGui::PopID();
            }

            ImGui::Separator();

            // Add group (appends; positions for new group are randomized in
            // sim)
            static int new_size = 500;
            static float new_r = 80.f;
            static float new_col[4] = {0.8f, 0.8f, 0.2f, 1.f};
            ImGui::InputInt("New group size", &new_size);
            ImGui::SliderFloat("New group radius r", &new_r, 1.f, 300.f,
                               "%.1f");
            ImGui::ColorEdit4("New group color", new_col,
                              ImGuiColorEditFlags_NoInputs);
            if (ImGui::Button("Add Group")) {
                auto cmd = mailbox::command::Command{};
                cmd.kind = mailbox::command::Command::Kind::AddGroup;
                auto add = std::make_shared<mailbox::command::AddGroupCmd>();
                add->size = std::max(0, new_size);
                add->r2 = new_r * new_r;
                add->color = Color{
                    (unsigned char)std::clamp(int(new_col[0] * 255.f), 0, 255),
                    (unsigned char)std::clamp(int(new_col[1] * 255.f), 0, 255),
                    (unsigned char)std::clamp(int(new_col[2] * 255.f), 0, 255),
                    (unsigned char)std::clamp(int(new_col[3] * 255.f), 0, 255)};
                cmd.add_group = add;
                cmdq.push(cmd);
            }

            ImGui::EndChild(); // end scrollable

            // MARK: Apply buttons
            bool can_hot_apply = (editor.G == stats.groups);
            if (!can_hot_apply)
                ImGui::BeginDisabled();

            if (ImGui::Button("Apply (hot, no reseed)")) {
                auto patch = std::make_shared<mailbox::command::RulePatch>();
                patch->groups = editor.G;
                patch->r2 = editor.r2;
                patch->rules = editor.rules;
                patch->hot = true;

                auto cmd = mailbox::command::Command{};
                cmd.kind = mailbox::command::Command::Kind::ApplyRules;
                cmd.rules = patch;
                cmdq.push(cmd);
                editor.dirty = false;
            }
            if (!can_hot_apply) {
                ImGui::EndDisabled();
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip(
                        "Group count/order changed. Hot apply disabled.");
            }
            ImGui::SameLine();
            if (ImGui::Button("Apply & Reseed")) {
                auto patch = std::make_shared<mailbox::command::RulePatch>();
                patch->groups = editor.G;
                patch->r2 = editor.r2;
                patch->rules = editor.rules;
                patch->hot = false;

                auto cmd = mailbox::command::Command{};
                cmd.kind = mailbox::command::Command::Kind::ApplyRules;
                cmd.rules = patch;
                cmdq.push(cmd);
                editor.dirty = false;
            }

            // TODO: world.set_group_color
            // for (int g = 0; g < editor.G && g < world.get_groups_size(); ++g)
            // {
            //     world.set_group_color(
            //         g, editor.colors[g]); // add this setter if not present
            // }

            // Quality-of-life helpers
            if (ImGui::Button("Make symmetric (w_ij = w_ji)")) {
                for (int i = 0; i < editor.G; ++i)
                    for (int j = i + 1; j < editor.G; ++j) {
                        float m = 0.5f * (editor.rules[i * editor.G + j] +
                                          editor.rules[j * editor.G + i]);
                        editor.rules[i * editor.G + j] =
                            editor.rules[j * editor.G + i] = m;
                    }
                editor.dirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Zero self (w_ii = 0)")) {
                for (int i = 0; i < editor.G; ++i)
                    editor.rules[i * editor.G + i] = 0.f;
                editor.dirty = true;
            }
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();

    if (scfg_updated) {
        scfgb.publish(scfg);
    }
}

#endif