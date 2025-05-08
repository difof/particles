#pragma once

#include <functional>

#include <imgui.h>
#include <raylib.h>

#include "../../undo/undo_manager.hpp"
#include "../../undo/value_action.hpp"
#include "../irenderer.hpp"
#include "../types/window.hpp"

/**
 * @brief UI component for rendering configuration settings
 */
class RenderConfigUI : public IRenderer {
  public:
    RenderConfigUI() = default;
    ~RenderConfigUI() override = default;
    RenderConfigUI(const RenderConfigUI &) = delete;
    RenderConfigUI &operator=(const RenderConfigUI &) = delete;
    RenderConfigUI(RenderConfigUI &&) = delete;
    RenderConfigUI &operator=(RenderConfigUI &&) = delete;

    void render(Context &ctx) override;

  private:
    void render_ui(Context &ctx);
    void render_interpolation_section(Context &ctx);
    void render_background_section(Context &ctx);
    void render_border_section(Context &ctx);
    void render_particle_rendering_section(Context &ctx);
    void render_glow_settings(Context &ctx);
    void render_overlays_section(Context &ctx, std::function<void(bool)> mark);
    void render_velocity_field_settings(Context &ctx);

    template <typename T, typename SetterFunc>
    void push_rcfg(Context &ctx, const char *key, const char *label, T before,
                   T after, SetterFunc setter) {
        ImGuiID id = ImGui::GetItemID();

        if (ImGui::IsItemActivated()) {
            ctx.undo.beginInteraction(id);
        }

        ctx.undo.push(std::unique_ptr<IAction>(new ValueAction<T>(
            key, label,
            []() {
                return T{};
            },
            setter, before, after)));

        if (ImGui::IsItemDeactivatedAfterEdit()) {
            ctx.undo.endInteraction(id);
        }
    }
};
