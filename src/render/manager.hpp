#pragma once

#include <chrono>
#include <functional>
#include <raylib.h>
#include <string>

#include "../save_manager.hpp"
#include "../undo.hpp"
#include "../window_config.hpp"
#include "particles_renderer.hpp"
#include "types/context.hpp"
#include "ui/editor_ui.hpp"
#include "ui/inspector_ui.hpp"
#include "ui/menu_bar_ui.hpp"
#include "ui/metrics_ui.hpp"
#include "ui/render_config_ui.hpp"
#include "ui/sim_config_ui.hpp"

// Manages render textures and frame orchestration.
class RenderManager {
  public:
    RenderManager(const WindowConfig &wcfg, SaveManager &json_manager,
                  UndoManager &undo_manager)
        : m_wcfg(wcfg), m_particles(wcfg), m_json_manager(json_manager),
          m_undo_manager(undo_manager) {}

    ~RenderManager() {}

    bool draw_frame(Simulation &sim, Config &rcfg) {
        auto view = sim.begin_read_draw();
        bool can_interpolate = rcfg.interpolate && view.t0 > 0 && view.t1 > 0 &&
                               view.t1 > view.t0 && view.prev && view.curr &&
                               view.prev->size() == view.curr->size() &&
                               !view.curr->empty();

        float alpha = 1.0f;
        if (can_interpolate) {
            const long long now_ns =
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now().time_since_epoch())
                    .count();
            const long long target_ns =
                now_ns - (long long)(rcfg.interp_delay_ms * 1'000'000.0f);
            if (target_ns <= view.t0)
                alpha = 0.0f;
            else if (target_ns >= view.t1)
                alpha = 1.0f;
            else
                alpha = float(target_ns - view.t0) / float(view.t1 - view.t0);
        }

        Context ctx{
            sim,   rcfg,           view,          m_wcfg, can_interpolate,
            alpha, m_undo_manager, m_json_manager};

        m_particles.render(ctx);
        m_inspector.render(ctx);

        BeginDrawing();
        ClearBackground(BLACK);

        // FIXME: if we also change this background, the preview in inspector
        // will not have the same effect:
        // ClearBackground(rcfg.background_color);

        {
            DrawTextureRec(
                m_particles.texture().texture,
                (Rectangle){0, 0, (float)m_particles.texture().texture.width,
                            (float)-m_particles.texture().texture.height},
                (Vector2){0, 0}, WHITE);

            DrawTextureRec(
                m_inspector.texture().texture,
                (Rectangle){0, 0, (float)m_particles.texture().texture.width,
                            (float)-m_particles.texture().texture.height},
                (Vector2){0, 0}, WHITE);
        }

        rlImGuiBegin();
        {
            m_menu_bar.render(ctx);
            m_editor.render(ctx);
            m_render_config.render(ctx);
            m_sim_config.render(ctx);
            m_metrics.render(ctx);
            m_inspector.update_selection_from_mouse(ctx);
            m_inspector.render_ui(ctx, m_particles.texture());
        }
        rlImGuiEnd();

        EndDrawing();

        sim.end_read_draw(view);

        return ctx.should_exit;
    }

  private:
    WindowConfig m_wcfg;
    ParticlesRenderer m_particles;
    InspectorUI m_inspector{};
    MenuBarUI m_menu_bar;
    EditorUI m_editor;
    RenderConfigUI m_render_config;
    SimConfigUI m_sim_config;
    MetricsUI m_metrics;
    SaveManager &m_json_manager;
    UndoManager &m_undo_manager;
};
