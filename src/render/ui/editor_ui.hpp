#pragma once

#include <algorithm>
#include <cmath>
#include <imgui.h>
#include <random>
#include <raylib.h>
#include <string>
#include <vector>

#include "../../undo.hpp"
#include "../../window_config.hpp"
#include "../renderer.hpp"

class EditorUI : public IRenderer {
  public:
    EditorUI() = default;
    ~EditorUI() override = default;

    void render(Context &ctx) override {
        if (!ctx.rcfg.show_ui || !ctx.rcfg.show_editor)
            return;
        render_ui(ctx);
    }

  private:
    void render_ui(Context &ctx) {
        auto &sim = ctx.sim;
        mailbox::SimulationStats::Snapshot stats = sim.get_stats();
        const World &world = sim.get_world();

        auto to_imvec4 = [](Color c) -> ImVec4 {
            return ImVec4(c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f);
        };

        ImGui::Begin("Particle & Rule Editor", &ctx.rcfg.show_editor);
        ImGui::SetWindowSize(ImVec2{600, 700}, ImGuiCond_FirstUseEver);

        // Editor state for groups and rules
        struct EditorState {
            int G = 0;
            std::vector<float> r2;
            std::vector<float> rules;
            std::vector<int> sizes;
            std::vector<Color> colors;
            std::vector<bool> enabled;
            bool live_apply = false;
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
            editor.enabled.resize(G);
            for (int g = 0; g < G; ++g) {
                editor.r2[g] = world.r2_of(g);
                editor.colors[g] = world.get_group_color(g);
                editor.sizes[g] =
                    world.get_group_end(g) - world.get_group_start(g);
                editor.enabled[g] = world.is_group_enabled(g);
                const auto rowv = world.rules_of(g);
                if (!rowv.row) {
                    for (int j = 0; j < G; ++j)
                        editor.rules[g * G + j] = 0.f;
                } else {
                    for (int j = 0; j < G; ++j)
                        editor.rules[g * G + j] = rowv.get(j);
                }
            }
            editor.dirty = false;
        };

        static int last_seen_groups = -1;
        if (last_seen_groups != stats.groups) {
            refresh_from_world();
            last_seen_groups = stats.groups;
        }

        ImGui::Text("Groups: %d", stats.groups);
        ImGui::Separator();
        ImGui::Checkbox("Live apply", &editor.live_apply);

        ImGui::BeginChild("GroupsRulesChild", ImVec2(0, 400), true,
                          ImGuiWindowFlags_AlwaysVerticalScrollbar);
        for (int g = 0; g < editor.G; ++g) {
            ImGui::PushID(g);
            ImGui::SeparatorText(
                (std::string("Group ") + std::to_string(g)).c_str());

            // Enable/disable checkbox
            bool enabled = editor.enabled[g];
            if (ImGui::Checkbox("Enabled", &enabled)) {
                bool before = editor.enabled[g];
                editor.enabled[g] = enabled;
                bool after = editor.enabled[g];
                ImGuiID id = ImGui::GetItemID();
                if (ImGui::IsItemActivated())
                    ctx.undo.beginInteraction(id);
                const int gi = g;
                ctx.undo.push(std::unique_ptr<IAction>(new ValueAction<bool>(
                    (std::string("editor.enabled.") + std::to_string(gi))
                        .c_str(),
                    "Group enabled",
                    []() {
                        return false;
                    },
                    [&, gi](const bool &e) {
                        editor.enabled[gi] = e;
                        editor.dirty = true;
                    },
                    before, after)));
                if (ImGui::IsItemDeactivatedAfterEdit())
                    ctx.undo.endInteraction(id);
                editor.dirty = true;
            }

            float col[4] = {
                editor.colors[g].r / 255.f, editor.colors[g].g / 255.f,
                editor.colors[g].b / 255.f, editor.colors[g].a / 255.f};
            if (ImGui::ColorEdit4("Color", col, ImGuiColorEditFlags_NoInputs)) {
                Color before = editor.colors[g];
                editor.colors[g] = Color{
                    (unsigned char)std::clamp(int(col[0] * 255.f), 0, 255),
                    (unsigned char)std::clamp(int(col[1] * 255.f), 0, 255),
                    (unsigned char)std::clamp(int(col[2] * 255.f), 0, 255),
                    (unsigned char)std::clamp(int(col[3] * 255.f), 0, 255)};
                Color after = editor.colors[g];
                ImGuiID id = ImGui::GetItemID();
                if (ImGui::IsItemActivated())
                    ctx.undo.beginInteraction(id);
                const int gi = g;
                ctx.undo.push(std::unique_ptr<IAction>(new ValueAction<Color>(
                    (std::string("editor.color.") + std::to_string(gi)).c_str(),
                    "Group color",
                    []() {
                        return Color{};
                    },
                    [&, gi](const Color &c) {
                        editor.colors[gi] = c;
                        editor.dirty = true;
                    },
                    before, after)));
                if (ImGui::IsItemDeactivatedAfterEdit())
                    ctx.undo.endInteraction(id);
                editor.dirty = true;
            }
            int sz = editor.sizes[g];
            ImGui::InputInt("Size (info)", &sz, 0, 0,
                            ImGuiInputTextFlags_ReadOnly);
            float r = std::sqrt(std::max(0.f, editor.r2[g]));
            if (ImGui::SliderFloat("Radius (r)", &r, 0.f, 300.f, "%.1f")) {
                float before = editor.r2[g];
                editor.r2[g] = r * r;
                float after = editor.r2[g];
                ImGuiID id = ImGui::GetItemID();
                if (ImGui::IsItemActivated())
                    ctx.undo.beginInteraction(id);
                const int gi = g;
                ctx.undo.push(std::unique_ptr<IAction>(new ValueAction<float>(
                    (std::string("editor.r2.") + std::to_string(gi)).c_str(),
                    "Radius^2",
                    []() {
                        return 0.f;
                    },
                    [&, gi](const float &v) {
                        editor.r2[gi] = v;
                        editor.dirty = true;
                    },
                    before, after)));
                if (ImGui::IsItemDeactivatedAfterEdit())
                    ctx.undo.endInteraction(id);
                editor.dirty = true;
            }
            if (ImGui::TreeNode("Rules Row")) {
                for (int j = 0; j < editor.G; ++j) {
                    ImGui::PushID(j);
                    ImVec4 csrc = to_imvec4(editor.colors[g]);
                    ImVec4 cdst = to_imvec4(editor.colors[j]);
                    ImGui::ColorButton("src", csrc,
                                       ImGuiColorEditFlags_NoTooltip |
                                           ImGuiColorEditFlags_NoPicker |
                                           ImGuiColorEditFlags_NoDragDrop,
                                       ImVec2(14, 14));
                    ImGui::SameLine(0.0f, 6.0f);
                    ImGui::Text("g%d  \xE2\x86\x92  g%d", g, j);
                    ImGui::SameLine(0.0f, 6.0f);
                    ImGui::ColorButton("dst", cdst,
                                       ImGuiColorEditFlags_NoTooltip |
                                           ImGuiColorEditFlags_NoPicker |
                                           ImGuiColorEditFlags_NoDragDrop,
                                       ImVec2(14, 14));
                    float &v = editor.rules[g * editor.G + j];
                    float before_v = v;
                    if (ImGui::SliderFloat("Strength", &v, -3.14f, 3.14f,
                                           "%.3f")) {
                        float before = before_v;
                        float after = v;
                        ImGuiID id = ImGui::GetItemID();
                        if (ImGui::IsItemActivated())
                            ctx.undo.beginInteraction(id);
                        const int gi = g;
                        const int gj = j;
                        ctx.undo.push(
                            std::unique_ptr<IAction>(new ValueAction<float>(
                                (std::string("editor.rule.") +
                                 std::to_string(gi) + "." + std::to_string(gj))
                                    .c_str(),
                                "Rule strength",
                                []() {
                                    return 0.f;
                                },
                                [&, gi, gj](const float &val) {
                                    editor.rules[gi * editor.G + gj] = val;
                                    editor.dirty = true;
                                },
                                before, after)));
                        if (ImGui::IsItemDeactivatedAfterEdit())
                            ctx.undo.endInteraction(id);
                        editor.dirty = true;
                    }
                    ImGui::Separator();
                    ImGui::PopID();
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        ImGui::EndChild();

        // Apply buttons
        bool can_hot_apply = (editor.G == stats.groups);
        auto send_patch = [&](bool hot) {
            auto patch = std::make_shared<mailbox::command::RulePatch>();
            patch->groups = editor.G;
            patch->r2 = editor.r2;
            patch->rules = editor.rules;
            patch->colors = editor.colors;
            patch->enabled = editor.enabled;
            patch->hot = hot;
            sim.push_command(mailbox::command::ApplyRules{patch});
            editor.dirty = false;
        };
        if (!can_hot_apply)
            ImGui::BeginDisabled();
        if (ImGui::Button("Apply (hot, no reseed)")) {
            send_patch(true);
        }
        if (!can_hot_apply) {
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(
                    "Group count/order changed. Hot apply disabled.");
        }
        ImGui::SameLine();
        if (ImGui::Button("Apply & Reseed")) {
            send_patch(false);
        }

        // Utility buttons
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
        if (ImGui::Button("Randomize rules")) {
            static std::mt19937 rng{std::random_device{}()};
            std::uniform_real_distribution<float> dist(-3.14f, 3.14f);
            for (int i = 0; i < editor.G; ++i)
                for (int j = 0; j < editor.G; ++j)
                    editor.rules[i * editor.G + j] = dist(rng);
            editor.dirty = true;
        }

        // Auto-apply when live apply is enabled
        if (editor.live_apply && editor.dirty) {
            send_patch(can_hot_apply);
        }

        ImGui::End();
    }
};
