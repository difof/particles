#include "style_editor.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

#include <imgui.h>

#include "../../utility/logger.hpp"
#include "file_dialog.hpp"

static std::string format_float(float value) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(10) << value;
    std::string result = oss.str();
    // Remove trailing zeros after decimal point, but keep at least one decimal
    // place
    size_t dot_pos = result.find('.');
    if (dot_pos == std::string::npos) {
        result += ".0";
    } else {
        // Remove trailing zeros, but keep at least one digit after decimal
        size_t last_nonzero = result.find_last_not_of('0');
        if (last_nonzero == dot_pos) {
            // Only zeros after decimal, keep one zero
            result = result.substr(0, dot_pos + 2);
        } else if (last_nonzero != std::string::npos) {
            result = result.substr(0, last_nonzero + 1);
        }
    }
    return result;
}

StyleEditorUI::StyleEditorUI() { m_backed_up_style = ImGui::GetStyle(); }

void StyleEditorUI::render(Context &ctx) {
    if (!ctx.rcfg.show_ui || !ctx.rcfg.show_style_editor) {
        return;
    }
    render_ui(ctx);
}

void StyleEditorUI::render_ui(Context &ctx) {
    ImGui::Begin("Style Editor", &ctx.rcfg.show_style_editor);
    ImGui::SetWindowSize(ImVec2{800, 600}, ImGuiCond_FirstUseEver);

    ImGuiStyle &style = ImGui::GetStyle();

    if (ImGui::Button("Export style.cpp")) {
        m_file_dialog.open(FileDialog::Mode::Save, "Save Style", "", &ctx.save);
        m_file_dialog.set_filename("style.cpp");
        m_file_dialog_open = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        reset_style(style);
    }

    if (m_file_dialog_open) {
        if (m_file_dialog.render()) {
            m_file_dialog_open = false;
            if (m_file_dialog.has_result() && !m_file_dialog.canceled()) {
                export_style_cpp(style);
            }
        }
    }

    ImGui::Separator();

    ImVec2 avail_size = ImGui::GetContentRegionAvail();
    avail_size.y -= ImGui::GetStyle().ItemSpacing.y;

    if (ImGui::BeginTabBar("StyleEditorTabs", ImGuiTabBarFlags_None)) {
        if (ImGui::BeginTabItem("Styles")) {
            if (ImGui::BeginChild("StylesContent", avail_size, false,
                                  ImGuiWindowFlags_None)) {
                render_styles_tab(style);
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Colors")) {
            if (ImGui::BeginChild("ColorsContent", avail_size, false,
                                  ImGuiWindowFlags_None)) {
                render_colors_tab(style);
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

void StyleEditorUI::render_styles_tab(ImGuiStyle &style) {
    ImGui::PushItemWidth(ImGui::GetFontSize() * -12);

    ImGui::SeparatorText("Font Scaling");
    ImGui::DragFloat("FontSizeBase", &style.FontSizeBase, 0.1f, 0.0f, 0.0f,
                     "%.1f");
    ImGui::DragFloat("FontScaleMain", &style.FontScaleMain, 0.01f, 0.0f, 10.0f,
                     "%.2f");
    ImGui::DragFloat("FontScaleDpi", &style.FontScaleDpi, 0.01f, 0.0f, 10.0f,
                     "%.2f");

    ImGui::SeparatorText("Alpha");
    ImGui::DragFloat("Alpha", &style.Alpha, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("DisabledAlpha", &style.DisabledAlpha, 0.01f, 0.0f, 1.0f,
                     "%.2f");

    ImGui::SeparatorText("Window");
    ImGui::DragFloat2("WindowPadding", (float *)&style.WindowPadding, 0.5f,
                      0.0f, 0.0f, "%.1f");
    ImGui::DragFloat("WindowRounding", &style.WindowRounding, 0.5f, 0.0f, 0.0f,
                     "%.1f");
    ImGui::DragFloat("WindowBorderSize", &style.WindowBorderSize, 0.1f, 0.0f,
                     10.0f, "%.1f");
    ImGui::DragFloat("WindowBorderHoverPadding",
                     &style.WindowBorderHoverPadding, 0.5f, 0.0f, 0.0f, "%.1f");
    ImGui::DragFloat2("WindowMinSize", (float *)&style.WindowMinSize, 1.0f,
                      0.0f, 0.0f, "%.1f");
    ImGui::DragFloat2("WindowTitleAlign", (float *)&style.WindowTitleAlign,
                      0.01f, 0.0f, 1.0f, "%.2f");
    const char *menu_button_pos_items[] = {"None", "Left", "Right"};
    int menu_button_pos = (int)style.WindowMenuButtonPosition;
    if (ImGui::Combo("WindowMenuButtonPosition", &menu_button_pos,
                     menu_button_pos_items,
                     IM_ARRAYSIZE(menu_button_pos_items))) {
        style.WindowMenuButtonPosition = (ImGuiDir)menu_button_pos;
    }

    ImGui::SeparatorText("Child Windows");
    ImGui::DragFloat("ChildRounding", &style.ChildRounding, 0.5f, 0.0f, 0.0f,
                     "%.1f");
    ImGui::DragFloat("ChildBorderSize", &style.ChildBorderSize, 0.1f, 0.0f,
                     10.0f, "%.1f");

    ImGui::SeparatorText("Popup");
    ImGui::DragFloat("PopupRounding", &style.PopupRounding, 0.5f, 0.0f, 0.0f,
                     "%.1f");
    ImGui::DragFloat("PopupBorderSize", &style.PopupBorderSize, 0.1f, 0.0f,
                     10.0f, "%.1f");

    ImGui::SeparatorText("Frame");
    ImGui::DragFloat2("FramePadding", (float *)&style.FramePadding, 0.5f, 0.0f,
                      0.0f, "%.1f");
    ImGui::DragFloat("FrameRounding", &style.FrameRounding, 0.5f, 0.0f, 0.0f,
                     "%.1f");
    ImGui::DragFloat("FrameBorderSize", &style.FrameBorderSize, 0.1f, 0.0f,
                     10.0f, "%.1f");

    ImGui::SeparatorText("Item Spacing");
    ImGui::DragFloat2("ItemSpacing", (float *)&style.ItemSpacing, 0.5f, 0.0f,
                      0.0f, "%.1f");
    ImGui::DragFloat2("ItemInnerSpacing", (float *)&style.ItemInnerSpacing,
                      0.5f, 0.0f, 0.0f, "%.1f");
    ImGui::DragFloat2("CellPadding", (float *)&style.CellPadding, 0.5f, 0.0f,
                      0.0f, "%.1f");
    ImGui::DragFloat2("TouchExtraPadding", (float *)&style.TouchExtraPadding,
                      0.5f, 0.0f, 0.0f, "%.1f");

    ImGui::SeparatorText("Indentation");
    ImGui::DragFloat("IndentSpacing", &style.IndentSpacing, 1.0f, 0.0f, 0.0f,
                     "%.1f");
    ImGui::DragFloat("ColumnsMinSpacing", &style.ColumnsMinSpacing, 1.0f, 0.0f,
                     0.0f, "%.1f");

    ImGui::SeparatorText("Scrollbar");
    ImGui::DragFloat("ScrollbarSize", &style.ScrollbarSize, 1.0f, 0.0f, 0.0f,
                     "%.1f");
    ImGui::DragFloat("ScrollbarRounding", &style.ScrollbarRounding, 0.5f, 0.0f,
                     0.0f, "%.1f");
    ImGui::DragFloat("ScrollbarPadding", &style.ScrollbarPadding, 1.0f, 0.0f,
                     0.0f, "%.1f");

    ImGui::SeparatorText("Grab");
    ImGui::DragFloat("GrabMinSize", &style.GrabMinSize, 1.0f, 0.0f, 0.0f,
                     "%.1f");
    ImGui::DragFloat("GrabRounding", &style.GrabRounding, 0.5f, 0.0f, 0.0f,
                     "%.1f");
    ImGui::DragFloat("LogSliderDeadzone", &style.LogSliderDeadzone, 1.0f, 0.0f,
                     0.0f, "%.1f");

    ImGui::SeparatorText("Image");
    ImGui::DragFloat("ImageBorderSize", &style.ImageBorderSize, 0.1f, 0.0f,
                     10.0f, "%.1f");

    ImGui::SeparatorText("Tab");
    ImGui::DragFloat("TabRounding", &style.TabRounding, 0.5f, 0.0f, 0.0f,
                     "%.1f");
    ImGui::DragFloat("TabBorderSize", &style.TabBorderSize, 0.1f, 0.0f, 10.0f,
                     "%.1f");
    ImGui::DragFloat("TabMinWidthBase", &style.TabMinWidthBase, 1.0f, 0.0f,
                     0.0f, "%.1f");
    ImGui::DragFloat("TabMinWidthShrink", &style.TabMinWidthShrink, 1.0f, 0.0f,
                     0.0f, "%.1f");
    ImGui::DragFloat("TabCloseButtonMinWidthSelected",
                     &style.TabCloseButtonMinWidthSelected, 1.0f, -1.0f,
                     FLT_MAX, "%.1f");
    ImGui::DragFloat("TabCloseButtonMinWidthUnselected",
                     &style.TabCloseButtonMinWidthUnselected, 1.0f, -1.0f,
                     FLT_MAX, "%.1f");
    ImGui::DragFloat("TabBarBorderSize", &style.TabBarBorderSize, 0.1f, 0.0f,
                     10.0f, "%.1f");
    ImGui::DragFloat("TabBarOverlineSize", &style.TabBarOverlineSize, 0.1f,
                     0.0f, 10.0f, "%.1f");

    ImGui::SeparatorText("Table");
    ImGui::DragFloat("TableAngledHeadersAngle", &style.TableAngledHeadersAngle,
                     1.0f, -50.0f, 50.0f, "%.1f");
    ImGui::DragFloat2("TableAngledHeadersTextAlign",
                      (float *)&style.TableAngledHeadersTextAlign, 0.01f, 0.0f,
                      1.0f, "%.2f");

    ImGui::SeparatorText("Tree Lines");
    int tree_lines_flags = (int)style.TreeLinesFlags;
    bool draw_lines_none =
        (tree_lines_flags & ImGuiTreeNodeFlags_DrawLinesNone) != 0;
    bool draw_lines_full =
        (tree_lines_flags & ImGuiTreeNodeFlags_DrawLinesFull) != 0;
    bool draw_lines_to_nodes =
        (tree_lines_flags & ImGuiTreeNodeFlags_DrawLinesToNodes) != 0;

    if (ImGui::Checkbox("DrawLinesNone", &draw_lines_none)) {
        if (draw_lines_none) {
            style.TreeLinesFlags = (style.TreeLinesFlags &
                                    ~(ImGuiTreeNodeFlags_DrawLinesFull |
                                      ImGuiTreeNodeFlags_DrawLinesToNodes)) |
                                   ImGuiTreeNodeFlags_DrawLinesNone;
        }
    }
    if (ImGui::Checkbox("DrawLinesFull", &draw_lines_full)) {
        if (draw_lines_full) {
            style.TreeLinesFlags = (style.TreeLinesFlags &
                                    ~(ImGuiTreeNodeFlags_DrawLinesNone |
                                      ImGuiTreeNodeFlags_DrawLinesToNodes)) |
                                   ImGuiTreeNodeFlags_DrawLinesFull;
        }
    }
    if (ImGui::Checkbox("DrawLinesToNodes", &draw_lines_to_nodes)) {
        if (draw_lines_to_nodes) {
            style.TreeLinesFlags =
                (style.TreeLinesFlags & ~(ImGuiTreeNodeFlags_DrawLinesNone |
                                          ImGuiTreeNodeFlags_DrawLinesFull)) |
                ImGuiTreeNodeFlags_DrawLinesToNodes;
        }
    }
    ImGui::DragFloat("TreeLinesSize", &style.TreeLinesSize, 0.5f, 0.0f, 0.0f,
                     "%.1f");
    ImGui::DragFloat("TreeLinesRounding", &style.TreeLinesRounding, 0.5f, 0.0f,
                     0.0f, "%.1f");

    ImGui::SeparatorText("Color Button");
    const char *color_button_pos_items[] = {"Left", "Right"};
    int color_button_pos = (int)style.ColorButtonPosition;
    if (ImGui::Combo("ColorButtonPosition", &color_button_pos,
                     color_button_pos_items,
                     IM_ARRAYSIZE(color_button_pos_items))) {
        style.ColorButtonPosition = (ImGuiDir)color_button_pos;
    }

    ImGui::SeparatorText("Text Alignment");
    ImGui::DragFloat2("ButtonTextAlign", (float *)&style.ButtonTextAlign, 0.01f,
                      0.0f, 1.0f, "%.2f");
    ImGui::DragFloat2("SelectableTextAlign",
                      (float *)&style.SelectableTextAlign, 0.01f, 0.0f, 1.0f,
                      "%.2f");

    ImGui::SeparatorText("Separator Text");
    ImGui::DragFloat("SeparatorTextBorderSize", &style.SeparatorTextBorderSize,
                     0.1f, 0.0f, 10.0f, "%.1f");
    ImGui::DragFloat2("SeparatorTextAlign", (float *)&style.SeparatorTextAlign,
                      0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat2("SeparatorTextPadding",
                      (float *)&style.SeparatorTextPadding, 0.5f, 0.0f, 0.0f,
                      "%.1f");

    ImGui::SeparatorText("Display");
    ImGui::DragFloat2("DisplayWindowPadding",
                      (float *)&style.DisplayWindowPadding, 1.0f, 0.0f, 0.0f,
                      "%.1f");
    ImGui::DragFloat2("DisplaySafeAreaPadding",
                      (float *)&style.DisplaySafeAreaPadding, 1.0f, 0.0f, 0.0f,
                      "%.1f");
    ImGui::DragFloat("MouseCursorScale", &style.MouseCursorScale, 0.1f, 0.0f,
                     10.0f, "%.1f");

    ImGui::SeparatorText("Anti-aliasing");
    ImGui::Checkbox("AntiAliasedLines", &style.AntiAliasedLines);
    ImGui::Checkbox("AntiAliasedLinesUseTex", &style.AntiAliasedLinesUseTex);
    ImGui::Checkbox("AntiAliasedFill", &style.AntiAliasedFill);

    ImGui::SeparatorText("Tessellation");
    ImGui::DragFloat("CurveTessellationTol", &style.CurveTessellationTol, 0.1f,
                     0.0f, 0.0f, "%.1f");
    ImGui::DragFloat("CircleTessellationMaxError",
                     &style.CircleTessellationMaxError, 0.1f, 0.0f, 0.0f,
                     "%.1f");

    ImGui::SeparatorText("Hover Behaviors");
    ImGui::DragFloat("HoverStationaryDelay", &style.HoverStationaryDelay, 0.01f,
                     0.0f, 10.0f, "%.2f");
    ImGui::DragFloat("HoverDelayShort", &style.HoverDelayShort, 0.01f, 0.0f,
                     10.0f, "%.2f");
    ImGui::DragFloat("HoverDelayNormal", &style.HoverDelayNormal, 0.01f, 0.0f,
                     10.0f, "%.2f");

    ImGui::PopItemWidth();
}

void StyleEditorUI::reset_style(ImGuiStyle &style) {
    style = m_backed_up_style;
}

void StyleEditorUI::render_colors_tab(ImGuiStyle &style) {
    ImGui::PushItemWidth(ImGui::GetFontSize() * -12);

    for (int i = 0; i < ImGuiCol_COUNT; i++) {
        ImGuiCol col = (ImGuiCol)i;
        const char *col_name = get_color_name(col);
        ImVec4 &col_value = style.Colors[i];

        ImGui::ColorEdit4(col_name, (float *)&col_value,
                          ImGuiColorEditFlags_AlphaBar |
                              ImGuiColorEditFlags_AlphaPreviewHalf);
    }

    ImGui::PopItemWidth();
}

void StyleEditorUI::export_style_cpp(ImGuiStyle &style) {
    std::string filepath = m_file_dialog.selected_path();
    if (filepath.empty()) {
        LOG_ERROR("Export failed: no file path selected");
        return;
    }

    std::ofstream file(filepath);
    if (!file.is_open()) {
        LOG_ERROR("Export failed: could not open file: " + filepath);
        return;
    }

    file << "// ImGui Style\n";
    file << "// Generated by Style Editor\n";
    file << "#include <imgui.h>\n\n";
    file << "void setup_style() {\n";
    file << "    ImGuiStyle &style = ImGui::GetStyle();\n\n";

    file << "    // Font scaling\n";
    file << "    style.FontSizeBase = " << format_float(style.FontSizeBase)
         << "f;\n";
    file << "    style.FontScaleMain = " << format_float(style.FontScaleMain)
         << "f;\n";
    file << "    style.FontScaleDpi = " << format_float(style.FontScaleDpi)
         << "f;\n\n";

    file << "    // Alpha\n";
    file << "    style.Alpha = " << format_float(style.Alpha) << "f;\n";
    file << "    style.DisabledAlpha = " << format_float(style.DisabledAlpha)
         << "f;\n\n";

    file << "    // Window\n";
    file << "    style.WindowPadding = ImVec2("
         << format_float(style.WindowPadding.x) << "f, "
         << format_float(style.WindowPadding.y) << "f);\n";
    file << "    style.WindowRounding = " << format_float(style.WindowRounding)
         << "f;\n";
    file << "    style.WindowBorderSize = "
         << format_float(style.WindowBorderSize) << "f;\n";
    file << "    style.WindowBorderHoverPadding = "
         << format_float(style.WindowBorderHoverPadding) << "f;\n";
    file << "    style.WindowMinSize = ImVec2("
         << format_float(style.WindowMinSize.x) << "f, "
         << format_float(style.WindowMinSize.y) << "f);\n";
    file << "    style.WindowTitleAlign = ImVec2("
         << format_float(style.WindowTitleAlign.x) << "f, "
         << format_float(style.WindowTitleAlign.y) << "f);\n";
    file << "    style.WindowMenuButtonPosition = ImGuiDir_"
         << (style.WindowMenuButtonPosition == ImGuiDir_None   ? "None"
             : style.WindowMenuButtonPosition == ImGuiDir_Left ? "Left"
                                                               : "Right")
         << ";\n\n";

    file << "    // Child\n";
    file << "    style.ChildRounding = " << format_float(style.ChildRounding)
         << "f;\n";
    file << "    style.ChildBorderSize = "
         << format_float(style.ChildBorderSize) << "f;\n\n";

    file << "    // Popup\n";
    file << "    style.PopupRounding = " << format_float(style.PopupRounding)
         << "f;\n";
    file << "    style.PopupBorderSize = "
         << format_float(style.PopupBorderSize) << "f;\n\n";

    file << "    // Frame\n";
    file << "    style.FramePadding = ImVec2("
         << format_float(style.FramePadding.x) << "f, "
         << format_float(style.FramePadding.y) << "f);\n";
    file << "    style.FrameRounding = " << format_float(style.FrameRounding)
         << "f;\n";
    file << "    style.FrameBorderSize = "
         << format_float(style.FrameBorderSize) << "f;\n\n";

    file << "    // Item spacing\n";
    file << "    style.ItemSpacing = ImVec2("
         << format_float(style.ItemSpacing.x) << "f, "
         << format_float(style.ItemSpacing.y) << "f);\n";
    file << "    style.ItemInnerSpacing = ImVec2("
         << format_float(style.ItemInnerSpacing.x) << "f, "
         << format_float(style.ItemInnerSpacing.y) << "f);\n";
    file << "    style.CellPadding = ImVec2("
         << format_float(style.CellPadding.x) << "f, "
         << format_float(style.CellPadding.y) << "f);\n";
    file << "    style.TouchExtraPadding = ImVec2("
         << format_float(style.TouchExtraPadding.x) << "f, "
         << format_float(style.TouchExtraPadding.y) << "f);\n\n";

    file << "    // Indentation\n";
    file << "    style.IndentSpacing = " << format_float(style.IndentSpacing)
         << "f;\n";
    file << "    style.ColumnsMinSpacing = "
         << format_float(style.ColumnsMinSpacing) << "f;\n\n";

    file << "    // Scrollbar\n";
    file << "    style.ScrollbarSize = " << format_float(style.ScrollbarSize)
         << "f;\n";
    file << "    style.ScrollbarRounding = "
         << format_float(style.ScrollbarRounding) << "f;\n";
    file << "    style.ScrollbarPadding = "
         << format_float(style.ScrollbarPadding) << "f;\n\n";

    file << "    // Grab\n";
    file << "    style.GrabMinSize = " << format_float(style.GrabMinSize)
         << "f;\n";
    file << "    style.GrabRounding = " << format_float(style.GrabRounding)
         << "f;\n";
    file << "    style.LogSliderDeadzone = "
         << format_float(style.LogSliderDeadzone) << "f;\n\n";

    file << "    // Image\n";
    file << "    style.ImageBorderSize = "
         << format_float(style.ImageBorderSize) << "f;\n\n";

    file << "    // Tab\n";
    file << "    style.TabRounding = " << format_float(style.TabRounding)
         << "f;\n";
    file << "    style.TabBorderSize = " << format_float(style.TabBorderSize)
         << "f;\n";
    file << "    style.TabMinWidthBase = "
         << format_float(style.TabMinWidthBase) << "f;\n";
    file << "    style.TabMinWidthShrink = "
         << format_float(style.TabMinWidthShrink) << "f;\n";
    file << "    style.TabCloseButtonMinWidthSelected = "
         << format_float(style.TabCloseButtonMinWidthSelected) << "f;\n";
    file << "    style.TabCloseButtonMinWidthUnselected = "
         << format_float(style.TabCloseButtonMinWidthUnselected) << "f;\n";
    file << "    style.TabBarBorderSize = "
         << format_float(style.TabBarBorderSize) << "f;\n";
    file << "    style.TabBarOverlineSize = "
         << format_float(style.TabBarOverlineSize) << "f;\n\n";

    file << "    // Table\n";
    file << "    style.TableAngledHeadersAngle = "
         << format_float(style.TableAngledHeadersAngle) << "f;\n";
    file << "    style.TableAngledHeadersTextAlign = ImVec2("
         << format_float(style.TableAngledHeadersTextAlign.x) << "f, "
         << format_float(style.TableAngledHeadersTextAlign.y) << "f);\n\n";

    file << "    // Tree lines\n";
    file << "    style.TreeLinesFlags = "
         << (style.TreeLinesFlags & ImGuiTreeNodeFlags_DrawLinesNone
                 ? "ImGuiTreeNodeFlags_DrawLinesNone"
             : style.TreeLinesFlags & ImGuiTreeNodeFlags_DrawLinesFull
                 ? "ImGuiTreeNodeFlags_DrawLinesFull"
                 : "ImGuiTreeNodeFlags_DrawLinesToNodes")
         << ";\n";
    file << "    style.TreeLinesSize = " << format_float(style.TreeLinesSize)
         << "f;\n";
    file << "    style.TreeLinesRounding = "
         << format_float(style.TreeLinesRounding) << "f;\n\n";

    file << "    // Color button\n";
    file << "    style.ColorButtonPosition = ImGuiDir_"
         << (style.ColorButtonPosition == ImGuiDir_Left ? "Left" : "Right")
         << ";\n\n";

    file << "    // Text alignment\n";
    file << "    style.ButtonTextAlign = ImVec2("
         << format_float(style.ButtonTextAlign.x) << "f, "
         << format_float(style.ButtonTextAlign.y) << "f);\n";
    file << "    style.SelectableTextAlign = ImVec2("
         << format_float(style.SelectableTextAlign.x) << "f, "
         << format_float(style.SelectableTextAlign.y) << "f);\n\n";

    file << "    // Separator text\n";
    file << "    style.SeparatorTextBorderSize = "
         << format_float(style.SeparatorTextBorderSize) << "f;\n";
    file << "    style.SeparatorTextAlign = ImVec2("
         << format_float(style.SeparatorTextAlign.x) << "f, "
         << format_float(style.SeparatorTextAlign.y) << "f);\n";
    file << "    style.SeparatorTextPadding = ImVec2("
         << format_float(style.SeparatorTextPadding.x) << "f, "
         << format_float(style.SeparatorTextPadding.y) << "f);\n\n";

    file << "    // Display\n";
    file << "    style.DisplayWindowPadding = ImVec2("
         << format_float(style.DisplayWindowPadding.x) << "f, "
         << format_float(style.DisplayWindowPadding.y) << "f);\n";
    file << "    style.DisplaySafeAreaPadding = ImVec2("
         << format_float(style.DisplaySafeAreaPadding.x) << "f, "
         << format_float(style.DisplaySafeAreaPadding.y) << "f);\n";
    file << "    style.MouseCursorScale = "
         << format_float(style.MouseCursorScale) << "f;\n\n";

    file << "    // Anti-aliasing\n";
    file << "    style.AntiAliasedLines = "
         << (style.AntiAliasedLines ? "true" : "false") << ";\n";
    file << "    style.AntiAliasedLinesUseTex = "
         << (style.AntiAliasedLinesUseTex ? "true" : "false") << ";\n";
    file << "    style.AntiAliasedFill = "
         << (style.AntiAliasedFill ? "true" : "false") << ";\n\n";

    file << "    // Tessellation\n";
    file << "    style.CurveTessellationTol = "
         << format_float(style.CurveTessellationTol) << "f;\n";
    file << "    style.CircleTessellationMaxError = "
         << format_float(style.CircleTessellationMaxError) << "f;\n\n";

    file << "    // Hover behaviors\n";
    file << "    style.HoverStationaryDelay = "
         << format_float(style.HoverStationaryDelay) << "f;\n";
    file << "    style.HoverDelayShort = "
         << format_float(style.HoverDelayShort) << "f;\n";
    file << "    style.HoverDelayNormal = "
         << format_float(style.HoverDelayNormal) << "f;\n\n";

    file << "    // Colors\n";
    for (int i = 0; i < ImGuiCol_COUNT; i++) {
        ImGuiCol col = (ImGuiCol)i;
        const char *col_name = get_color_name(col);
        ImVec4 &col_value = style.Colors[i];
        file << "    style.Colors[ImGuiCol_" << col_name << "] = ImVec4("
             << format_float(col_value.x) << "f, " << format_float(col_value.y)
             << "f, " << format_float(col_value.z) << "f, "
             << format_float(col_value.w) << "f);\n";
    }

    file << "}\n";

    file.close();
    LOG_INFO("Style exported successfully to: " + filepath);
}

const char *StyleEditorUI::get_color_name(ImGuiCol col) {
    switch (col) {
    case ImGuiCol_Text:
        return "Text";
    case ImGuiCol_TextDisabled:
        return "TextDisabled";
    case ImGuiCol_WindowBg:
        return "WindowBg";
    case ImGuiCol_ChildBg:
        return "ChildBg";
    case ImGuiCol_PopupBg:
        return "PopupBg";
    case ImGuiCol_Border:
        return "Border";
    case ImGuiCol_BorderShadow:
        return "BorderShadow";
    case ImGuiCol_FrameBg:
        return "FrameBg";
    case ImGuiCol_FrameBgHovered:
        return "FrameBgHovered";
    case ImGuiCol_FrameBgActive:
        return "FrameBgActive";
    case ImGuiCol_TitleBg:
        return "TitleBg";
    case ImGuiCol_TitleBgActive:
        return "TitleBgActive";
    case ImGuiCol_TitleBgCollapsed:
        return "TitleBgCollapsed";
    case ImGuiCol_MenuBarBg:
        return "MenuBarBg";
    case ImGuiCol_ScrollbarBg:
        return "ScrollbarBg";
    case ImGuiCol_ScrollbarGrab:
        return "ScrollbarGrab";
    case ImGuiCol_ScrollbarGrabHovered:
        return "ScrollbarGrabHovered";
    case ImGuiCol_ScrollbarGrabActive:
        return "ScrollbarGrabActive";
    case ImGuiCol_CheckMark:
        return "CheckMark";
    case ImGuiCol_SliderGrab:
        return "SliderGrab";
    case ImGuiCol_SliderGrabActive:
        return "SliderGrabActive";
    case ImGuiCol_Button:
        return "Button";
    case ImGuiCol_ButtonHovered:
        return "ButtonHovered";
    case ImGuiCol_ButtonActive:
        return "ButtonActive";
    case ImGuiCol_Header:
        return "Header";
    case ImGuiCol_HeaderHovered:
        return "HeaderHovered";
    case ImGuiCol_HeaderActive:
        return "HeaderActive";
    case ImGuiCol_Separator:
        return "Separator";
    case ImGuiCol_SeparatorHovered:
        return "SeparatorHovered";
    case ImGuiCol_SeparatorActive:
        return "SeparatorActive";
    case ImGuiCol_ResizeGrip:
        return "ResizeGrip";
    case ImGuiCol_ResizeGripHovered:
        return "ResizeGripHovered";
    case ImGuiCol_ResizeGripActive:
        return "ResizeGripActive";
    case ImGuiCol_InputTextCursor:
        return "InputTextCursor";
    case ImGuiCol_TabHovered:
        return "TabHovered";
    case ImGuiCol_Tab:
        return "Tab";
    case ImGuiCol_TabSelected:
        return "TabSelected";
    case ImGuiCol_TabSelectedOverline:
        return "TabSelectedOverline";
    case ImGuiCol_TabDimmed:
        return "TabDimmed";
    case ImGuiCol_TabDimmedSelected:
        return "TabDimmedSelected";
    case ImGuiCol_TabDimmedSelectedOverline:
        return "TabDimmedSelectedOverline";
    case ImGuiCol_PlotLines:
        return "PlotLines";
    case ImGuiCol_PlotLinesHovered:
        return "PlotLinesHovered";
    case ImGuiCol_PlotHistogram:
        return "PlotHistogram";
    case ImGuiCol_PlotHistogramHovered:
        return "PlotHistogramHovered";
    case ImGuiCol_TableHeaderBg:
        return "TableHeaderBg";
    case ImGuiCol_TableBorderStrong:
        return "TableBorderStrong";
    case ImGuiCol_TableBorderLight:
        return "TableBorderLight";
    case ImGuiCol_TableRowBg:
        return "TableRowBg";
    case ImGuiCol_TableRowBgAlt:
        return "TableRowBgAlt";
    case ImGuiCol_TextLink:
        return "TextLink";
    case ImGuiCol_TextSelectedBg:
        return "TextSelectedBg";
    case ImGuiCol_TreeLines:
        return "TreeLines";
    case ImGuiCol_DragDropTarget:
        return "DragDropTarget";
    case ImGuiCol_NavCursor:
        return "NavCursor";
    case ImGuiCol_NavWindowingHighlight:
        return "NavWindowingHighlight";
    case ImGuiCol_NavWindowingDimBg:
        return "NavWindowingDimBg";
    case ImGuiCol_ModalWindowDimBg:
        return "ModalWindowDimBg";
    default:
        return "Unknown";
    }
}
