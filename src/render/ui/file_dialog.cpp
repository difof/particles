#include <algorithm>
#include <cstring>
#include <filesystem>
#include <tinydir.h>

#include "file_dialog.hpp"
#include "misc/cpp/imgui_stdlib.h"

/**
 * @brief Normalize directory path by ensuring it ends with a slash
 * @param path directory path to normalize
 * @return normalized directory path
 */
static std::string normalize_dir(const std::string &path) {
    if (path.empty()) {
        return std::string(".");
    }
    if (path.back() == '/') {
        return path;
    }
    return path + "/";
}

void FileDialog::open(Mode mode, const std::string &title,
                      const std::string &start_dir) {
    m_mode = mode;
    m_title = title;
    m_open = true;
    m_has_result = false;
    m_canceled = false;
    m_selected_path.clear();

    if (!start_dir.empty()) {
        m_current_dir = start_dir;
    } else {
        m_current_dir = ".";
    }
    ensure_current_dir();
    list_directory();
}

bool FileDialog::render() {
    if (!m_open) {
        return false;
    }

    bool closed_this_frame = false;

    ImGui::OpenPopup(m_title.c_str());
    if (ImGui::BeginPopupModal(m_title.c_str(), nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Directory:");
        ImGui::SameLine();
        ImGui::TextUnformatted(m_current_dir.c_str());

        if (ImGui::Button("Up")) {
            go_up_dir();
            list_directory();
        }
        ImGui::SameLine();
        if (ImGui::Button("Refresh")) {
            list_directory();
        }

        ImGui::Separator();

        // List entries
        ImGui::BeginChild("##fd_entries", ImVec2(600, 300), true);
        for (const auto &e : m_entries) {
            if (e.is_dir) {
                if (ImGui::Selectable(("[DIR] " + e.name).c_str(), false)) {
                    enter_dir(e.name);
                    list_directory();
                }
            } else {
                if (ImGui::Selectable(e.name.c_str(), false)) {
                    m_file_name = e.name;
                }
            }
        }
        ImGui::EndChild();

        ImGui::Separator();

        ImGui::InputText("File name", &m_file_name);

        if (ImGui::BeginTable("##fd_actions", 3)) {
            ImGui::TableNextColumn();
            if (ImGui::Button("New Folder")) {
                m_new_dir_name.clear();
                ImGui::OpenPopup("##new_folder");
            }
            if (ImGui::BeginPopup("##new_folder")) {
                ImGui::InputText("Name", &m_new_dir_name);
                if (ImGui::Button("Create")) {
                    if (!m_new_dir_name.empty()) {
                        std::string path =
                            normalize_dir(m_current_dir) + m_new_dir_name;
                        std::error_code ec;
                        std::filesystem::create_directory(path, ec);
                        list_directory();
                    }
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel##mkd")) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            ImGui::TableNextColumn();
            if (ImGui::Button(m_mode == Mode::Open ? "Open" : "Save")) {
                std::string base = normalize_dir(m_current_dir);
                m_selected_path = base + m_file_name;
                m_has_result = true;
                m_canceled = false;
                m_open = false;
                ImGui::CloseCurrentPopup();
                closed_this_frame = true;
            }

            ImGui::TableNextColumn();
            if (ImGui::Button("Cancel")) {
                m_has_result = false;
                m_canceled = true;
                m_open = false;
                ImGui::CloseCurrentPopup();
                closed_this_frame = true;
            }
            ImGui::EndTable();
        }

        ImGui::EndPopup();
    }

    return closed_this_frame;
}

void FileDialog::ensure_current_dir() {
    // Ensure path ends without trailing file part; keep as-is since
    // tinydir_open resolves it
}

void FileDialog::list_directory() {
    m_entries.clear();

    tinydir_dir dir;
    if (tinydir_open(&dir, m_current_dir.c_str()) == -1) {
        return;
    }

    while (dir.has_next) {
        tinydir_file file;
        if (tinydir_readfile(&dir, &file) == -1) {
            break;
        }
        tinydir_next(&dir);

        std::string name = file.name;
        if (name == "." || name == "..") {
            continue;
        }
        Entry e;
        e.name = name;
        e.is_dir = file.is_dir != 0;
        m_entries.push_back(e);
    }

    tinydir_close(&dir);

    std::sort(m_entries.begin(), m_entries.end(),
              [](const Entry &a, const Entry &b) {
                  if (a.is_dir != b.is_dir)
                      return a.is_dir && !b.is_dir;
                  return a.name < b.name;
              });
}

void FileDialog::go_up_dir() {
    if (m_current_dir.empty()) {
        return;
    }
    // Remove trailing slash if any
    std::string path = m_current_dir;
    if (!path.empty() && path.back() == '/') {
        path.pop_back();
    }
    auto pos = path.find_last_of('/');
    if (pos == std::string::npos) {
        m_current_dir = ".";
    } else if (pos == 0) {
        m_current_dir = "/";
    } else {
        m_current_dir = path.substr(0, pos);
    }
}

void FileDialog::enter_dir(const std::string &name) {
    if (name.empty()) {
        return;
    }
    if (m_current_dir.empty() || m_current_dir == "/") {
        m_current_dir = "/" + name;
    } else {
        m_current_dir = normalize_dir(m_current_dir) + name;
    }
}
