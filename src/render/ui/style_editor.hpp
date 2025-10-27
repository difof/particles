#pragma once

#include <imgui.h>

#include "../irenderer.hpp"
#include "../types/context.hpp"
#include "file_dialog.hpp"

/**
 * @brief UI component for editing ImGui style and exporting to style.cpp
 */
class StyleEditorUI : public IRenderer {
  public:
    StyleEditorUI();
    ~StyleEditorUI() override = default;
    StyleEditorUI(const StyleEditorUI &) = delete;
    StyleEditorUI &operator=(const StyleEditorUI &) = delete;
    StyleEditorUI(StyleEditorUI &&) = delete;
    StyleEditorUI &operator=(StyleEditorUI &&) = delete;

    void render(Context &ctx) override;

  private:
    void render_ui(Context &ctx);
    void render_styles_tab(ImGuiStyle &style);
    void render_colors_tab(ImGuiStyle &style);
    void export_style_cpp(ImGuiStyle &style);
    void reset_style(ImGuiStyle &style);
    const char *get_color_name(ImGuiCol col);

    FileDialog m_file_dialog;
    bool m_file_dialog_open = false;
    ImGuiStyle m_backed_up_style;
};
