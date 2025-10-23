#pragma once

#include <imgui.h>
#include <raylib.h>
#include <string>

#include "../../save_manager.hpp"
#include "../irenderer.hpp"
#include "../types/config.hpp"
#include "../types/window.hpp"
#include "file_dialog.hpp"

/**
 * @brief Main menu bar UI component for the particle simulator
 */
class MenuBarUI : public IRenderer {
  public:
    MenuBarUI() = default;
    ~MenuBarUI() override = default;
    MenuBarUI(const MenuBarUI &) = delete;
    MenuBarUI &operator=(const MenuBarUI &) = delete;
    MenuBarUI(MenuBarUI &&) = delete;
    MenuBarUI &operator=(MenuBarUI &&) = delete;

    /**
     * @brief Render the menu bar UI
     * @param ctx The rendering context containing simulation and UI state
     */
    void render(Context &ctx) override;

    /**
     * @brief Set the current file path for the project
     * @param filepath The path to the current project file
     */
    void set_current_filepath(const std::string &filepath);

    /**
     * @brief Trigger new project creation
     * @param ctx The rendering context
     */
    void trigger_new_project(Context &ctx);

    /**
     * @brief Trigger open project dialog
     * @param ctx The rendering context
     */
    void trigger_open_project(Context &ctx);

    /**
     * @brief Trigger save project
     * @param ctx The rendering context
     */
    void trigger_save_project(Context &ctx);

    /**
     * @brief Trigger save as project dialog
     * @param ctx The rendering context
     */
    void trigger_save_as_project(Context &ctx);

  private:
    /**
     * @brief Internal method to render the UI components
     * @param ctx The rendering context
     */
    void render_ui(Context &ctx);

    /**
     * @brief Render the project indicator button
     * @param ctx The rendering context
     */
    void render_project_indicator(Context &ctx);

    /**
     * @brief Render the File menu
     * @param ctx The rendering context
     */
    void render_file_menu(Context &ctx);

    /**
     * @brief Render the Edit menu
     * @param ctx The rendering context
     */
    void render_edit_menu(Context &ctx);

    /**
     * @brief Render the Windows menu
     * @param ctx The rendering context
     */
    void render_windows_menu(Context &ctx);

    /**
     * @brief Render the Controls menu
     * @param ctx The rendering context
     */
    void render_controls_menu(Context &ctx);

    /**
     * @brief Render the file dialog if open
     * @param ctx The rendering context
     */
    void render_file_dialog(Context &ctx);

    /**
     * @brief Handle creating a new project
     * @param ctx The rendering context
     * @throws particles::UIError if project creation fails
     */
    void handle_new_project(Context &ctx);

    /**
     * @brief Handle opening a project file dialog
     * @param ctx The rendering context
     */
    void handle_open_project(Context &ctx);

    /**
     * @brief Handle saving the current project
     * @param ctx The rendering context
     * @throws particles::UIError if project saving fails
     */
    void handle_save_project(Context &ctx);

    /**
     * @brief Handle save as project file dialog
     * @param ctx The rendering context
     */
    void handle_save_as_project(Context &ctx);

    /**
     * @brief Handle opening a specific file
     * @param ctx The rendering context
     * @param filepath The path to the file to open
     * @throws particles::UIError if file loading fails
     */
    void handle_open_file(Context &ctx, const std::string &filepath);

    /** @brief Enumeration of pending file operations */
    enum class PendingAction { None, Open, SaveAs };

    /** @brief Currently pending file operation */
    PendingAction m_pending_action = PendingAction::None;

    /** @brief Current project file path */
    std::string m_current_filepath;

    /** @brief File dialog for opening/saving projects */
    FileDialog m_file_dialog;

    /** @brief Whether the file dialog is currently open */
    bool m_file_dialog_open = false;
};
