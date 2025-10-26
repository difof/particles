#include "history_ui.hpp"

#include <chrono>
#include <fmt/format.h>
#include <iomanip>
#include <sstream>

void HistoryUI::render(Context &ctx) {
    if (!ctx.rcfg.show_ui || !ctx.rcfg.show_history_ui) {
        return;
    }
    render_ui(ctx);
}

void HistoryUI::render_ui(Context &ctx) {
    ImGui::Begin("[5] Undo History", &ctx.rcfg.show_history_ui);
    ImGui::SetWindowSize(ImVec2{600, 400}, ImGuiCond_FirstUseEver);
    ImGui::SetWindowPos(ImVec2{50, 50}, ImGuiCond_FirstUseEver);

    const auto &past_entries = ctx.undo.get_past_entries();
    const auto &future_entries = ctx.undo.get_future_entries();

    // Check if history is empty
    if (past_entries.empty() && future_entries.empty()) {
        ImGui::Text("No actions in history");
        ImGui::End();
        return;
    }

    // Create a scrollable child window
    ImGui::BeginChild("HistoryList",
                      ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), false,
                      ImGuiWindowFlags_HorizontalScrollbar);

    // Render past entries (most recent first)
    for (int i = static_cast<int>(past_entries.size()) - 1; i >= 0; --i) {
        const auto &entry = past_entries[i];
        bool is_current_state =
            (i == static_cast<int>(past_entries.size()) - 1);

        if (is_current_state) {
            // Highlight current state in green
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
        }

        std::string timestamp_str = format_timestamp(entry.timestamp);
        std::string description =
            entry.act ? entry.act->get_description() : "Unknown Action";

        ImGui::Text("[%s] %s", timestamp_str.c_str(), description.c_str());

        if (is_current_state) {
            ImGui::PopStyleColor();
        }
    }

    // Add separator between past and future
    if (!past_entries.empty() && !future_entries.empty()) {
        ImGui::Separator();
        ImGui::Text("--- Future Actions (Redo) ---");
        ImGui::Separator();
    }

    // Render future entries (oldest first)
    for (size_t i = 0; i < future_entries.size(); ++i) {
        const auto &entry = future_entries[i];
        std::string timestamp_str = format_timestamp(entry.timestamp);
        std::string description =
            entry.act ? entry.act->get_description() : "Unknown Action";

        ImGui::Text("[%s] %s", timestamp_str.c_str(), description.c_str());
    }

    ImGui::EndChild();

    // Add summary at bottom
    ImGui::Separator();
    ImGui::Text("Past: %zu | Future: %zu", past_entries.size(),
                future_entries.size());

    ImGui::End();
}

std::string HistoryUI::format_timestamp(
    const std::chrono::time_point<std::chrono::steady_clock> &timestamp) const {
    auto now = std::chrono::steady_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::seconds>(now - timestamp);

    if (duration.count() < 60) {
        return fmt::format("{}s ago", duration.count());
    } else if (duration.count() < 3600) {
        return fmt::format("{}m ago", duration.count() / 60);
    } else {
        return fmt::format("{}h ago", duration.count() / 3600);
    }
}
