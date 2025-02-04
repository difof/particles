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
