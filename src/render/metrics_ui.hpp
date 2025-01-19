#ifndef __METRICS_UI_HPP
#define __METRICS_UI_HPP

#include <array>
#include <imgui.h>
#include <raylib.h>

#include "renderconfig.hpp"
#include "renderer.hpp"

class MetricsUI : public IRenderer {
  public:
    MetricsUI(const WindowConfig &wcfg) : m_wcfg(wcfg) {}
    ~MetricsUI() override = default;

    void render(RenderContext &ctx) override {
        if (!ctx.rcfg.show_ui || !ctx.rcfg.show_metrics_ui)
            return;
        render_ui(ctx);
    }

  private:
    void render_ui(RenderContext &ctx) {
        static std::array<float, 240> fps_buf{};
        static std::array<float, 240> tps_buf{};
        static int head = 0;

        const int fps = GetFPS();
        const auto stats = ctx.sim.get_stats();
        fps_buf[head] = (float)fps;
        tps_buf[head] = (float)stats.effective_tps;
        head = (head + 1) % (int)fps_buf.size();

        ImGui::Begin("metrics", NULL);

        const float width = (float)m_wcfg.panel_width;
        const float height = (float)m_wcfg.screen_height * 0.30f;
        ImGui::SetWindowPos(ImVec2{10.f, (float)m_wcfg.screen_height * 0.72f},
                            ImGuiCond_Appearing);
        ImGui::SetWindowSize(ImVec2{width, height}, ImGuiCond_Appearing);

        ImGui::SeparatorText("Performance");

        struct PlotCtx {
            const std::array<float, 240> *arr;
            int headIdx;
        };
        auto plot_circ = [](const std::array<float, 240> &buf, int start,
                            float scale_max) {
            PlotCtx ctx{&buf, start};
            ImGui::PlotLines(
                "",
                [](void *data, int idx) {
                    auto *ctx = (PlotCtx *)data;
                    const auto &arr = *ctx->arr;
                    const int headIdx = ctx->headIdx;
                    const int N = (int)arr.size();
                    return arr[(headIdx + idx) % N];
                },
                (void *)&ctx, (int)buf.size(), 0, NULL, 0.0f, scale_max,
                ImVec2(-1, 44));
        };

        ImGui::Text("FPS: %d", fps);
        plot_circ(fps_buf, head, 240.0f);
        ImGui::Text("TPS: %d", stats.effective_tps);
        plot_circ(tps_buf, head, 240.0f);

        // details moved from ControlUI
        ImGui::SeparatorText("Details");
        ImGui::Text("Last step: %.3f ms", stats.last_step_ns / 1e6);
        ImGui::Text("Num steps: %lld", stats.num_steps);
        ImGui::Text("Particles: %d  Groups: %d  Threads: %d", stats.particles,
                    stats.groups, stats.sim_threads);
        const auto scfg = ctx.sim.get_config();
        ImGui::Text("Sim Bounds: %.0f x %.0f", scfg.bounds_width,
                    scfg.bounds_height);

        ImGui::SeparatorText("Debug DPI");
        ImGui::Text("Screen %d x %d", GetScreenWidth(), GetScreenHeight());
        ImGui::Text("Render %d x %d", GetRenderWidth(), GetRenderHeight());
        ImGui::Text("Mouse  %.1f, %.1f", GetMousePosition().x,
                    GetMousePosition().y);

        ImGui::End();
    }

  private:
    WindowConfig m_wcfg;
};

#endif
