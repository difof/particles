#pragma once

#include <imgui.h>
#include <raylib.h>

#include "../../undo/undo_manager.hpp"
#include "../../undo/value_action.hpp"
#include "../irenderer.hpp"
#include "../types/window.hpp"

/**
 * @brief UI component for simulation configuration
 */
class SimConfigUI : public IRenderer {
  public:
    SimConfigUI() = default;
    ~SimConfigUI() override = default;
    SimConfigUI(const SimConfigUI &) = delete;
    SimConfigUI &operator=(const SimConfigUI &) = delete;
    SimConfigUI(SimConfigUI &&) = delete;
    SimConfigUI &operator=(SimConfigUI &&) = delete;

    void render(Context &ctx) override;

  private:
    void render_ui(Context &ctx);
    void render_bounds_section(Context &ctx,
                               mailbox::SimulationConfigSnapshot &scfg,
                               bool &scfg_updated);
    void render_simulation_params(Context &ctx,
                                  mailbox::SimulationConfigSnapshot &scfg,
                                  bool &scfg_updated);
    void render_gravity_section(Context &ctx,
                                mailbox::SimulationConfigSnapshot &scfg,
                                bool &scfg_updated);
    void render_parallelism_section(Context &ctx,
                                    mailbox::SimulationConfigSnapshot &scfg,
                                    bool &scfg_updated);

    template <typename T, typename F>
    void push_scfg_action(Context &ctx, const char *key, const char *label,
                          T before, T after, F setter) {
        ImGuiID id = ImGui::GetItemID();
        if (ImGui::IsItemActivated())
            ctx.undo.beginInteraction(id);

        ctx.undo.push(std::unique_ptr<IAction>(new ValueAction<T>(
            key, label,
            []() {
                return T{};
            },
            setter, before, after)));

        if (ImGui::IsItemDeactivatedAfterEdit())
            ctx.undo.endInteraction(id);
    }
};
