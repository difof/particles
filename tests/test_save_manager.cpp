#include <catch_amalgamated.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "save_manager.hpp"
#include "simulation/world.hpp"
#include "utility/exceptions.hpp"

TEST_CASE("SaveManager - Basic functionality", "[json_manager]") {
    SaveManager manager;

    // Clean up any existing state
    manager.clear_recent_files();
    manager.set_last_opened_file("");

    SECTION("New project creation") {
        SaveManager::ProjectData data;
        REQUIRE_NOTHROW(manager.new_project(data));

        // Check default values
        REQUIRE(data.sim_config.bounds_width == 1080.0f);
        REQUIRE(data.sim_config.bounds_height == 800.0f);
        REQUIRE(data.sim_config.time_scale == 1.0f);
        REQUIRE(data.sim_config.viscosity == 0.271f);
        REQUIRE(data.sim_config.wall_repel == 86.0f);
        REQUIRE(data.sim_config.wall_strength == 0.129f);
        REQUIRE(data.sim_config.sim_threads == -1);

        // Check render config defaults
        REQUIRE(data.render_config.interpolate == true);
        REQUIRE(data.render_config.core_size == 1.5f);
        REQUIRE(data.render_config.glow_enabled == true);

        // Check seed data
        REQUIRE(data.seed != nullptr);
        REQUIRE(data.seed->sizes.size() == 5);
        REQUIRE(data.seed->colors.size() == 5);
        REQUIRE(data.seed->r2.size() == 5);
        REQUIRE(data.seed->rules.size() == 25); // 5x5 matrix

        // Check window config
        REQUIRE(data.window_config.screen_width == 1080);
        REQUIRE(data.window_config.screen_height == 800);
        REQUIRE(data.window_config.panel_width == 500);
        REQUIRE(data.window_config.render_width == 1080);
    }

    SECTION("Save and load project") {
        const std::string test_file = "test_project.json";

        // Create test data
        SaveManager::ProjectData original_data;
        REQUIRE_NOTHROW(manager.new_project(original_data));

        // Modify some values to make them unique
        original_data.sim_config.viscosity = 0.5f;
        original_data.sim_config.wall_repel = 100.0f;
        original_data.render_config.core_size = 2.0f;
        original_data.render_config.background_color = {255, 0, 0,
                                                        255}; // Red background

        // Save project
        REQUIRE_NOTHROW(manager.save_project(test_file, original_data));

        // Load project
        SaveManager::ProjectData loaded_data;
        REQUIRE_NOTHROW(manager.load_project(test_file, loaded_data));

        // Verify loaded data matches original
        REQUIRE(loaded_data.sim_config.viscosity == 0.5f);
        REQUIRE(loaded_data.sim_config.wall_repel == 100.0f);
        REQUIRE(loaded_data.render_config.core_size == 2.0f);
        REQUIRE(loaded_data.render_config.background_color.r == 255);
        REQUIRE(loaded_data.render_config.background_color.g == 0);
        REQUIRE(loaded_data.render_config.background_color.b == 0);
        REQUIRE(loaded_data.render_config.background_color.a == 255);

        // Verify seed data
        REQUIRE(loaded_data.seed != nullptr);
        REQUIRE(loaded_data.seed->sizes.size() ==
                original_data.seed->sizes.size());
        REQUIRE(loaded_data.seed->colors.size() ==
                original_data.seed->colors.size());
        REQUIRE(loaded_data.seed->r2.size() == original_data.seed->r2.size());
        REQUIRE(loaded_data.seed->rules.size() ==
                original_data.seed->rules.size());

        // Clean up
        std::filesystem::remove(test_file);
    }

    SECTION("Recent files management") {
        const std::string test_file1 = "test1.json";
        const std::string test_file2 = "test2.json";
        const std::string test_file3 = "test3.json";

        // Create and save test projects
        SaveManager::ProjectData data;
        REQUIRE_NOTHROW(manager.new_project(data));

        REQUIRE_NOTHROW(manager.save_project(test_file1, data));
        REQUIRE_NOTHROW(manager.save_project(test_file2, data));
        REQUIRE_NOTHROW(manager.save_project(test_file3, data));

        // Check recent files
        auto recent_files = manager.get_recent_files();
        REQUIRE(recent_files.size() == 3);
        REQUIRE(recent_files[0] == test_file3); // Most recent first
        REQUIRE(recent_files[1] == test_file2);
        REQUIRE(recent_files[2] == test_file1);

        // Test clear recent files
        manager.clear_recent_files();
        recent_files = manager.get_recent_files();
        REQUIRE(recent_files.empty());

        // Clean up
        std::filesystem::remove(test_file1);
        std::filesystem::remove(test_file2);
        std::filesystem::remove(test_file3);
    }

    SECTION("Last opened file tracking") {
        const std::string test_file = "test_last_file.json";

        // Initially no last file
        REQUIRE(manager.get_last_opened_file().empty());

        // Set last file
        manager.set_last_opened_file(test_file);
        REQUIRE(manager.get_last_opened_file() == test_file);

        // Clean up
        std::filesystem::remove(test_file);
    }
}

TEST_CASE("SaveManager - World seed extraction", "[json_manager]") {
    SaveManager manager;
    World world;

    // Clean up any existing state
    manager.clear_recent_files();
    manager.set_last_opened_file("");

    SECTION("Extract from empty world") {
        // Create WorldSnapshot from World
        mailbox::WorldSnapshot snapshot;
        snapshot.group_count = world.get_groups_size();
        snapshot.particles_count = world.get_particles_size();
        snapshot.m_group_ranges = world.get_group_ranges();
        snapshot.m_group_colors = world.get_group_colors();
        snapshot.m_group_radii2 = world.get_group_radii2();
        snapshot.m_group_enabled = world.get_group_enabled();
        snapshot.m_rules = world.get_rules();
        snapshot.m_particle_groups = world.get_particle_groups();

        auto seed = manager.extract_current_seed(snapshot);
        REQUIRE(seed == nullptr); // No groups in empty world
    }

    SECTION("Extract from world with groups") {
        // Add some groups to the world
        Color color1 = {255, 0, 0, 255};
        Color color2 = {0, 255, 0, 255};

        int group1 = world.add_group(100, color1);
        int group2 = world.add_group(200, color2);

        // Initialize rule tables for the groups
        world.init_rule_tables(2);

        // Set some rules and radii
        world.set_rule(group1, group2, 0.5f);
        world.set_rule(group2, group1, -0.3f);
        world.set_r2(group1, 100.0f);
        world.set_r2(group2, 150.0f);

        // Finalize groups
        world.finalize_groups();

        // Create WorldSnapshot from World
        mailbox::WorldSnapshot snapshot;
        snapshot.group_count = world.get_groups_size();
        snapshot.particles_count = world.get_particles_size();
        snapshot.m_group_ranges = world.get_group_ranges();
        snapshot.m_group_colors = world.get_group_colors();
        snapshot.m_group_radii2 = world.get_group_radii2();
        snapshot.m_group_enabled = world.get_group_enabled();
        snapshot.m_rules = world.get_rules();
        snapshot.m_particle_groups = world.get_particle_groups();

        // Extract seed
        auto seed = manager.extract_current_seed(snapshot);
        REQUIRE(seed != nullptr);

        // Verify extracted data
        REQUIRE(seed->sizes.size() == 2);
        REQUIRE(seed->sizes[0] == 100);
        REQUIRE(seed->sizes[1] == 200);

        REQUIRE(seed->colors.size() == 2);
        REQUIRE(seed->colors[0].r == 255);
        REQUIRE(seed->colors[0].g == 0);
        REQUIRE(seed->colors[0].b == 0);
        REQUIRE(seed->colors[1].r == 0);
        REQUIRE(seed->colors[1].g == 255);
        REQUIRE(seed->colors[1].b == 0);

        REQUIRE(seed->r2.size() == 2);
        REQUIRE(seed->r2[0] == 100.0f);
        REQUIRE(seed->r2[1] == 150.0f);

        REQUIRE(seed->rules.size() == 4); // 2x2 matrix
        REQUIRE(seed->rules[0] == 0.0f);  // group1->group1
        REQUIRE(seed->rules[1] == 0.5f);  // group1->group2
        REQUIRE(seed->rules[2] == -0.3f); // group2->group1
        REQUIRE(seed->rules[3] == 0.0f);  // group2->group2
    }
}

TEST_CASE("SaveManager - Error handling", "[json_manager]") {
    SaveManager manager;

    SECTION("Load non-existent file") {
        SaveManager::ProjectData data;
        REQUIRE_THROWS_AS(manager.load_project("non_existent_file.json", data),
                          particles::IOError);
    }

    SECTION("Save to invalid path") {
        SaveManager::ProjectData data;
        REQUIRE_NOTHROW(manager.new_project(data));

        // Try to save to a directory that doesn't exist and can't be created
        REQUIRE_THROWS_AS(
            manager.save_project("/invalid/path/that/does/not/exist/file.json",
                                 data),
            particles::IOError);
    }
}

TEST_CASE("SaveManager - Color serialization", "[json_manager]") {
    SaveManager manager;

    SECTION("Color round-trip") {
        Color original = {128, 64, 192, 255};

        // Convert to JSON and back
        auto json_color = manager.color_to_json(original);
        Color converted = manager.json_to_color(json_color);

        REQUIRE(converted.r == original.r);
        REQUIRE(converted.g == original.g);
        REQUIRE(converted.b == original.b);
        REQUIRE(converted.a == original.a);
    }

    SECTION("Edge case colors") {
        Color black = {0, 0, 0, 0};
        Color white = {255, 255, 255, 255};

        auto json_black = manager.color_to_json(black);
        auto json_white = manager.color_to_json(white);

        Color converted_black = manager.json_to_color(json_black);
        Color converted_white = manager.json_to_color(json_white);

        REQUIRE(converted_black.r == 0);
        REQUIRE(converted_black.g == 0);
        REQUIRE(converted_black.b == 0);
        REQUIRE(converted_black.a == 0);

        REQUIRE(converted_white.r == 255);
        REQUIRE(converted_white.g == 255);
        REQUIRE(converted_white.b == 255);
        REQUIRE(converted_white.a == 255);
    }
}
