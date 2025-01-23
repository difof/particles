#include "json_manager.hpp"
#include <algorithm>
#include <iostream>
#ifndef _WIN32
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#endif

JsonManager::JsonManager() { load_config(); }

bool JsonManager::save_project(const std::string &filepath,
                               const ProjectData &data) {
    try {
        json j;

        // Save simulation config
        j["simulation"] = sim_config_to_json(data.sim_config);

        // Save render config
        j["render"] = render_config_to_json(data.render_config);

        // Save seed data
        if (data.seed) {
            j["seed"] = seed_to_json(data.seed);
        }

        // Save window config
        j["window"] = window_config_to_json(data.window_config);

        // Write to file
        std::ofstream file(filepath);
        if (!file.is_open()) {
            return false;
        }

        file << j.dump(2); // Pretty print with 2 spaces
        file.close();

        // Add to recent files
        add_to_recent(filepath);
        set_last_opened_file(filepath);

        return true;
    } catch (const std::exception &e) {
        std::cerr << "Error saving project: " << e.what() << std::endl;
        return false;
    }
}

bool JsonManager::load_project(const std::string &filepath, ProjectData &data) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return false;
        }

        json j;
        file >> j;
        file.close();

        // Load simulation config
        if (j.contains("simulation")) {
            data.sim_config = json_to_sim_config(j["simulation"]);
        }

        // Load render config
        if (j.contains("render")) {
            data.render_config = json_to_render_config(j["render"]);
        }

        // Load seed data
        if (j.contains("seed")) {
            data.seed = json_to_seed(j["seed"]);
        }

        // Load window config
        if (j.contains("window")) {
            data.window_config = json_to_window_config(j["window"]);
        }

        // Add to recent files
        add_to_recent(filepath);
        set_last_opened_file(filepath);

        return true;
    } catch (const std::exception &e) {
        std::cerr << "Error loading project: " << e.what() << std::endl;
        return false;
    }
}

bool JsonManager::new_project(ProjectData &data) {
    // Set default values (from main.cpp hardcoded values)
    data.sim_config = {};
    data.sim_config.bounds_width = 1080.0f;
    data.sim_config.bounds_height = 800.0f;
    data.sim_config.target_tps = 0;
    data.sim_config.time_scale = 1.0f;
    data.sim_config.viscosity = 0.271f;
    data.sim_config.wall_repel = 86.0f;
    data.sim_config.wall_strength = 0.129f;
    data.sim_config.sim_threads = -1;

    // Default render config
    data.render_config = {};
    data.render_config.interpolate = true;
    data.render_config.core_size = 1.5f;
    data.render_config.glow_enabled = true;
    data.render_config.outer_scale_mul = 24.f;
    data.render_config.outer_rgb_gain = .78f;
    data.render_config.inner_scale_mul = 1.f;
    data.render_config.inner_rgb_gain = .52f;

    // Default seed (from main.cpp)
    data.seed = std::make_shared<mailbox::command::SeedSpec>();
    const int groups = 5;
    data.seed->sizes = std::vector<int>(groups, 1500);
    data.seed->colors = {(Color){0, 228, 114, 255}, (Color){238, 70, 82, 255},
                         (Color){227, 172, 72, 255}, (Color){0, 121, 241, 255},
                         (Color){200, 122, 255, 255}};
    data.seed->r2 = {80.f * 80.f, 80.f * 80.f, 96.6f * 96.6f, 80.f * 80.f,
                     80.f * 80.f};
    data.seed->rules = {
        // row 0
        +0.926f,
        -0.834f,
        +0.281f,
        -0.06427308f,
        +0.51738745f,
        // row 1
        -0.46170965f,
        +0.49142435f,
        +0.2760726f,
        +0.6413487f,
        -0.7276546f,
        // row 2
        -0.78747644f,
        +0.23373386f,
        -0.024112331f,
        -0.74875921f,
        +0.22836663f,
        // row 3
        +0.56558144f,
        +0.94846946f,
        -0.36052886f,
        +0.44114092f,
        -0.31766385f,
        // row 4
        std::sin(1.0f),
        std::cos(2.0f),
        +1.0f,
        -1.0f,
        +3.14f,
    };

    // Default window config
    data.window_config = {1080, 800, 500, 1080};

    return true;
}

std::shared_ptr<mailbox::command::SeedSpec>
JsonManager::extract_current_seed(const World &world) {
    auto seed = std::make_shared<mailbox::command::SeedSpec>();

    const int G = world.get_groups_size();
    if (G == 0) {
        return nullptr; // No groups to extract
    }

    // Extract group sizes
    seed->sizes.clear();
    for (int g = 0; g < G; ++g) {
        seed->sizes.push_back(world.get_group_size(g));
    }

    // Extract group colors
    seed->colors.clear();
    for (int g = 0; g < G; ++g) {
        seed->colors.push_back(world.get_group_color(g));
    }

    // Extract radii squared
    seed->r2.clear();
    for (int g = 0; g < G; ++g) {
        seed->r2.push_back(world.r2_of(g));
    }

    // Extract rules matrix (G*G)
    seed->rules.clear();
    for (int gsrc = 0; gsrc < G; ++gsrc) {
        for (int gdst = 0; gdst < G; ++gdst) {
            seed->rules.push_back(world.rule_val(gsrc, gdst));
        }
    }

    return seed;
}

void JsonManager::add_to_recent(const std::string &filepath) {
    // Remove if already exists
    auto it = std::find(m_recent_files.begin(), m_recent_files.end(), filepath);
    if (it != m_recent_files.end()) {
        m_recent_files.erase(it);
    }

    // Add to front
    m_recent_files.insert(m_recent_files.begin(), filepath);

    // Limit to max recent files
    if (m_recent_files.size() > MAX_RECENT_FILES) {
        m_recent_files.resize(MAX_RECENT_FILES);
    }

    save_config();
}

std::vector<std::string> JsonManager::get_recent_files() const {
    return m_recent_files;
}

void JsonManager::clear_recent_files() {
    m_recent_files.clear();
    save_config();
}

std::string JsonManager::get_last_opened_file() const { return m_last_file; }

void JsonManager::set_last_opened_file(const std::string &filepath) {
    m_last_file = filepath;
    save_config();
}

// JSON serialization helpers
json JsonManager::color_to_json(const Color &color) {
    return json{{"r", color.r}, {"g", color.g}, {"b", color.b}, {"a", color.a}};
}

Color JsonManager::json_to_color(const json &j) {
    return Color{static_cast<unsigned char>(j["r"].get<int>()),
                 static_cast<unsigned char>(j["g"].get<int>()),
                 static_cast<unsigned char>(j["b"].get<int>()),
                 static_cast<unsigned char>(j["a"].get<int>())};
}

json JsonManager::seed_to_json(
    const std::shared_ptr<mailbox::command::SeedSpec> &seed) {
    if (!seed)
        return json::object();

    json j;
    j["sizes"] = seed->sizes;
    j["colors"] = json::array();
    for (const auto &color : seed->colors) {
        j["colors"].push_back(color_to_json(color));
    }
    j["r2"] = seed->r2;
    j["rules"] = seed->rules;
    return j;
}

std::shared_ptr<mailbox::command::SeedSpec>
JsonManager::json_to_seed(const json &j) {
    auto seed = std::make_shared<mailbox::command::SeedSpec>();

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

    return seed;
}

json JsonManager::sim_config_to_json(
    const mailbox::SimulationConfig::Snapshot &config) {
    return json{{"bounds_width", config.bounds_width},
                {"bounds_height", config.bounds_height},
                {"time_scale", config.time_scale},
                {"viscosity", config.viscosity},
                {"wall_repel", config.wall_repel},
                {"wall_strength", config.wall_strength},
                {"target_tps", config.target_tps},
                {"sim_threads", config.sim_threads},
                {"draw_report", {{"grid_data", config.draw_report.grid_data}}}};
}

mailbox::SimulationConfig::Snapshot
JsonManager::json_to_sim_config(const json &j) {
    mailbox::SimulationConfig::Snapshot config = {};

    if (j.contains("bounds_width"))
        config.bounds_width = j["bounds_width"];
    if (j.contains("bounds_height"))
        config.bounds_height = j["bounds_height"];
    if (j.contains("time_scale"))
        config.time_scale = j["time_scale"];
    if (j.contains("viscosity"))
        config.viscosity = j["viscosity"];
    if (j.contains("wall_repel"))
        config.wall_repel = j["wall_repel"];
    if (j.contains("wall_strength"))
        config.wall_strength = j["wall_strength"];
    if (j.contains("target_tps"))
        config.target_tps = j["target_tps"];
    if (j.contains("sim_threads"))
        config.sim_threads = j["sim_threads"];
    if (j.contains("draw_report") && j["draw_report"].contains("grid_data")) {
        config.draw_report.grid_data = j["draw_report"]["grid_data"];
    }

    return config;
}

json JsonManager::render_config_to_json(const RenderConfig &config) {
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
                {"show_grid_lines", config.show_grid_lines}};
}

RenderConfig JsonManager::json_to_render_config(const json &j) {
    RenderConfig config = {};

    if (j.contains("show_ui"))
        config.show_ui = j["show_ui"];
    if (j.contains("show_metrics_ui"))
        config.show_metrics_ui = j["show_metrics_ui"];
    if (j.contains("show_editor"))
        config.show_editor = j["show_editor"];
    if (j.contains("show_render_config"))
        config.show_render_config = j["show_render_config"];
    if (j.contains("show_sim_config"))
        config.show_sim_config = j["show_sim_config"];
    if (j.contains("interpolate"))
        config.interpolate = j["interpolate"];
    if (j.contains("interp_delay_ms"))
        config.interp_delay_ms = j["interp_delay_ms"];
    if (j.contains("glow_enabled"))
        config.glow_enabled = j["glow_enabled"];
    if (j.contains("core_size"))
        config.core_size = j["core_size"];
    if (j.contains("outer_scale_mul"))
        config.outer_scale_mul = j["outer_scale_mul"];
    if (j.contains("outer_rgb_gain"))
        config.outer_rgb_gain = j["outer_rgb_gain"];
    if (j.contains("inner_scale_mul"))
        config.inner_scale_mul = j["inner_scale_mul"];
    if (j.contains("inner_rgb_gain"))
        config.inner_rgb_gain = j["inner_rgb_gain"];
    if (j.contains("final_additive_blit"))
        config.final_additive_blit = j["final_additive_blit"];
    if (j.contains("background_color"))
        config.background_color = json_to_color(j["background_color"]);
    if (j.contains("show_density_heat"))
        config.show_density_heat = j["show_density_heat"];
    if (j.contains("heat_alpha"))
        config.heat_alpha = j["heat_alpha"];
    if (j.contains("show_velocity_field"))
        config.show_velocity_field = j["show_velocity_field"];
    if (j.contains("vel_scale"))
        config.vel_scale = j["vel_scale"];
    if (j.contains("vel_thickness"))
        config.vel_thickness = j["vel_thickness"];
    if (j.contains("show_grid_lines"))
        config.show_grid_lines = j["show_grid_lines"];

    return config;
}

json JsonManager::window_config_to_json(
    const ProjectData::WindowConfig &config) {
    return json{{"screen_width", config.screen_width},
                {"screen_height", config.screen_height},
                {"panel_width", config.panel_width},
                {"render_width", config.render_width}};
}

JsonManager::ProjectData::WindowConfig
JsonManager::json_to_window_config(const json &j) {
    ProjectData::WindowConfig config = {};

    if (j.contains("screen_width"))
        config.screen_width = j["screen_width"];
    if (j.contains("screen_height"))
        config.screen_height = j["screen_height"];
    if (j.contains("panel_width"))
        config.panel_width = j["panel_width"];
    if (j.contains("render_width"))
        config.render_width = j["render_width"];

    return config;
}

static std::string get_home_directory() {
#if defined(_WIN32)
    const char *home = getenv("USERPROFILE");
    if (home && *home)
        return std::string(home);
    const char *drive = getenv("HOMEDRIVE");
    const char *path = getenv("HOMEPATH");
    if (drive && path)
        return std::string(drive) + std::string(path);
    return std::string(".");
#else
    const char *home = getenv("HOME");
    if (home && *home)
        return std::string(home);
    struct passwd *pw = getpwuid(getuid());
    if (pw && pw->pw_dir)
        return std::string(pw->pw_dir);
    return std::string(".");
#endif
}

std::string JsonManager::get_config_path() const {
    const std::string home = get_home_directory();
    return home + "/.particles/" + CONFIG_FILE;
}

void JsonManager::save_config() {
    try {
        json j;
        j[RECENT_FILES_KEY] = m_recent_files;
        j[LAST_FILE_KEY] = m_last_file;

        std::string config_path = get_config_path();
        std::filesystem::create_directories(
            std::filesystem::path(config_path).parent_path());

        std::ofstream file(config_path);
        if (file.is_open()) {
            file << j.dump(2);
            file.close();
        }
    } catch (const std::exception &e) {
        std::cerr << "Error saving config: " << e.what() << std::endl;
    }
}

void JsonManager::load_config() {
    try {
        std::string config_path = get_config_path();
        std::ifstream file(config_path);
        if (!file.is_open()) {
            return; // No config file exists yet
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
