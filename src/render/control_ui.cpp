#include "control_ui.hpp"
#include <filesystem>
#include <iostream>
#include <raylib.h>

void ControlUI::handle_new_project(RenderContext &ctx) {
    if (!m_json_manager)
        return;

    JsonManager::ProjectData data;
    if (m_json_manager->new_project(data)) {
        // Use current window/render sizes for bounds (avoid small default)
        data.sim_config.bounds_width = (float)ctx.wcfg.render_width;
        data.sim_config.bounds_height = (float)ctx.wcfg.screen_height;

        // Apply the new project data
        ctx.sim.update_config(data.sim_config);
        ctx.rcfg = data.render_config;

        // Send new seed to simulation
        if (data.seed) {
            ctx.sim.push_command(mailbox::command::SeedWorld{data.seed});
        }

        // Clear current file path (unsaved new project)
        m_current_filepath.clear();

        std::cout << "New project created" << std::endl;
    }
}

void ControlUI::handle_open_project(RenderContext &ctx) {
    if (!m_json_manager)
        return;

    if (!m_file_dialog_open) {
        m_file_dialog.set_filename("");
        m_file_dialog.open(FileDialog::Mode::Open, "Open Project");
        m_file_dialog_open = true;
        m_pending_action = PendingAction::Open;
        return;
    }
}

void ControlUI::handle_save_project(RenderContext &ctx) {
    if (!m_json_manager)
        return;

    if (m_current_filepath.empty()) {
        // No current file, do save as instead (open dialog)
        handle_save_as_project(ctx);
        return;
    }

    // Collect current state
    JsonManager::ProjectData data;
    data.sim_config = ctx.sim.get_config();
    data.render_config = ctx.rcfg;

    // Extract current seed from world
    data.seed = m_json_manager->extract_current_seed(ctx.sim.get_world());

    if (m_json_manager->save_project(m_current_filepath, data)) {
        std::cout << "Project saved to: " << m_current_filepath << std::endl;
    } else {
        std::cout << "Failed to save project to: " << m_current_filepath
                  << std::endl;
    }
}

void ControlUI::handle_save_as_project(RenderContext &ctx) {
    if (!m_json_manager)
        return;

    if (!m_file_dialog_open) {
        // Prefill filename if we have a current one
        if (!m_current_filepath.empty()) {
            // Extract filename portion
            auto pos = m_current_filepath.find_last_of('/');
            if (pos != std::string::npos && pos + 1 < m_current_filepath.size())
                m_file_dialog.set_filename(m_current_filepath.substr(pos + 1));
            else
                m_file_dialog.set_filename(m_current_filepath);
        } else {
            m_file_dialog.set_filename("project.json");
        }
        m_file_dialog.open(FileDialog::Mode::Save, "Save Project");
        m_file_dialog_open = true;
        m_pending_action = PendingAction::SaveAs;
        return;
    }
}

void ControlUI::handle_open_file(RenderContext &ctx,
                                 const std::string &filepath) {
    if (!m_json_manager)
        return;

    JsonManager::ProjectData data;
    if (m_json_manager->load_project(filepath, data)) {
        // Apply the loaded project data
        ctx.sim.update_config(data.sim_config);
        ctx.rcfg = data.render_config;

        // Send loaded seed to simulation
        if (data.seed) {
            ctx.sim.push_command(mailbox::command::SeedWorld{data.seed});
        }

        m_current_filepath = filepath;
        std::cout << "Project loaded from: " << filepath << std::endl;
    } else {
        std::cout << "Failed to load project from: " << filepath << std::endl;
    }
}
