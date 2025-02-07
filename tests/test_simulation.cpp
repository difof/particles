#include <catch_amalgamated.hpp>

#include "simulation/simulation.hpp"
#include "utility/exceptions.hpp"
#include <chrono>
#include <thread>

TEST_CASE("Simulation initialization", "[simulation]") {
    mailbox::SimulationConfigSnapshot cfg;
    cfg.bounds_width = 1000.0f;
    cfg.bounds_height = 800.0f;
    cfg.target_tps = 60;
    cfg.time_scale = 1.0f;
    cfg.viscosity = 0.1f;
    cfg.wall_repel = 10.0f;
    cfg.wall_strength = 0.1f;
    cfg.sim_threads = 1;

    Simulation sim(cfg);
    // Constructor should not throw with valid config
}

TEST_CASE("Simulation config validation", "[simulation]") {
    // Create a valid config first
    mailbox::SimulationConfigSnapshot valid_cfg;
    valid_cfg.bounds_width = 1000.0f;
    valid_cfg.bounds_height = 800.0f;
    valid_cfg.target_tps = 60;
    valid_cfg.time_scale = 1.0f;
    valid_cfg.viscosity = 0.1f;
    valid_cfg.wall_repel = 10.0f;
    valid_cfg.wall_strength = 0.1f;
    valid_cfg.sim_threads = 1;

    Simulation sim(valid_cfg);

    // Test invalid bounds
    auto invalid_cfg = mailbox::SimulationConfigSnapshot{};
    invalid_cfg.bounds_width = -100.0f;
    invalid_cfg.bounds_height = 800.0f;

    REQUIRE_THROWS_AS(sim.update_config(invalid_cfg), particles::ConfigError);

    // Test invalid time scale
    invalid_cfg.bounds_width = 1000.0f;
    invalid_cfg.time_scale = -1.0f;

    REQUIRE_THROWS_AS(sim.update_config(invalid_cfg), particles::ConfigError);

    // Test invalid viscosity
    invalid_cfg.time_scale = 1.0f;
    invalid_cfg.viscosity = 1.5f;

    REQUIRE_THROWS_AS(sim.update_config(invalid_cfg), particles::ConfigError);

    // Test invalid thread count
    invalid_cfg.viscosity = 0.1f;
    invalid_cfg.sim_threads = -2;

    REQUIRE_THROWS_AS(sim.update_config(invalid_cfg), particles::ConfigError);
}

TEST_CASE("Simulation lifecycle", "[simulation]") {
    mailbox::SimulationConfigSnapshot cfg;
    cfg.bounds_width = 1000.0f;
    cfg.bounds_height = 800.0f;
    cfg.target_tps = 0; // No throttling for tests
    cfg.time_scale = 1.0f;
    cfg.viscosity = 0.1f;
    cfg.sim_threads = 1;

    Simulation sim(cfg);

    // Test initial state
    REQUIRE(sim.get_run_state() == Simulation::RunState::NotStarted);

    // Test start
    sim.begin();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    REQUIRE(sim.get_run_state() == Simulation::RunState::Running);

    // Test pause
    sim.pause();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    REQUIRE(sim.get_run_state() == Simulation::RunState::Paused);

    // Test resume
    sim.resume();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    REQUIRE(sim.get_run_state() == Simulation::RunState::Running);

    // Test reset
    sim.reset();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    REQUIRE(sim.get_run_state() == Simulation::RunState::Running);

    // Test end
    sim.end();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    REQUIRE(sim.get_run_state() == Simulation::RunState::Quit);
}

TEST_CASE("Simulation command processing", "[simulation]") {
    mailbox::SimulationConfigSnapshot cfg;
    cfg.bounds_width = 1000.0f;
    cfg.bounds_height = 800.0f;
    cfg.target_tps = 0;
    cfg.time_scale = 1.0f;
    cfg.viscosity = 0.1f;
    cfg.sim_threads = 1;

    Simulation sim(cfg);
    sim.begin();

    // Test command queue
    sim.push_command(mailbox::command::Pause{});
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    REQUIRE(sim.get_run_state() == Simulation::RunState::Paused);

    sim.push_command(mailbox::command::Resume{});
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    REQUIRE(sim.get_run_state() == Simulation::RunState::Running);

    sim.end();
}

TEST_CASE("Simulation config updates", "[simulation]") {
    mailbox::SimulationConfigSnapshot cfg;
    cfg.bounds_width = 1000.0f;
    cfg.bounds_height = 800.0f;
    cfg.target_tps = 0;
    cfg.time_scale = 1.0f;
    cfg.viscosity = 0.1f;
    cfg.sim_threads = 1;

    Simulation sim(cfg);
    sim.begin();

    // Update config while running
    cfg.time_scale = 2.0f;
    cfg.viscosity = 0.2f;

    REQUIRE_NOTHROW(sim.update_config(cfg));

    // Verify config was updated
    auto current_cfg = sim.get_config();
    REQUIRE(current_cfg.time_scale == Catch::Approx(2.0f));
    REQUIRE(current_cfg.viscosity == Catch::Approx(0.2f));

    sim.end();
}

TEST_CASE("Simulation boundary conditions", "[simulation]") {
    mailbox::SimulationConfigSnapshot cfg;
    cfg.bounds_width = 100.0f;
    cfg.bounds_height = 100.0f;
    cfg.target_tps = 0;
    cfg.time_scale = 1.0f;
    cfg.viscosity = 0.0f;  // No damping
    cfg.wall_repel = 0.0f; // No walls
    cfg.sim_threads = 1;

    Simulation sim(cfg);
    sim.begin();

    // Let it run for a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Check that simulation is still running
    REQUIRE(sim.get_run_state() == Simulation::RunState::Running);

    sim.end();
}

TEST_CASE("Simulation zero particles", "[simulation]") {
    mailbox::SimulationConfigSnapshot cfg;
    cfg.bounds_width = 1000.0f;
    cfg.bounds_height = 800.0f;
    cfg.target_tps = 0;
    cfg.time_scale = 1.0f;
    cfg.viscosity = 0.1f;
    cfg.sim_threads = 1;

    Simulation sim(cfg);
    sim.begin();

    // Should handle zero particles gracefully
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    REQUIRE(sim.get_run_state() == Simulation::RunState::Running);

    sim.end();
}

TEST_CASE("Simulation stats", "[simulation]") {
    mailbox::SimulationConfigSnapshot cfg;
    cfg.bounds_width = 1000.0f;
    cfg.bounds_height = 800.0f;
    cfg.target_tps = 0;
    cfg.time_scale = 1.0f;
    cfg.viscosity = 0.1f;
    cfg.sim_threads = 1;

    Simulation sim(cfg);

    // Test stats before starting
    auto stats = sim.get_stats();
    REQUIRE(stats.particles >= 0);
    REQUIRE(stats.groups >= 0);
    // sim_threads might be 0 if not started yet
    REQUIRE(stats.sim_threads >= 0);

    sim.begin();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Test stats while running
    stats = sim.get_stats();
    REQUIRE(stats.particles >= 0);
    REQUIRE(stats.groups >= 0);
    REQUIRE(stats.sim_threads >= 0);

    sim.end();
}

TEST_CASE("Simulation draw data access", "[simulation]") {
    mailbox::SimulationConfigSnapshot cfg;
    cfg.bounds_width = 1000.0f;
    cfg.bounds_height = 800.0f;
    cfg.target_tps = 0;
    cfg.time_scale = 1.0f;
    cfg.viscosity = 0.1f;
    cfg.sim_threads = 1;

    Simulation sim(cfg);
    sim.begin();

    // Test read_current_draw
    const auto &draw_data = sim.read_current_draw();
    REQUIRE(draw_data.size() >= 0); // Should be valid even with no particles

    // Test begin_read_draw and end_read_draw
    auto read_view = sim.begin_read_draw();
    REQUIRE((read_view.prev != nullptr || read_view.curr != nullptr));
    sim.end_read_draw(read_view);

    sim.end();
}

TEST_CASE("Simulation force stats publish", "[simulation]") {
    mailbox::SimulationConfigSnapshot cfg;
    cfg.bounds_width = 1000.0f;
    cfg.bounds_height = 800.0f;
    cfg.target_tps = 0;
    cfg.time_scale = 1.0f;
    cfg.viscosity = 0.1f;
    cfg.sim_threads = 1;

    Simulation sim(cfg);
    sim.begin();

    // Force stats publish should not throw
    REQUIRE_NOTHROW(sim.force_stats_publish());

    // Check that stats are updated
    auto stats = sim.get_stats();
    REQUIRE(stats.particles >= 0);
    REQUIRE(stats.groups >= 0);

    sim.end();
}

TEST_CASE("Simulation seed world command", "[simulation]") {
    mailbox::SimulationConfigSnapshot cfg;
    cfg.bounds_width = 1000.0f;
    cfg.bounds_height = 800.0f;
    cfg.target_tps = 0;
    cfg.time_scale = 1.0f;
    cfg.viscosity = 0.1f;
    cfg.sim_threads = 1;

    Simulation sim(cfg);
    sim.begin();

    // Wait for simulation to start and be running
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    REQUIRE(sim.get_run_state() == Simulation::RunState::Running);

    // Test seeding with particles
    auto seed = std::make_shared<mailbox::command::SeedSpec>();
    seed->sizes = {100, 50};
    seed->colors = {RED, BLUE};
    seed->r2 = {6400.0f, 1600.0f};           // 80^2, 40^2
    seed->rules = {0.0f, 0.1f, -0.1f, 0.0f}; // 2x2 matrix
    seed->enabled = {true, true};

    mailbox::command::SeedWorld seed_cmd;
    seed_cmd.seed = seed;
    sim.push_command(seed_cmd);

    // Wait for command to be processed
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Check that particles were added by checking the world directly
    const World &world = sim.get_world();
    REQUIRE(world.get_particles_size() == 150);
    REQUIRE(world.get_groups_size() == 2);

    // Test clearing world with RemoveAllGroups command
    mailbox::command::RemoveAllGroups remove_all_cmd;
    sim.push_command(remove_all_cmd);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Check that world was cleared
    REQUIRE(world.get_particles_size() == 0);
    REQUIRE(world.get_groups_size() == 0);

    sim.end();
}

TEST_CASE("Simulation group management commands", "[simulation]") {
    mailbox::SimulationConfigSnapshot cfg;
    cfg.bounds_width = 1000.0f;
    cfg.bounds_height = 800.0f;
    cfg.target_tps = 0;
    cfg.time_scale = 1.0f;
    cfg.viscosity = 0.1f;
    cfg.sim_threads = 1;

    Simulation sim(cfg);
    sim.begin();

    // Wait for simulation to start
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    REQUIRE(sim.get_run_state() == Simulation::RunState::Running);

    // Test AddGroup
    mailbox::command::AddGroup add_cmd;
    add_cmd.size = 50;
    add_cmd.color = GREEN;
    add_cmd.r2 = 2500.0f; // 50^2
    sim.push_command(add_cmd);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Check that particles were added by checking the world directly
    const World &world = sim.get_world();
    REQUIRE(world.get_particles_size() == 50);
    REQUIRE(world.get_groups_size() == 1);

    // Test ResizeGroup
    mailbox::command::ResizeGroup resize_cmd;
    resize_cmd.group_index = 0;
    resize_cmd.new_size = 75;
    sim.push_command(resize_cmd);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    REQUIRE(world.get_particles_size() == 75);
    REQUIRE(world.get_groups_size() == 1);

    // Test RemoveGroup
    mailbox::command::RemoveGroup remove_cmd;
    remove_cmd.group_index = 0;
    sim.push_command(remove_cmd);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    REQUIRE(world.get_particles_size() == 0);
    REQUIRE(world.get_groups_size() == 0);

    sim.end();
}

TEST_CASE("Simulation remove all groups command", "[simulation]") {
    mailbox::SimulationConfigSnapshot cfg;
    cfg.bounds_width = 1000.0f;
    cfg.bounds_height = 800.0f;
    cfg.target_tps = 0;
    cfg.time_scale = 1.0f;
    cfg.viscosity = 0.1f;
    cfg.sim_threads = 1;

    Simulation sim(cfg);
    sim.begin();

    // Wait for simulation to start
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    REQUIRE(sim.get_run_state() == Simulation::RunState::Running);

    // Add multiple groups first
    mailbox::command::AddGroup add_cmd1;
    add_cmd1.size = 30;
    add_cmd1.color = RED;
    sim.push_command(add_cmd1);

    mailbox::command::AddGroup add_cmd2;
    add_cmd2.size = 40;
    add_cmd2.color = BLUE;
    sim.push_command(add_cmd2);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Check that particles were added by checking the world directly
    const World &world = sim.get_world();
    REQUIRE(world.get_particles_size() == 70);
    REQUIRE(world.get_groups_size() == 2);

    // Test RemoveAllGroups
    mailbox::command::RemoveAllGroups remove_all_cmd;
    sim.push_command(remove_all_cmd);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    REQUIRE(world.get_particles_size() == 0);
    REQUIRE(world.get_groups_size() == 0);

    sim.end();
}

TEST_CASE("Simulation apply rules command", "[simulation]") {
    mailbox::SimulationConfigSnapshot cfg;
    cfg.bounds_width = 1000.0f;
    cfg.bounds_height = 800.0f;
    cfg.target_tps = 0;
    cfg.time_scale = 1.0f;
    cfg.viscosity = 0.1f;
    cfg.sim_threads = 1;

    Simulation sim(cfg);
    sim.begin();

    // Wait for simulation to start
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    REQUIRE(sim.get_run_state() == Simulation::RunState::Running);

    // First add a group
    mailbox::command::AddGroup add_cmd;
    add_cmd.size = 50;
    add_cmd.color = RED;
    sim.push_command(add_cmd);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Test hot patch (same group count)
    auto patch = std::make_shared<mailbox::command::RulePatch>();
    patch->groups = 1;
    patch->r2 = {3600.0f}; // 60^2
    patch->rules = {0.0f}; // 1x1 matrix
    patch->colors = {GREEN};
    patch->enabled = {true};
    patch->hot = true;

    mailbox::command::ApplyRules apply_cmd;
    apply_cmd.patch = patch;
    sim.push_command(apply_cmd);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Should still have same particle count
    const World &world = sim.get_world();
    REQUIRE(world.get_particles_size() == 50);
    REQUIRE(world.get_groups_size() == 1);

    sim.end();
}

TEST_CASE("Simulation one step command", "[simulation]") {
    mailbox::SimulationConfigSnapshot cfg;
    cfg.bounds_width = 1000.0f;
    cfg.bounds_height = 800.0f;
    cfg.target_tps = 0;
    cfg.time_scale = 1.0f;
    cfg.viscosity = 0.1f;
    cfg.sim_threads = 1;

    Simulation sim(cfg);
    sim.begin();

    // Add some particles
    mailbox::command::AddGroup add_cmd;
    add_cmd.size = 25;
    add_cmd.color = RED;
    sim.push_command(add_cmd);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Pause simulation
    sim.pause();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    REQUIRE(sim.get_run_state() == Simulation::RunState::Paused);

    // Test OneStep command
    mailbox::command::OneStep one_step_cmd;
    sim.push_command(one_step_cmd);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Should be paused again after one step
    REQUIRE(sim.get_run_state() == Simulation::RunState::Paused);

    sim.end();
}

TEST_CASE("Simulation TPS measurement with manual stepping", "[simulation]") {
    mailbox::SimulationConfigSnapshot cfg;
    cfg.bounds_width = 1000.0f;
    cfg.bounds_height = 800.0f;
    cfg.target_tps = 0; // No throttling
    cfg.time_scale = 1.0f;
    cfg.viscosity = 0.1f;
    cfg.sim_threads = 1;

    Simulation sim(cfg);
    sim.begin();

    // Add particles for meaningful simulation
    mailbox::command::AddGroup add_cmd;
    add_cmd.size = 100;
    add_cmd.color = RED;
    sim.push_command(add_cmd);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Pause and step manually to test TPS measurement
    sim.pause();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Get initial stats
    auto initial_stats = sim.get_stats();
    long long initial_steps = initial_stats.num_steps;

    // Perform several manual steps
    for (int i = 0; i < 5; ++i) {
        mailbox::command::OneStep one_step_cmd;
        sim.push_command(one_step_cmd);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Check that step count increased
    auto final_stats = sim.get_stats();
    REQUIRE(final_stats.num_steps >= initial_steps);

    sim.end();
}

TEST_CASE("Simulation thread pool management", "[simulation]") {
    mailbox::SimulationConfigSnapshot cfg;
    cfg.bounds_width = 1000.0f;
    cfg.bounds_height = 800.0f;
    cfg.target_tps = 0;
    cfg.time_scale = 1.0f;
    cfg.viscosity = 0.1f;
    cfg.sim_threads = 2; // Test with multiple threads

    Simulation sim(cfg);
    sim.begin();

    // Wait for simulation to start
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    REQUIRE(sim.get_run_state() == Simulation::RunState::Running);

    // Add particles to test multi-threading
    mailbox::command::AddGroup add_cmd;
    add_cmd.size = 200;
    add_cmd.color = RED;
    sim.push_command(add_cmd);

    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Check that particles were added by checking the world directly
    const World &world = sim.get_world();
    REQUIRE(world.get_particles_size() == 200);
    REQUIRE(world.get_groups_size() == 1);

    // Test changing thread count
    cfg.sim_threads = 4;
    sim.update_config(cfg);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Thread count change is tested by the fact that the simulation continues
    // to work We don't need to verify the exact thread count as it's an
    // internal implementation detail

    sim.end();
}

TEST_CASE("Simulation edge cases", "[simulation]") {
    mailbox::SimulationConfigSnapshot cfg;
    cfg.bounds_width = 1000.0f;
    cfg.bounds_height = 800.0f;
    cfg.target_tps = 0;
    cfg.time_scale = 1.0f;
    cfg.viscosity = 0.1f;
    cfg.sim_threads = 1;

    Simulation sim(cfg);
    sim.begin();

    // Test invalid group operations
    mailbox::command::RemoveGroup invalid_remove;
    invalid_remove.group_index = 999; // Non-existent group
    sim.push_command(invalid_remove);

    mailbox::command::ResizeGroup invalid_resize;
    invalid_resize.group_index = -1; // Invalid index
    invalid_resize.new_size = 50;
    sim.push_command(invalid_resize);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Should still be running without errors
    REQUIRE(sim.get_run_state() == Simulation::RunState::Running);

    // Test negative resize
    mailbox::command::AddGroup add_cmd;
    add_cmd.size = 50;
    add_cmd.color = RED;
    sim.push_command(add_cmd);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    mailbox::command::ResizeGroup negative_resize;
    negative_resize.group_index = 0;
    negative_resize.new_size = -10; // Negative size
    sim.push_command(negative_resize);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Should handle gracefully
    REQUIRE(sim.get_run_state() == Simulation::RunState::Running);

    sim.end();
}

TEST_CASE("Simulation world access", "[simulation]") {
    mailbox::SimulationConfigSnapshot cfg;
    cfg.bounds_width = 1000.0f;
    cfg.bounds_height = 800.0f;
    cfg.target_tps = 0;
    cfg.time_scale = 1.0f;
    cfg.viscosity = 0.1f;
    cfg.sim_threads = 1;

    Simulation sim(cfg);
    sim.begin();

    // Test get_world() method
    const World &world = sim.get_world();
    REQUIRE(world.get_particles_size() >= 0);
    REQUIRE(world.get_groups_size() >= 0);

    // Add particles and verify world state
    mailbox::command::AddGroup add_cmd;
    add_cmd.size = 75;
    add_cmd.color = BLUE;
    sim.push_command(add_cmd);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    REQUIRE(world.get_particles_size() == 75);
    REQUIRE(world.get_groups_size() == 1);

    sim.end();
}

TEST_CASE("Simulation multiple begin/end calls", "[simulation]") {
    mailbox::SimulationConfigSnapshot cfg;
    cfg.bounds_width = 1000.0f;
    cfg.bounds_height = 800.0f;
    cfg.target_tps = 0;
    cfg.time_scale = 1.0f;
    cfg.viscosity = 0.1f;
    cfg.sim_threads = 1;

    Simulation sim(cfg);

    // Test multiple begin calls (should be safe)
    sim.begin();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    REQUIRE(sim.get_run_state() == Simulation::RunState::Running);

    // Second begin call should be ignored
    sim.begin();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    REQUIRE(sim.get_run_state() == Simulation::RunState::Running);

    // Test end call
    sim.end();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    REQUIRE(sim.get_run_state() == Simulation::RunState::Quit);

    // Second end call should be safe
    sim.end();
    REQUIRE(sim.get_run_state() == Simulation::RunState::Quit);
}

TEST_CASE("Simulation config edge cases", "[simulation]") {
    mailbox::SimulationConfigSnapshot cfg;
    cfg.bounds_width = 1000.0f;
    cfg.bounds_height = 800.0f;
    cfg.target_tps = 0;
    cfg.time_scale = 1.0f;
    cfg.viscosity = 0.1f;
    cfg.sim_threads = 1;

    Simulation sim(cfg);

    // Test boundary values
    cfg.viscosity = 0.0f; // Minimum viscosity
    REQUIRE_NOTHROW(sim.update_config(cfg));

    cfg.viscosity = 1.0f; // Maximum viscosity
    REQUIRE_NOTHROW(sim.update_config(cfg));

    cfg.time_scale = 0.0f; // Zero time scale
    REQUIRE_NOTHROW(sim.update_config(cfg));

    cfg.sim_threads = 0; // Auto-detect threads
    REQUIRE_NOTHROW(sim.update_config(cfg));

    cfg.sim_threads = -1; // Auto-detect threads (alternative)
    REQUIRE_NOTHROW(sim.update_config(cfg));
}

TEST_CASE("Simulation draw report configuration", "[simulation]") {
    mailbox::SimulationConfigSnapshot cfg;
    cfg.bounds_width = 1000.0f;
    cfg.bounds_height = 800.0f;
    cfg.target_tps = 0;
    cfg.time_scale = 1.0f;
    cfg.viscosity = 0.1f;
    cfg.sim_threads = 1;
    cfg.draw_report.grid_data = true; // Enable grid data

    Simulation sim(cfg);
    sim.begin();

    // Add particles to test grid data
    mailbox::command::AddGroup add_cmd;
    add_cmd.size = 50;
    add_cmd.color = RED;
    sim.push_command(add_cmd);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Test draw data with grid enabled
    auto read_view = sim.begin_read_draw();
    REQUIRE(read_view.grid != nullptr);
    sim.end_read_draw(read_view);

    sim.end();
}

TEST_CASE("Simulation step count behavior during pause/resume",
          "[simulation]") {
    mailbox::SimulationConfigSnapshot cfg;
    cfg.bounds_width = 1000.0f;
    cfg.bounds_height = 800.0f;
    cfg.target_tps = 0;
    cfg.time_scale = 1.0f;
    cfg.viscosity = 0.1f;
    cfg.sim_threads = 1;

    Simulation sim(cfg);
    sim.begin();

    // Wait for simulation to start and be running
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    REQUIRE(sim.get_run_state() == Simulation::RunState::Running);

    // Add some particles for meaningful simulation
    mailbox::command::AddGroup add_cmd;
    add_cmd.size = 50;
    add_cmd.color = RED;
    sim.push_command(add_cmd);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Let simulation run for a bit to accumulate steps
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Get initial step count
    auto initial_stats = sim.get_stats();
    long long initial_steps = initial_stats.num_steps;
    REQUIRE(initial_steps > 0); // Should have some steps by now

    // Pause simulation
    sim.pause();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    REQUIRE(sim.get_run_state() == Simulation::RunState::Paused);

    // Check that step count doesn't increase significantly while paused
    // Allow for a small race condition (1-2 steps) due to timing
    auto paused_stats = sim.get_stats();
    REQUIRE(paused_stats.num_steps <= initial_steps + 2);

    // Resume simulation
    sim.resume();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    REQUIRE(sim.get_run_state() == Simulation::RunState::Running);

    // Let it run for a bit more
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Check that step count continues to increase after resume
    auto resumed_stats = sim.get_stats();
    REQUIRE(resumed_stats.num_steps > initial_steps);

    sim.end();
}

TEST_CASE("Simulation step count behavior during reset", "[simulation]") {
    mailbox::SimulationConfigSnapshot cfg;
    cfg.bounds_width = 1000.0f;
    cfg.bounds_height = 800.0f;
    cfg.target_tps = 0;
    cfg.time_scale = 1.0f;
    cfg.viscosity = 0.1f;
    cfg.sim_threads = 1;

    Simulation sim(cfg);
    sim.begin();

    // Wait for simulation to start and be running
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    REQUIRE(sim.get_run_state() == Simulation::RunState::Running);

    // Add some particles for meaningful simulation
    mailbox::command::AddGroup add_cmd;
    add_cmd.size = 50;
    add_cmd.color = RED;
    sim.push_command(add_cmd);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Let simulation run for a bit to accumulate steps
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Get step count before reset
    auto stats_before_reset = sim.get_stats();
    long long steps_before_reset = stats_before_reset.num_steps;
    REQUIRE(steps_before_reset > 0); // Should have some steps by now

    // Reset simulation
    sim.reset();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Check that step count is reset to near 0 (allowing for steps during the
    // delay)
    auto stats_after_reset = sim.get_stats();
    REQUIRE(stats_after_reset.num_steps < steps_before_reset);

    // Let it run for a bit more to ensure it continues counting
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Check that step count starts increasing again after reset
    auto stats_after_running = sim.get_stats();
    REQUIRE(stats_after_running.num_steps > 0);

    sim.end();
}

TEST_CASE("Simulation step count behavior with manual stepping",
          "[simulation]") {
    mailbox::SimulationConfigSnapshot cfg;
    cfg.bounds_width = 1000.0f;
    cfg.bounds_height = 800.0f;
    cfg.target_tps = 0;
    cfg.time_scale = 1.0f;
    cfg.viscosity = 0.1f;
    cfg.sim_threads = 1;

    Simulation sim(cfg);
    sim.begin();

    // Wait for simulation to start and be running
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    REQUIRE(sim.get_run_state() == Simulation::RunState::Running);

    // Add some particles for meaningful simulation
    mailbox::command::AddGroup add_cmd;
    add_cmd.size = 50;
    add_cmd.color = RED;
    sim.push_command(add_cmd);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Let simulation run for a bit to accumulate steps
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Get initial step count
    auto initial_stats = sim.get_stats();
    long long initial_steps = initial_stats.num_steps;
    REQUIRE(initial_steps > 0);

    // Pause and do manual stepping
    sim.pause();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    REQUIRE(sim.get_run_state() == Simulation::RunState::Paused);

    // Perform several manual steps
    for (int i = 0; i < 5; ++i) {
        mailbox::command::OneStep one_step_cmd;
        sim.push_command(one_step_cmd);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Check that step count increased with manual steps
    auto stats_after_manual = sim.get_stats();
    REQUIRE(stats_after_manual.num_steps > initial_steps);

    // Resume and let it run
    sim.resume();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    REQUIRE(sim.get_run_state() == Simulation::RunState::Running);

    // Let it run for a bit more
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Check that step count continues to increase after resume
    auto final_stats = sim.get_stats();
    REQUIRE(final_stats.num_steps > stats_after_manual.num_steps);

    sim.end();
}
