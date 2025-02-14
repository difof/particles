#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <raylib.h>

#include "mailbox/mailbox.hpp"
#include "render/types/config.hpp"
#include "utility/exceptions.hpp"
#include "utility/logger.hpp"

using json = nlohmann::json;

/**
 * @brief Manages saving and loading of particle simulation projects and
 * configuration.
 *
 * Handles serialization of project data including simulation configuration,
 * render settings, particle seeds, and window state. Provides recent files
 * management and auto-load functionality.
 */
class SaveManager {
  public:
    /**
     * @brief Complete project data structure containing all simulation state.
     */
    struct ProjectData {
        /** @brief Current simulation configuration snapshot */
        mailbox::SimulationConfigSnapshot sim_config;

        /** @brief Current render configuration */
        Config render_config;

        /** @brief Particle seed specification for reproducible simulations */
        std::shared_ptr<mailbox::command::SeedSpec> seed;

        /**
         * @brief Window configuration parameters.
         */
        struct WindowConfig {
            /** @brief Screen width in pixels */
            int screen_width;
            /** @brief Screen height in pixels */
            int screen_height;
            /** @brief Control panel width in pixels */
            int panel_width;
            /** @brief Render area width in pixels */
            int render_width;
        } window_config;
    };

    /**
     * @brief Window state for persistence across application sessions.
     */
    struct WindowState {
        /** @brief Window width in pixels */
        int width = 1080;
        /** @brief Window height in pixels */
        int height = 800;
        /** @brief Window X position in pixels */
        int x = 0;
        /** @brief Window Y position in pixels */
        int y = 0;
    };

    SaveManager();
    ~SaveManager() = default;

    // Delete copy and move semantics
    SaveManager(const SaveManager &) = delete;
    SaveManager &operator=(const SaveManager &) = delete;
    SaveManager(SaveManager &&) = delete;
    SaveManager &operator=(SaveManager &&) = delete;

    /**
     * @brief Save project data to specified file path.
     * @param filepath Path where to save the project file
     * @param data Project data to serialize and save
     * @throws SaveException if file operations fail
     */
    void save_project(const std::string &filepath, const ProjectData &data);

    /**
     * @brief Load project data from specified file path.
     * @param filepath Path to the project file to load
     * @param data Project data structure to populate with loaded data
     * @throws LoadException if file operations or parsing fail
     */
    void load_project(const std::string &filepath, ProjectData &data);

    /**
     * @brief Initialize new project with default values.
     * @param data Project data structure to initialize with defaults
     */
    void new_project(ProjectData &data);

    /**
     * @brief Extract current particle seed from world snapshot.
     * @param world_snapshot Current world state snapshot
     * @return Shared pointer to seed specification
     */
    std::shared_ptr<mailbox::command::SeedSpec>
    extract_current_seed(const mailbox::WorldSnapshot &world_snapshot);

    /**
     * @brief Convert Color to JSON representation.
     * @param color Raylib Color to serialize
     * @return JSON object containing color data
     */
    json color_to_json(const Color &color);

    /**
     * @brief Convert JSON to Color object.
     * @param j JSON object containing color data
     * @return Deserialized Color object
     */
    Color json_to_color(const json &j);

    /**
     * @brief Add file path to recent files list.
     * @param filepath Path to add to recent files
     */
    void add_to_recent(const std::string &filepath);

    /**
     * @brief Get list of recently opened files.
     * @return Vector of recent file paths
     */
    std::vector<std::string> get_recent_files() const;

    /**
     * @brief Clear all recent files from the list.
     */
    void clear_recent_files();

    /**
     * @brief Get the last opened file path.
     * @return Path to the last opened file, empty if none
     */
    std::string get_last_opened_file() const;

    /**
     * @brief Set the last opened file path.
     * @param filepath Path to set as last opened file
     */
    void set_last_opened_file(const std::string &filepath);

    /**
     * @brief Save window state for persistence.
     * @param state Window state to save
     */
    void save_window_state(const WindowState &state);

    /**
     * @brief Load previously saved window state.
     * @return Window state loaded from configuration
     */
    WindowState load_window_state() const;

  private:
    /**
     * @brief Convert seed specification to JSON.
     * @param seed Seed specification to serialize
     * @return JSON object containing seed data
     */
    json seed_to_json(const std::shared_ptr<mailbox::command::SeedSpec> &seed);

    /**
     * @brief Convert JSON to seed specification.
     * @param j JSON object containing seed data
     * @return Deserialized seed specification
     */
    std::shared_ptr<mailbox::command::SeedSpec> json_to_seed(const json &j);

    /**
     * @brief Convert simulation configuration to JSON.
     * @param config Simulation configuration to serialize
     * @return JSON object containing simulation config data
     */
    json sim_config_to_json(const mailbox::SimulationConfigSnapshot &config);

    /**
     * @brief Convert JSON to simulation configuration.
     * @param j JSON object containing simulation config data
     * @return Deserialized simulation configuration
     */
    mailbox::SimulationConfigSnapshot json_to_sim_config(const json &j);

    /**
     * @brief Convert render configuration to JSON.
     * @param config Render configuration to serialize
     * @return JSON object containing render config data
     */
    json render_config_to_json(const Config &config);

    /**
     * @brief Convert JSON to render configuration.
     * @param j JSON object containing render config data
     * @return Deserialized render configuration
     */
    Config json_to_render_config(const json &j);

    /**
     * @brief Convert window configuration to JSON.
     * @param config Window configuration to serialize
     * @return JSON object containing window config data
     */
    json window_config_to_json(const ProjectData::WindowConfig &config);

    /**
     * @brief Convert JSON to window configuration.
     * @param j JSON object containing window config data
     * @return Deserialized window configuration
     */
    ProjectData::WindowConfig json_to_window_config(const json &j);

    /**
     * @brief Get path to configuration file.
     * @return Full path to the configuration file
     */
    std::string get_config_path() const;

    /**
     * @brief Save current configuration to file.
     */
    void save_config();

    /**
     * @brief Load configuration from file.
     */
    void load_config();

    /** @brief List of recently opened file paths */
    std::vector<std::string> m_recent_files;

    /** @brief Path to the last opened file */
    std::string m_last_file;

    /** @brief Maximum number of recent files to keep */
    static constexpr int MAX_RECENT_FILES = 10;

    /** @brief JSON key for recent files array */
    static constexpr const char *RECENT_FILES_KEY = "recent_files";

    /** @brief JSON key for last opened file */
    static constexpr const char *LAST_FILE_KEY = "last_file";

    /** @brief JSON key for window state */
    static constexpr const char *WINDOW_STATE_KEY = "window_state";

    /** @brief Configuration file name */
    static constexpr const char *CONFIG_FILE = "particles_config.json";
};
