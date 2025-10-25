#include <catch_amalgamated.hpp>

#include "../src/mailbox/mailbox.hpp"
#include "../src/render/types/context.hpp"
#include "../src/render/ui/menu_bar_ui.hpp"
#include "../src/save_manager.hpp"
#include "../src/simulation/simulation.hpp"
#include "../src/undo/add_group_action.hpp"
#include "../src/undo/undo_manager.hpp"

TEST_CASE("UndoManager version tracking", "[version_tracking]") {
    UndoManager undo_manager;

    SECTION("Initial version is zero") {
        REQUIRE(undo_manager.get_state_version() == 0);
    }

    SECTION("Version increments on push") {
        auto action = std::make_unique<AddGroupAction>(
            100, Color{255, 0, 0, 255}, 25.0f, 0);
        undo_manager.push(std::move(action));
        REQUIRE(undo_manager.get_state_version() == 1);
    }

    SECTION("Version increments on undo") {
        auto action = std::make_unique<AddGroupAction>(
            100, Color{255, 0, 0, 255}, 25.0f, 0);
        undo_manager.push(std::move(action));
        REQUIRE(undo_manager.get_state_version() == 1);

        undo_manager.undo();
        REQUIRE(undo_manager.get_state_version() == 2);
    }

    SECTION("Version increments on redo") {
        auto action = std::make_unique<AddGroupAction>(
            100, Color{255, 0, 0, 255}, 25.0f, 0);
        undo_manager.push(std::move(action));
        undo_manager.undo();
        REQUIRE(undo_manager.get_state_version() == 2);

        undo_manager.redo();
        REQUIRE(undo_manager.get_state_version() == 3);
    }
}

TEST_CASE("SaveManager version tracking", "[version_tracking]") {
    SaveManager save_manager;

    SECTION("Initial version is zero") {
        REQUIRE(save_manager.get_file_operation_version() == 0);
    }

    SECTION("Version increments on new project") {
        SaveManager::ProjectData data;
        save_manager.new_project(data);
        REQUIRE(save_manager.get_file_operation_version() == 1);
    }

    SECTION("Version increments on save project") {
        SaveManager::ProjectData data;
        save_manager.new_project(data);
        REQUIRE(save_manager.get_file_operation_version() == 1);

        // Create a temporary file for testing
        std::string temp_file = "/tmp/test_project.json";
        save_manager.save_project(temp_file, data);
        REQUIRE(save_manager.get_file_operation_version() == 2);

        // Clean up
        std::remove(temp_file.c_str());
    }

    SECTION("Version increments on load project") {
        SaveManager::ProjectData data;
        save_manager.new_project(data);

        // Create a temporary file for testing
        std::string temp_file = "/tmp/test_project.json";
        save_manager.save_project(temp_file, data);
        REQUIRE(save_manager.get_file_operation_version() == 2);

        SaveManager::ProjectData loaded_data;
        save_manager.load_project(temp_file, loaded_data);
        REQUIRE(save_manager.get_file_operation_version() == 3);

        // Clean up
        std::remove(temp_file.c_str());
    }
}

TEST_CASE("MenuBarUI undo back to saved state", "[version_tracking]") {
    // Create minimal context for testing
    mailbox::SimulationConfigSnapshot scfg = {};
    scfg.bounds_width = 800.0f;
    scfg.bounds_height = 600.0f;
    Simulation sim(scfg);

    Config rcfg;
    WindowConfig wcfg = {800, 600};
    bool can_interpolate = false;
    float alpha = 1.0f;
    auto view = sim.begin_read_draw();
    auto world_snapshot = sim.get_world_snapshot();

    SaveManager save_manager;
    UndoManager undo_manager;

    Context ctx{
        sim,   rcfg,           view,         wcfg,        can_interpolate,
        alpha, world_snapshot, save_manager, undo_manager};

    MenuBarUI menu_bar;

    SECTION("Undo back to saved state clears indicator") {
        // Capture initial saved state
        menu_bar.capture_saved_state(ctx);
        REQUIRE_FALSE(menu_bar.has_unsaved_changes(ctx));

        // Make a change (this increments version and past size)
        auto action = std::make_unique<AddGroupAction>(
            100, Color{255, 0, 0, 255}, 25.0f, 0);
        ctx.undo.push(std::move(action));
        REQUIRE(menu_bar.has_unsaved_changes(ctx));

        // Undo back to saved state
        ctx.undo.undo();

        // Should now be at saved state (same past size as when saved)
        REQUIRE_FALSE(menu_bar.has_unsaved_changes(ctx));
    }

    SECTION("Redo after undo shows indicator again") {
        // Capture initial saved state
        menu_bar.capture_saved_state(ctx);
        REQUIRE_FALSE(menu_bar.has_unsaved_changes(ctx));

        // Make a change
        auto action = std::make_unique<AddGroupAction>(
            100, Color{255, 0, 0, 255}, 25.0f, 0);
        ctx.undo.push(std::move(action));
        REQUIRE(menu_bar.has_unsaved_changes(ctx));

        // Undo back to saved state
        ctx.undo.undo();
        REQUIRE_FALSE(menu_bar.has_unsaved_changes(ctx));

        // Redo the change
        ctx.undo.redo();
        REQUIRE(menu_bar.has_unsaved_changes(ctx));
    }

    sim.end_read_draw(view);
}
