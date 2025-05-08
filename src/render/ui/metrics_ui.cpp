#include "metrics_ui.hpp"

void MetricsUI::render(Context &ctx) {
    if (!ctx.rcfg.show_ui || !ctx.rcfg.show_metrics_ui) {
        return;
    }
    render_ui(ctx);
}

void MetricsUI::render_ui(Context &ctx) {
    static std::array<float, 240> fps_buf{};
    static std::array<float, 240> tps_buf{};
    static int head = 0;

    const int fps = GetFPS();
    const auto stats = ctx.sim.get_stats();
    fps_buf[head] = static_cast<float>(fps);
    tps_buf[head] = static_cast<float>(stats.effective_tps);
    head = (head + 1) % static_cast<int>(fps_buf.size());

    ImGui::Begin("[1] metrics", &ctx.rcfg.show_metrics_ui);

    const float width = static_cast<float>(ctx.wcfg.screen_width) * 0.25f;
    const float height = static_cast<float>(ctx.wcfg.screen_height) * 0.30f;
    ImGui::SetWindowPos(
        ImVec2{10.f, static_cast<float>(ctx.wcfg.screen_height) * 0.72f},
        ImGuiCond_Appearing);
    ImGui::SetWindowSize(ImVec2{width, height}, ImGuiCond_Appearing);

    render_performance_section(ctx, fps_buf, tps_buf, head, fps, stats);
    render_details_section(ctx, stats);
    render_camera_section(ctx);
    render_debug_section();

    ImGui::End();
}

void MetricsUI::render_performance_section(
    Context &ctx, const std::array<float, 240> &fps_buf,
    const std::array<float, 240> &tps_buf, int head, int fps,
    const mailbox::SimulationStatsSnapshot &stats) {
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
                auto *ctx = static_cast<PlotCtx *>(data);
                const auto &arr = *ctx->arr;
                const int headIdx = ctx->headIdx;
                const int N = static_cast<int>(arr.size());
                return arr[(headIdx + idx) % N];
            },
            static_cast<void *>(&ctx), static_cast<int>(buf.size()), 0, nullptr,
            0.0f, scale_max, ImVec2(-1, 44));
    };

    ImGui::Text("FPS: %d", fps);
    plot_circ(fps_buf, head, 240.0f, "##fps_plot");
    ImGui::Text("TPS: %d", stats.effective_tps);
    plot_circ(tps_buf, head, 240.0f, "##tps_plot");
}

void MetricsUI::render_details_section(
    Context &ctx, const mailbox::SimulationStatsSnapshot &stats) {
    ImGui::SeparatorText("Details");
    ImGui::Text("Last step: %.3f ms", stats.last_step_ns / 1e6);
    ImGui::Text("Num steps: %lld", stats.num_steps);
    ImGui::Text("Particles: %d  Groups: %d  Threads: %d", stats.particles,
                stats.groups, stats.sim_threads);
    const auto scfg = ctx.sim.get_config();
    ImGui::Text("Sim Bounds: %.0f x %.0f", scfg.bounds_width,
                scfg.bounds_height);
}

void MetricsUI::render_camera_section(Context &ctx) {
    ImGui::SeparatorText("Camera");
    ImGui::Text("Position: %.1f, %.1f", ctx.rcfg.camera.x, ctx.rcfg.camera.y);
    ImGui::SameLine();
    if (ImGui::Button("Center")) {
        ctx.rcfg.camera.x = 0.0f;
        ctx.rcfg.camera.y = 0.0f;
    }
    ImGui::Text("Zoom: %.2fx (log: %.2f)", ctx.rcfg.camera.zoom(),
                ctx.rcfg.camera.zoom_log);
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        ctx.rcfg.camera.zoom_log = 0.0f;
    }
}

void MetricsUI::render_debug_section() {
    ImGui::SeparatorText("Debug DPI");
    ImGui::Text("Screen %d x %d", GetScreenWidth(), GetScreenHeight());
    ImGui::Text("Render %d x %d", GetRenderWidth(), GetRenderHeight());
    ImGui::Text("Mouse  %.1f, %.1f", GetMousePosition().x,
                GetMousePosition().y);
}
