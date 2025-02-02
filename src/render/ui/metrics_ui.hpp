#ifndef __METRICS_UI_HPP
#define __METRICS_UI_HPP

#include <array>
#include <imgui.h>
#include <raylib.h>

#include "../renderer.hpp"
#include "../types/config.hpp"

class MetricsUI : public IRenderer {
  public:
    MetricsUI() = default;
    ~MetricsUI() override = default;

    void render(Context &ctx) override {
        if (!ctx.rcfg.show_ui || !ctx.rcfg.show_metrics_ui)
            return;
        render_ui(ctx);
    }

  private:
    void render_ui(Context &ctx) {
        static std::array<float, 240> fps_buf{};
        static std::array<float, 240> tps_buf{};
        static int head = 0;

        const int fps = GetFPS();
        const auto stats = ctx.sim.get_stats();
        fps_buf[head] = (float)fps;
        tps_buf[head] = (float)stats.effective_tps;
        head = (head + 1) % (int)fps_buf.size();

        ImGui::Begin("[1] metrics", &ctx.rcfg.show_metrics_ui);

        const float width = (float)ctx.wcfg.panel_width;
        const float height = (float)ctx.wcfg.screen_height * 0.30f;
        ImGui::SetWindowPos(ImVec2{10.f, (float)ctx.wcfg.screen_height * 0.72f},
                            ImGuiCond_Appearing);
        ImGui::SetWindowSize(ImVec2{width, height}, ImGuiCond_Appearing);

        ImGui::SeparatorText("Performance");

        struct PlotCtx {
            const std::array<float, 240> *arr;
            int headIdx;
        };
        auto plot_circ = [](const std::array<float, 240> &buf, int start,
                            float scale_max, const char *label) {
            PlotCtx ctx{&buf, start};
            ImGui::PlotLines(
                label,
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
        plot_circ(fps_buf, head, 240.0f, "##fps_plot");
        ImGui::Text("TPS: %d", stats.effective_tps);
        plot_circ(tps_buf, head, 240.0f, "##tps_plot");

        // details moved from ControlUI
        ImGui::SeparatorText("Details");
        ImGui::Text("Last step: %.3f ms", stats.last_step_ns / 1e6);
        ImGui::Text("Num steps: %lld", stats.num_steps);
        ImGui::Text("Particles: %d  Groups: %d  Threads: %d", stats.particles,
                    stats.groups, stats.sim_threads);
        const auto scfg = ctx.sim.get_config();
        ImGui::Text("Sim Bounds: %.0f x %.0f", scfg.bounds_width,
                    scfg.bounds_height);

        ImGui::SeparatorText("Camera");
        ImGui::Text("Position: %.1f, %.1f", ctx.rcfg.camera.x,
                    ctx.rcfg.camera.y);
        ImGui::SameLine();
        if (ImGui::Button("Center")) {
            // Center camera at (0,0) which centers on the bounds center
            ctx.rcfg.camera.x = 0.0f;
            ctx.rcfg.camera.y = 0.0f;
        }
        ImGui::Text("Zoom: %.2fx (log: %.2f)", ctx.rcfg.camera.zoom(),
                    ctx.rcfg.camera.zoom_log);
        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
            ctx.rcfg.camera.zoom_log = 0.0f;
        }

        ImGui::SeparatorText("Debug DPI");
        ImGui::Text("Screen %d x %d", GetScreenWidth(), GetScreenHeight());
        ImGui::Text("Render %d x %d", GetRenderWidth(), GetRenderHeight());
        ImGui::Text("Mouse  %.1f, %.1f", GetMousePosition().x,
                    GetMousePosition().y);

        ImGui::End();
    }
};

#endif
