#ifndef __RENDER_MANAGER_HPP
#define __RENDER_MANAGER_HPP

#include <functional>
#include <raylib.h>

#include "../types.hpp"
#include "context.hpp"
#include "control_ui.hpp"
#include "editor_ui.hpp"
#include "inspector_ui.hpp"
#include "metrics_ui.hpp"
#include "particles_renderer.hpp"

// Manages render textures and frame orchestration.
class RenderManager {
  public:
    RenderManager(const WindowConfig &wcfg)
        : m_wcfg(wcfg), m_particles(wcfg), m_ui(wcfg), m_editor(),
          m_metrics(wcfg) {}

    ~RenderManager() {}

    void draw_frame(Simulation &sim, RenderConfig &rcfg) {
        auto view = sim.begin_read_draw();
        bool canInterp = rcfg.interpolate && view.t0 > 0 && view.t1 > 0 &&
                         view.t1 > view.t0 && view.prev && view.curr &&
                         view.prev->size() == view.curr->size() &&
                         !view.curr->empty();

        float alpha = 1.0f;
        if (canInterp) {
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

        RenderContext ctx{sim, rcfg, view, canInterp, alpha};

        m_particles.render(ctx);
        m_inspector.render(ctx);

        BeginDrawing();
        ClearBackground(BLACK);

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
            m_ui.render(ctx);
            m_editor.render(ctx);
            m_metrics.render(ctx);
            m_inspector.update_selection_from_mouse(ctx);
            m_inspector.render_ui(ctx, m_particles.texture());
        }
        rlImGuiEnd();

        EndDrawing();

        sim.end_read_draw(view);
    }

    const RenderTexture2D &render_texture() const {
        return m_particles.texture();
    }

  private:
    WindowConfig m_wcfg;
    ParticlesRenderer m_particles;
    InspectorUI m_inspector{};
    ControlUI m_ui;
    EditorUI m_editor;
    MetricsUI m_metrics;
};

#endif
