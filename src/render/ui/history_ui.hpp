#pragma once

#include <imgui.h>
#include <raylib.h>

#include "../irenderer.hpp"

/**
 * @brief UI component for displaying undo/redo history.
 *
 * Shows a scrollable list of all past and future actions with timestamps,
 * highlighting the current undo/redo position in green.
 */
class HistoryUI : public IRenderer {
  public:
    HistoryUI() = default;
    ~HistoryUI() override = default;
    HistoryUI(const HistoryUI &) = delete;
    HistoryUI &operator=(const HistoryUI &) = delete;
    HistoryUI(HistoryUI &&) = delete;
    HistoryUI &operator=(HistoryUI &&) = delete;

    /**
     * @brief Renders the history UI if enabled
     * @param ctx Rendering context containing configuration and undo manager
     */
    void render(Context &ctx) override;

  private:
    /**
     * @brief Renders the main history window
     * @param ctx Rendering context
     */
    void render_ui(Context &ctx);

    /**
     * @brief Formats a timestamp for display
     * @param timestamp The timestamp to format
     * @return Formatted time string
     */
    std::string format_timestamp(
        const std::chrono::time_point<std::chrono::steady_clock> &timestamp)
        const;

    /**
     * @brief Renders a single history entry
     * @param entry The history entry to render
     * @param is_current_state Whether this entry represents the current state
     */
    void render_history_entry(const auto &entry, bool is_current_state);
};
