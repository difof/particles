#include <algorithm>
#include <iostream>

#ifndef _WIN32
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "save_manager.hpp"
#include "utility/default_seed.hpp"
#include "utility/logger.hpp"

SaveManager::SaveManager() { load_config(); }

void SaveManager::save_project(const std::string &filepath,
                               const ProjectData &data) {
    LOG_INFO("Saving project to: " + filepath);

    try {
        json j;

        j["simulation"] = sim_config_to_json(data.sim_config);
        j["render"] = render_config_to_json(data.render_config);

        if (data.seed) {
            j["seed"] = seed_to_json(data.seed);
        }

        j["window"] = window_config_to_json(data.window_config);

        std::ofstream file(filepath);
        if (!file.is_open()) {
            throw particles::IOError("Failed to open file for writing: " +
                                     filepath);
        }

        file << j.dump(2);
        file.close();

        add_to_recent(filepath);
        set_last_opened_file(filepath);

        LOG_INFO("Project saved successfully");
    } catch (const particles::IOError &) {
        throw;
    } catch (const std::exception &e) {
        LOG_ERROR("JSON serialization error: " + std::string(e.what()));
        throw particles::IOError("JSON serialization failed: " +
                                 std::string(e.what()));
    }
}

void SaveManager::load_project(const std::string &filepath, ProjectData &data) {
    LOG_INFO("Loading project from: " + filepath);

    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            throw particles::IOError("Failed to open file for reading: " +
                                     filepath);
        }

        json j;
        file >> j;
        file.close();

        if (j.contains("simulation")) {
            data.sim_config = json_to_sim_config(j["simulation"]);
        }

        if (j.contains("render")) {
            data.render_config = json_to_render_config(j["render"]);
        }

        if (j.contains("seed")) {
            data.seed = json_to_seed(j["seed"]);
        }

        if (j.contains("window")) {
            data.window_config = json_to_window_config(j["window"]);
        }

        add_to_recent(filepath);
        set_last_opened_file(filepath);

        LOG_INFO("Project loaded successfully");
    } catch (const particles::IOError &) {
        throw;
    } catch (const std::exception &e) {
        LOG_ERROR("JSON parsing error: " + std::string(e.what()));
        throw particles::IOError("JSON parsing failed: " +
                                 std::string(e.what()));
    }
}

void SaveManager::new_project(ProjectData &data) {
    LOG_INFO("Creating new project");
    data.sim_config = {};
    data.sim_config.bounds_width = 1080.0f;
    data.sim_config.bounds_height = 800.0f;
    data.sim_config.target_tps = 0;
    data.sim_config.time_scale = 1.0f;
    data.sim_config.viscosity = 0.271f;
    data.sim_config.wall_repel = 86.0f;
    data.sim_config.wall_strength = 0.129f;
    data.sim_config.sim_threads = -1;

    data.render_config = {};
    data.render_config.interpolate = true;
    data.render_config.core_size = 1.5f;
    data.render_config.glow_enabled = true;
    data.render_config.outer_scale_mul = 24.f;
    data.render_config.outer_rgb_gain = .78f;
    data.render_config.inner_scale_mul = 1.f;
    data.render_config.inner_rgb_gain = .52f;

    data.seed = particles::utility::create_default_seed();

    data.window_config = {1080, 800, 500, 1080};

    LOG_INFO("New project created successfully");
}

std::shared_ptr<mailbox::command::SeedSpec> SaveManager::extract_current_seed(
    const mailbox::WorldSnapshot &world_snapshot) {
    auto seed = std::make_shared<mailbox::command::SeedSpec>();

    const int G = world_snapshot.get_groups_size();
    if (G == 0) {
        return nullptr;
    }

    seed->sizes.clear();
    for (int g = 0; g < G; ++g) {
        seed->sizes.push_back(world_snapshot.get_group_size(g));
    }

    seed->colors.clear();
    for (int g = 0; g < G; ++g) {
        seed->colors.push_back(world_snapshot.get_group_color(g));
    }

    seed->r2.clear();
    for (int g = 0; g < G; ++g) {
        seed->r2.push_back(world_snapshot.r2_of(g));
    }

    seed->rules.clear();
    for (int gsrc = 0; gsrc < G; ++gsrc) {
        for (int gdst = 0; gdst < G; ++gdst) {
            seed->rules.push_back(world_snapshot.rule_val(gsrc, gdst));
        }
    }

    seed->enabled.clear();
    for (int g = 0; g < G; ++g) {
        seed->enabled.push_back(world_snapshot.is_group_enabled(g));
    }

    return seed;
}

void SaveManager::add_to_recent(const std::string &filepath) {
    auto it = std::find(m_recent_files.begin(), m_recent_files.end(), filepath);
    if (it != m_recent_files.end()) {
        m_recent_files.erase(it);
    }

    m_recent_files.insert(m_recent_files.begin(), filepath);

    if (m_recent_files.size() > MAX_RECENT_FILES) {
        m_recent_files.resize(MAX_RECENT_FILES);
    }

    save_config();
}

std::vector<std::string> SaveManager::get_recent_files() const {
    return m_recent_files;
}

void SaveManager::clear_recent_files() {
    m_recent_files.clear();
    save_config();
}

std::string SaveManager::get_last_opened_file() const { return m_last_file; }

void SaveManager::set_last_opened_file(const std::string &filepath) {
    m_last_file = filepath;
    save_config();
}

json SaveManager::color_to_json(const Color &color) {
    return json{{"r", color.r}, {"g", color.g}, {"b", color.b}, {"a", color.a}};
}

Color SaveManager::json_to_color(const json &j) {
    return Color{static_cast<unsigned char>(j["r"].get<int>()),
                 static_cast<unsigned char>(j["g"].get<int>()),
                 static_cast<unsigned char>(j["b"].get<int>()),
                 static_cast<unsigned char>(j["a"].get<int>())};
}

json SaveManager::seed_to_json(
    const std::shared_ptr<mailbox::command::SeedSpec> &seed) {
    if (!seed) {
        return json::object();
    }

    json j;

    const std::size_t group_count =
        std::min({seed->sizes.size(), seed->colors.size(), seed->r2.size(),
                  seed->enabled.size()});
    j["groups"] = json::array();
    for (std::size_t g = 0; g < group_count; ++g) {
        json group;
        group["size"] = seed->sizes[g];
        group["color"] = color_to_json(seed->colors[g]);
        group["r2"] = seed->r2[g];
        group["enabled"] = seed->enabled[g];

        const std::size_t row_start = g * group_count;
        json rules_row = json::array();
        for (std::size_t d = 0; d < group_count; ++d) {
            const std::size_t idx = row_start + d;
            rules_row.push_back((idx < seed->rules.size()) ? seed->rules[idx]
                                                           : 0.0f);
        }
        group["rules"] = rules_row;

        j["groups"].push_back(group);
    }

    return j;
}

std::shared_ptr<mailbox::command::SeedSpec>
SaveManager::json_to_seed(const json &j) {
    auto seed = std::make_shared<mailbox::command::SeedSpec>();

    if (j.contains("groups")) {
        const auto &groups = j["groups"];
        seed->sizes.clear();
        seed->colors.clear();
        seed->r2.clear();
        seed->enabled.clear();
        seed->rules.clear();

        for (const auto &g : groups) {
            if (g.contains("size")) {
                seed->sizes.push_back(g["size"].get<int>());
            }
            if (g.contains("color")) {
                seed->colors.push_back(json_to_color(g["color"]));
            }
            if (g.contains("r2")) {
                seed->r2.push_back(g["r2"].get<float>());
            }
            if (g.contains("enabled")) {
                seed->enabled.push_back(g["enabled"].get<bool>());
            }
        }

        const std::size_t G = groups.size();
        seed->rules.resize(G * G, 0.0f);
        for (std::size_t i = 0; i < G; ++i) {
            const auto &gi = groups[i];
            if (gi.contains("rules") && gi["rules"].is_array()) {
                const auto &row = gi["rules"];
                const std::size_t cols = std::min<std::size_t>(row.size(), G);
                for (std::size_t jcol = 0; jcol < cols; ++jcol) {
                    seed->rules[i * G + jcol] = row[jcol].get<float>();
                }
            }
        }

        return seed;
    }

    // Legacy flat arrays fallback
    if (j.contains("sizes")) {
        seed->sizes = j["sizes"].get<std::vector<int>>();
    }

    if (j.contains("colors")) {
        seed->colors.clear();
        for (const auto &color_json : j["colors"]) {
            seed->colors.push_back(json_to_color(color_json));
        }
    }

    if (j.contains("r2")) {
        seed->r2 = j["r2"].get<std::vector<float>>();
    }

    if (j.contains("rules")) {
        seed->rules = j["rules"].get<std::vector<float>>();
    }

    if (j.contains("enabled")) {
        seed->enabled = j["enabled"].get<std::vector<bool>>();
    }

    return seed;
}

json SaveManager::sim_config_to_json(
    const mailbox::SimulationConfigSnapshot &config) {
    return json{{"bounds_width", config.bounds_width},
                {"bounds_height", config.bounds_height},
                {"time_scale", config.time_scale},
                {"viscosity", config.viscosity},
                {"wall_repel", config.wall_repel},
                {"wall_strength", config.wall_strength},
                {"gravity_x", config.gravity_x},
                {"gravity_y", config.gravity_y},
                {"target_tps", config.target_tps},
                {"sim_threads", config.sim_threads},
                {"draw_report", {{"grid_data", config.draw_report.grid_data}}}};
}

mailbox::SimulationConfigSnapshot
SaveManager::json_to_sim_config(const json &j) {
    mailbox::SimulationConfigSnapshot config = {};

    if (j.contains("bounds_width")) {
        config.bounds_width = j["bounds_width"];
    }
    if (j.contains("bounds_height")) {
        config.bounds_height = j["bounds_height"];
    }
    if (j.contains("time_scale")) {
        config.time_scale = j["time_scale"];
    }
    if (j.contains("viscosity")) {
        config.viscosity = j["viscosity"];
    }
    if (j.contains("wall_repel")) {
        config.wall_repel = j["wall_repel"];
    }
    if (j.contains("wall_strength")) {
        config.wall_strength = j["wall_strength"];
    }
    if (j.contains("gravity_x")) {
        config.gravity_x = j["gravity_x"];
    }
    if (j.contains("gravity_y")) {
        config.gravity_y = j["gravity_y"];
    }
    if (j.contains("target_tps")) {
        config.target_tps = j["target_tps"];
    }
    if (j.contains("sim_threads")) {
        config.sim_threads = j["sim_threads"];
    }
    if (j.contains("draw_report") && j["draw_report"].contains("grid_data")) {
        config.draw_report.grid_data = j["draw_report"]["grid_data"];
    }

    return config;
}

json SaveManager::render_config_to_json(const Config &config) {
    return json{{"show_ui", config.show_ui},
                {"show_metrics_ui", config.show_metrics_ui},
                {"show_editor", config.show_editor},
                {"show_render_config", config.show_render_config},
                {"show_sim_config", config.show_sim_config},
                {"interpolate", config.interpolate},
                {"interp_delay_ms", config.interp_delay_ms},
                {"glow_enabled", config.glow_enabled},
                {"core_size", config.core_size},
                {"outer_scale_mul", config.outer_scale_mul},
                {"outer_rgb_gain", config.outer_rgb_gain},
                {"inner_scale_mul", config.inner_scale_mul},
                {"inner_rgb_gain", config.inner_rgb_gain},
                {"final_additive_blit", config.final_additive_blit},
                {"background_color", color_to_json(config.background_color)},
                {"show_density_heat", config.show_density_heat},
                {"heat_alpha", config.heat_alpha},
                {"show_velocity_field", config.show_velocity_field},
                {"vel_scale", config.vel_scale},
                {"vel_thickness", config.vel_thickness},
                {"show_grid_lines", config.show_grid_lines},
                {"camera",
                 {{"x", config.camera.x},
                  {"y", config.camera.y},
                  {"zoom_log", config.camera.zoom_log}}}};
}

Config SaveManager::json_to_render_config(const json &j) {
    Config config = {};

    if (j.contains("show_ui")) {
        config.show_ui = j["show_ui"];
    }
    if (j.contains("show_metrics_ui")) {
        config.show_metrics_ui = j["show_metrics_ui"];
    }
    if (j.contains("show_editor")) {
        config.show_editor = j["show_editor"];
    }
    if (j.contains("show_render_config")) {
        config.show_render_config = j["show_render_config"];
    }
    if (j.contains("show_sim_config")) {
        config.show_sim_config = j["show_sim_config"];
    }
    if (j.contains("interpolate")) {
        config.interpolate = j["interpolate"];
    }
    if (j.contains("interp_delay_ms")) {
        config.interp_delay_ms = j["interp_delay_ms"];
    }
    if (j.contains("glow_enabled")) {
        config.glow_enabled = j["glow_enabled"];
    }
    if (j.contains("core_size")) {
        config.core_size = j["core_size"];
    }
    if (j.contains("outer_scale_mul")) {
        config.outer_scale_mul = j["outer_scale_mul"];
    }
    if (j.contains("outer_rgb_gain")) {
        config.outer_rgb_gain = j["outer_rgb_gain"];
    }
    if (j.contains("inner_scale_mul")) {
        config.inner_scale_mul = j["inner_scale_mul"];
    }
    if (j.contains("inner_rgb_gain")) {
        config.inner_rgb_gain = j["inner_rgb_gain"];
    }
    if (j.contains("final_additive_blit")) {
        config.final_additive_blit = j["final_additive_blit"];
    }
    if (j.contains("background_color")) {
        config.background_color = json_to_color(j["background_color"]);
    }
    if (j.contains("show_density_heat")) {
        config.show_density_heat = j["show_density_heat"];
    }
    if (j.contains("heat_alpha")) {
        config.heat_alpha = j["heat_alpha"];
    }
    if (j.contains("show_velocity_field")) {
        config.show_velocity_field = j["show_velocity_field"];
    }
    if (j.contains("vel_scale")) {
        config.vel_scale = j["vel_scale"];
    }
    if (j.contains("vel_thickness")) {
        config.vel_thickness = j["vel_thickness"];
    }
    if (j.contains("show_grid_lines")) {
        config.show_grid_lines = j["show_grid_lines"];
    }

    if (j.contains("camera")) {
        const auto &camera = j["camera"];
        if (camera.contains("x")) {
            config.camera.x = camera["x"];
        }
        if (camera.contains("y")) {
            config.camera.y = camera["y"];
        }
        if (camera.contains("zoom_log")) {
            config.camera.zoom_log = camera["zoom_log"];
        }
    }

    return config;
}

json SaveManager::window_config_to_json(
    const ProjectData::WindowConfig &config) {
    return json{{"screen_width", config.screen_width},
                {"screen_height", config.screen_height},
                {"panel_width", config.panel_width},
                {"render_width", config.render_width}};
}

SaveManager::ProjectData::WindowConfig
SaveManager::json_to_window_config(const json &j) {
    ProjectData::WindowConfig config = {};

    if (j.contains("screen_width")) {
        config.screen_width = j["screen_width"];
    }
    if (j.contains("screen_height")) {
        config.screen_height = j["screen_height"];
    }
    if (j.contains("panel_width")) {
        config.panel_width = j["panel_width"];
    }
    if (j.contains("render_width")) {
        config.render_width = j["render_width"];
    }

    return config;
}

static std::string get_home_directory() {
#if defined(_WIN32)
    const char *home = getenv("USERPROFILE");
    if (home && *home) {
        return std::string(home);
    }
    const char *drive = getenv("HOMEDRIVE");
    const char *path = getenv("HOMEPATH");
    if (drive && path) {
        return std::string(drive) + std::string(path);
    }
    return std::string(".");
#else
    const char *home = getenv("HOME");
    if (home && *home) {
        return std::string(home);
    }
    struct passwd *pw = getpwuid(getuid());
    if (pw && pw->pw_dir) {
        return std::string(pw->pw_dir);
    }
    return std::string(".");
#endif
}

std::string SaveManager::get_config_path() const {
    const std::string home = get_home_directory();
    return home + "/.particles/" + CONFIG_FILE;
}

void SaveManager::save_config() {
    try {
        json j;
        std::string config_path = get_config_path();
        std::ifstream file(config_path);
        if (file.is_open()) {
            file >> j;
            file.close();
        }

        j[RECENT_FILES_KEY] = m_recent_files;
        j[LAST_FILE_KEY] = m_last_file;

        std::filesystem::create_directories(
            std::filesystem::path(config_path).parent_path());

        std::ofstream out_file(config_path);
        if (out_file.is_open()) {
            out_file << j.dump(2);
            out_file.close();
        }
    } catch (const std::exception &e) {
        std::cerr << "Error saving config: " << e.what() << std::endl;
    }
}

void SaveManager::save_window_state(const WindowState &state) {
    try {
        json j;
        j["width"] = state.width;
        j["height"] = state.height;
        j["x"] = state.x;
        j["y"] = state.y;

        std::string config_path = get_config_path();
        std::filesystem::create_directories(
            std::filesystem::path(config_path).parent_path());

        json existing_config;
        std::ifstream file(config_path);
        if (file.is_open()) {
            file >> existing_config;
            file.close();
        }

        existing_config[WINDOW_STATE_KEY] = j;

        std::ofstream out_file(config_path);
        if (out_file.is_open()) {
            out_file << existing_config.dump(2);
            out_file.close();
        }
    } catch (const std::exception &e) {
        std::cerr << "Error saving window state: " << e.what() << std::endl;
    }
}

SaveManager::WindowState SaveManager::load_window_state() const {
    WindowState state;
    try {
        std::string config_path = get_config_path();
        std::ifstream file(config_path);
        if (!file.is_open()) {
            return state;
        }

        json j;
        file >> j;
        file.close();

        if (j.contains(WINDOW_STATE_KEY)) {
            const auto &ws = j[WINDOW_STATE_KEY];
            if (ws.contains("width")) {
                state.width = ws["width"];
            }
            if (ws.contains("height")) {
                state.height = ws["height"];
            }
            if (ws.contains("x")) {
                state.x = ws["x"];
            }
            if (ws.contains("y")) {
                state.y = ws["y"];
            }
        }
    } catch (const std::exception &e) {
        std::cerr << "Error loading window state: " << e.what() << std::endl;
    }
    return state;
}

void SaveManager::load_config() {
    try {
        std::string config_path = get_config_path();
        LOG_INFO("Loading config from " + config_path);

        std::ifstream file(config_path);
        if (!file.is_open()) {
            return;
        }

        json j;
        file >> j;
        file.close();

        if (j.contains(RECENT_FILES_KEY)) {
            m_recent_files =
                j[RECENT_FILES_KEY].get<std::vector<std::string>>();
        }

        if (j.contains(LAST_FILE_KEY)) {
            m_last_file = j[LAST_FILE_KEY].get<std::string>();
        }
    } catch (const std::exception &e) {
        std::cerr << "Error loading config: " << e.what() << std::endl;
    }
}
