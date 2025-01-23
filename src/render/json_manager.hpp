#ifndef __JSON_MANAGER_HPP
#define __JSON_MANAGER_HPP

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <raylib.h>
#include <string>
#include <vector>

#include "../mailbox/command.hpp"
#include "../mailbox/simconfig.hpp"
#include "../simulation/world.hpp"
#include "renderconfig.hpp"

using json = nlohmann::json;

class JsonManager {
  public:
    struct ProjectData {
        // Simulation config
        mailbox::SimulationConfig::Snapshot sim_config;

        // Render config
        RenderConfig render_config;

        // Particle seed data
        std::shared_ptr<mailbox::command::SeedSpec> seed;

        // Window config
        struct WindowConfig {
            int screen_width, screen_height, panel_width, render_width;
        } window_config;
    };

    JsonManager();
    ~JsonManager() = default;

    // File operations
    bool save_project(const std::string &filepath, const ProjectData &data);
    bool load_project(const std::string &filepath, ProjectData &data);
    bool new_project(ProjectData &data);

    // Extract current world state
    std::shared_ptr<mailbox::command::SeedSpec>
    extract_current_seed(const World &world);

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

  private:
    // JSON serialization helpers (moved to public for testing)

    json seed_to_json(const std::shared_ptr<mailbox::command::SeedSpec> &seed);
    std::shared_ptr<mailbox::command::SeedSpec> json_to_seed(const json &j);

    json sim_config_to_json(const mailbox::SimulationConfig::Snapshot &config);
    mailbox::SimulationConfig::Snapshot json_to_sim_config(const json &j);

    json render_config_to_json(const RenderConfig &config);
    RenderConfig json_to_render_config(const json &j);

    json window_config_to_json(const ProjectData::WindowConfig &config);
    ProjectData::WindowConfig json_to_window_config(const json &j);

    // Recent files storage
    static constexpr const char *RECENT_FILES_KEY = "recent_files";
    static constexpr const char *LAST_FILE_KEY = "last_file";
    static constexpr const char *CONFIG_FILE = "particles_config.json";

    std::string get_config_path() const;
    void save_config();
    void load_config();

    std::vector<std::string> m_recent_files;
    std::string m_last_file;
    static constexpr int MAX_RECENT_FILES = 10;
};

#endif
