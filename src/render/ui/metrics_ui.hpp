#pragma once

#include <array>

#include <imgui.h>
#include <raylib.h>

#include "../../mailbox/data_snapshot.hpp"
#include "../irenderer.hpp"
#include "../types/config.hpp"

/**
 * @brief UI component for displaying performance metrics and debug information
 */
class MetricsUI : public IRenderer {
  public:
    MetricsUI() = default;
    ~MetricsUI() override = default;
    MetricsUI(const MetricsUI &) = delete;
    MetricsUI &operator=(const MetricsUI &) = delete;
    MetricsUI(MetricsUI &&) = delete;
    MetricsUI &operator=(MetricsUI &&) = delete;

    /**
     * @brief Renders the metrics UI if enabled
     * @param ctx Rendering context containing configuration and simulation data
     */
    void render(Context &ctx) override;

  private:
    void render_ui(Context &ctx);
    void render_performance_section(
        Context &ctx, const std::array<float, 240> &fps_buf,
        const std::array<float, 240> &tps_buf, int head, int fps,
        const mailbox::SimulationStatsSnapshot &stats);
    void render_details_section(Context &ctx,
                                const mailbox::SimulationStatsSnapshot &stats);
    void render_camera_section(Context &ctx);
    void render_debug_section();
};
