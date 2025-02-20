#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <thread>
#include <vector>

#include "../mailbox/data_snapshot.hpp"
#include "../mailbox/mailbox.hpp"
#include "../utility/exceptions.hpp"
#include "../utility/logger.hpp"
#include "../utility/math.hpp"
#include "multicore.hpp"
#include "neighborindex.hpp"
#include "render/types/window.hpp"
#include "uniformgrid.hpp"
#include "world.hpp"

/**
 * @brief Main simulation class that manages particle physics simulation
 *
 * Handles particle interactions, threading, and communication with the renderer
 * through mailbox system.
 */
class Simulation {
  private:
    /**
     * @brief Data structure containing kernel parameters for particle
     * computation
     */
    struct KernelData {
        KernelData() = default;
        ~KernelData() = default;
        KernelData(const KernelData &) = delete;
        KernelData &operator=(const KernelData &) = delete;
        KernelData(KernelData &&) = delete;
        KernelData &operator=(KernelData &&) = delete;

        /** @brief Number of particles in the simulation */
        int particles_count = 0;
        /** @brief Time scaling factor for simulation speed */
        float k_time_scale = 0.f;
        /** @brief Viscosity coefficient (0-1) */
        float k_viscosity = 0.f;
        /** @brief Inverse viscosity for velocity damping */
        float k_inverse_viscosity = 1.f;
        /** @brief Wall repulsion distance threshold */
        float k_wall_repel = 0.f;
        /** @brief Wall repulsion strength */
        float k_wall_strength = 0.f;
        /** @brief Gravity force in X direction */
        float k_gravity_x = 0.f;
        /** @brief Gravity force in Y direction */
        float k_gravity_y = 0.f;
        /** @brief Inverse cell size for spatial grid */
        float inverse_cell = 1.f;
        /** @brief Simulation bounds width */
        float width = 0.f;
        /** @brief Simulation bounds height */
        float height = 0.f;

        /** @brief Raw pointer to force buffer X components (owned by
         * Simulation) */
        float *fx = nullptr;
        /** @brief Raw pointer to force buffer Y components (owned by
         * Simulation) */
        float *fy = nullptr;
    };

  public:
    /**
     * @brief Simulation execution states
     */
    enum class RunState { NotStarted, Quit, Running, Paused, OneStep };

  public:
    Simulation(mailbox::SimulationConfigSnapshot cfg);
    ~Simulation();
    Simulation(const Simulation &) = delete;
    Simulation &operator=(const Simulation &) = delete;
    Simulation(Simulation &&) = delete;
    Simulation &operator=(Simulation &&) = delete;

    /**
     * @brief Starts the simulation thread and begins execution
     */
    void begin();

    /**
     * @brief Stops the simulation thread and cleans up resources
     */
    void end();

    /**
     * @brief Pauses the simulation
     */
    void pause();

    /**
     * @brief Resumes the simulation
     */
    void resume();

    /**
     * @brief Resets the simulation world to initial state
     */
    void reset();

    /**
     * @brief Updates simulation configuration
     * @param cfg New configuration parameters
     */
    void update_config(mailbox::SimulationConfigSnapshot &cfg);

    /**
     * @brief Pushes a command to the simulation command queue
     * @param cmd Command to execute
     */
    void push_command(const mailbox::command::Command &cmd);

    /**
     * @brief Gets current draw data for rendering
     * @return Reference to current particle position/velocity data
     */
    const std::vector<float> &read_current_draw();

    /**
     * @brief Begins reading draw data with proper synchronization
     * @return ReadView for safe access to draw data
     */
    mailbox::render::ReadView begin_read_draw();

    /**
     * @brief Ends reading draw data and releases synchronization
     * @param view ReadView to release
     */
    void end_read_draw(const mailbox::render::ReadView &view);

    /**
     * @brief Gets current simulation statistics
     * @return Current simulation stats snapshot
     */
    mailbox::SimulationStatsSnapshot get_stats() const;

    /**
     * @brief Gets current simulation configuration
     * @return Current configuration snapshot
     */
    mailbox::SimulationConfigSnapshot get_config() const;

    /**
     * @brief Gets current world snapshot
     * @return Current world snapshot
     */
    mailbox::WorldSnapshot get_world_snapshot() const;

    /**
     * @brief Gets current simulation run state
     * @return Current run state
     */
    inline RunState get_run_state() const noexcept { return m_t_run_state; }

    /**
     * @brief Forces an immediate update of simulation statistics
     */
    void force_stats_publish();

  private:
    /**
     * @brief Clears the simulation world of all particles
     */
    void clear_world();

    /**
     * @brief Applies a seed specification to initialize the world
     * @param seed Seed specification containing particle groups and rules
     * @param cfg Current simulation configuration
     */
    void apply_seed(const mailbox::command::SeedSpec &seed,
                    mailbox::SimulationConfigSnapshot &cfg);

    /**
     * @brief Performs one simulation step (force calculation, velocity update,
     * position update)
     * @param cfg Current simulation configuration
     */
    void step(mailbox::SimulationConfigSnapshot &cfg);

    /**
     * @brief Main simulation loop running in separate thread
     */
    void loop_thread();

    /**
     * @brief Processes all pending commands from the command queue
     * @param cfg Current simulation configuration
     */
    void process_commands(mailbox::SimulationConfigSnapshot &cfg);

    /**
     * @brief Publishes current particle data to the draw buffer
     * @param cfg Current simulation configuration
     */
    void publish_draw(mailbox::SimulationConfigSnapshot &cfg);

    /**
     * @brief Publishes current world snapshot
     */
    void publish_world_snapshot();

    /**
     * @brief Ensures thread pool has correct number of threads
     * @param current_threads Current thread count
     * @param cfg Current simulation configuration
     * @return Actual number of threads in pool
     */
    int ensure_pool(int current_threads,
                    mailbox::SimulationConfigSnapshot &cfg);

    /**
     * @brief Checks if simulation can perform a step
     * @return True if simulation should step
     */
    bool can_step() const noexcept;

    /**
     * @brief Measures and updates TPS (ticks per second) statistics
     * @param n_threads Number of threads used
     * @param step_diff_ns Time taken for the step in nanoseconds
     */
    void measure_tps(int n_threads,
                     std::chrono::nanoseconds step_diff_ns) noexcept;

    /**
     * @brief Waits to maintain target TPS if specified
     * @param target_tps Target ticks per second (0 = no limit)
     */
    void wait_on_tps(int target_tps) noexcept;

    /**
     * @brief Publishes stats immediately for better responsiveness
     * @param n_threads Number of threads used
     * @param step_diff_ns Time taken for the step in nanoseconds
     */
    void
    publish_stats_immediately(int n_threads,
                              std::chrono::nanoseconds step_diff_ns) noexcept;

    /**
     * @brief Kernel function for force calculation between particles
     * @param start Start particle index
     * @param end End particle index (exclusive)
     * @param data Kernel data containing simulation parameters
     */
    void kernel_force(int start, int end, KernelData &data);

    /**
     * @brief Kernel function for position update and boundary collision
     * @param start Start particle index
     * @param end End particle index (exclusive)
     * @param data Kernel data containing simulation parameters
     */
    void kernel_pos(int start, int end, KernelData &data);

    /**
     * @brief Kernel function for velocity update with viscosity
     * @param start Start particle index
     * @param end End particle index (exclusive)
     * @param data Kernel data containing simulation parameters
     */
    void kernel_vel(int start, int end, KernelData &data);

    // Command processing functions
    /**
     * @brief Handles SeedWorld command
     * @param cmd The seed world command
     * @param cfg Current simulation configuration
     */
    void handle_seed_world(const mailbox::command::SeedWorld &cmd,
                           mailbox::SimulationConfigSnapshot &cfg);

    /**
     * @brief Handles OneStep command
     */
    void handle_one_step();

    /**
     * @brief Handles Pause command
     */
    void handle_pause();

    /**
     * @brief Handles Resume command
     */
    void handle_resume();

    /**
     * @brief Handles ResetWorld command
     * @param cfg Current simulation configuration
     */
    void handle_reset_world(mailbox::SimulationConfigSnapshot &cfg);

    /**
     * @brief Handles ApplyRules command
     * @param cmd The apply rules command
     * @param cfg Current simulation configuration
     */
    void handle_apply_rules(const mailbox::command::ApplyRules &cmd,
                            mailbox::SimulationConfigSnapshot &cfg);

    /**
     * @brief Handles AddGroup command
     * @param cmd The add group command
     * @param cfg Current simulation configuration
     */
    void handle_add_group(const mailbox::command::AddGroup &cmd,
                          mailbox::SimulationConfigSnapshot &cfg);

    /**
     * @brief Handles RemoveGroup command
     * @param cmd The remove group command
     */
    void handle_remove_group(const mailbox::command::RemoveGroup &cmd);

    /**
     * @brief Handles RemoveAllGroups command
     */
    void handle_remove_all_groups();

    /**
     * @brief Handles ResizeGroup command
     * @param cmd The resize group command
     * @param cfg Current simulation configuration
     */
    void handle_resize_group(const mailbox::command::ResizeGroup &cmd,
                             mailbox::SimulationConfigSnapshot &cfg);

    /**
     * @brief Handles Quit command
     */
    void handle_quit();

  private:
    /** @brief Simulation world containing all particles and groups */
    World m_world;
    /** @brief Spatial indexing structure for efficient neighbor finding */
    NeighborIndex m_idx;
    /** @brief Thread pool for parallel computation */
    std::unique_ptr<SimulationThreadPool> m_pool;
    /** @brief Command queue for thread-safe communication */
    mailbox::command::Queue m_mail_cmd;
    /** @brief Draw buffer for particle data exchange with renderer */
    mailbox::render::DrawBuffer m_mail_draw;
    /** @brief Configuration data snapshot for thread-safe access */
    mailbox::DataSnapshot<mailbox::SimulationConfigSnapshot> m_mail_cfg;
    /** @brief Statistics data snapshot for thread-safe access */
    mailbox::DataSnapshot<mailbox::SimulationStatsSnapshot> m_mail_stats;
    /** @brief World data snapshot for thread-safe access */
    mailbox::DataSnapshot<mailbox::WorldSnapshot> m_mail_world;
    /** @brief Main simulation thread */
    std::thread m_thread;
    /** @brief Initial seed used to create the simulation */
    std::optional<mailbox::command::SeedSpec> m_initial_seed;
    /** @brief Current seed specification */
    std::optional<mailbox::command::SeedSpec> m_current_seed;

    /** @brief Force buffer X components (reused every step) */
    std::vector<float> m_fx;
    /** @brief Force buffer Y components (reused every step) */
    std::vector<float> m_fy;

  private:
    /** @brief Current simulation execution state */
    RunState m_t_run_state{RunState::NotStarted};
    /** @brief Last published TPS value */
    int m_t_last_published_tps{0};
    /** @brief Number of steps in current measurement window */
    int m_t_window_steps{0};
    /** @brief Total number of simulation steps completed */
    long long m_total_steps{0};
    /** @brief Start time of current TPS measurement window */
    std::chrono::steady_clock::time_point m_t_window_start;
    /** @brief Time of last simulation step */
    std::chrono::steady_clock::time_point m_t_last_step_time;
};