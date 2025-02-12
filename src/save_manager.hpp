#pragma once

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <raylib.h>
#include <string>
#include <vector>

#include "mailbox/mailbox.hpp"
#include "render/types/config.hpp"
#include "utility/exceptions.hpp"
#include "utility/logger.hpp"

using json = nlohmann::json;

class SaveManager {
  public:
    struct ProjectData {
        // Simulation config
        mailbox::SimulationConfigSnapshot sim_config;

        // Render config
        Config render_config;

        // Particle seed data
        std::shared_ptr<mailbox::command::SeedSpec> seed;

        // Window config
        struct WindowConfig {
            int screen_width, screen_height, panel_width, render_width;
        } window_config;
    };

    SaveManager();
    ~SaveManager() = default;

    // File operations - now throw exceptions instead of returning bool
    void save_project(const std::string &filepath, const ProjectData &data);
    void load_project(const std::string &filepath, ProjectData &data);
    void new_project(ProjectData &data);

    // Extract current world state
    std::shared_ptr<mailbox::command::SeedSpec>
    extract_current_seed(const mailbox::WorldSnapshot &world_snapshot);

    // JSON serialization helpers (public for testing)
    json color_to_json(const Color &color);
    Color json_to_color(const json &j);

    // Recent files management
    void add_to_recent(const std::string &filepath);
    std::vector<std::string> get_recent_files() const;
    void clear_recent_files();

    // Auto-load functionality
    std::string get_last_opened_file() const;
    void set_last_opened_file(const std::string &filepath);

    // Window state persistence
    struct WindowState {
        int width = 1080;
        int height = 800;
        int x = 0;
        int y = 0;
    };
    void save_window_state(const WindowState &state);
    WindowState load_window_state() const;

  private:
    // JSON serialization helpers (moved to public for testing)

    json seed_to_json(const std::shared_ptr<mailbox::command::SeedSpec> &seed);
    std::shared_ptr<mailbox::command::SeedSpec> json_to_seed(const json &j);

    json sim_config_to_json(const mailbox::SimulationConfigSnapshot &config);
    mailbox::SimulationConfigSnapshot json_to_sim_config(const json &j);

    json render_config_to_json(const Config &config);
    Config json_to_render_config(const json &j);

    json window_config_to_json(const ProjectData::WindowConfig &config);
    ProjectData::WindowConfig json_to_window_config(const json &j);

    // Recent files storage
    static constexpr const char *RECENT_FILES_KEY = "recent_files";
    static constexpr const char *LAST_FILE_KEY = "last_file";
    static constexpr const char *WINDOW_STATE_KEY = "window_state";
    static constexpr const char *CONFIG_FILE = "particles_config.json";

    std::string get_config_path() const;
    void save_config();
    void load_config();

    std::vector<std::string> m_recent_files;
    std::string m_last_file;
    static constexpr int MAX_RECENT_FILES = 10;
};
