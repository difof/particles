#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

#include "../mailbox/mailbox.hpp"
#include "../utility/exceptions.hpp"
#include "../utility/logger.hpp"
#include "../utility/math.hpp"
#include "../window_config.hpp"
#include "multicore.hpp"
#include "neighborindex.hpp"
#include "uniformgrid.hpp"
#include "world.hpp"

class Simulation {
  private:
    struct KernelData {
        KernelData() = default;
        ~KernelData() = default;
        KernelData(const KernelData &) = delete;
        KernelData &operator=(const KernelData &) = delete;
        KernelData(KernelData &&) = delete;
        KernelData &operator=(KernelData &&) = delete;

        int particles_count = 0;
        float k_time_scale = 0.f, k_viscosity = 0.f, k_inverse_viscosity = 1.f,
              k_wall_repel = 0.f, k_wall_strength = 0.f;
        float k_gravity_x = 0.f, k_gravity_y = 0.f;
        float inverse_cell = 1.f, width = 0.f, height = 0.f;

        // raw pointers to Simulation-owned scratch buffers to avoid per-tick
        // allocs
        float *fx = nullptr;
        float *fy = nullptr;
    };

  public:
    enum class RunState { NotStarted, Quit, Running, Paused, OneStep };

  public:
    Simulation(mailbox::SimulationConfig::Snapshot cfg);
    ~Simulation();

    Simulation(const Simulation &) = delete;
    Simulation &operator=(const Simulation &) = delete;
    Simulation(Simulation &&) = delete;
    Simulation &operator=(Simulation &&) = delete;

    void begin();
    void end();

    void pause();
    void resume();
    void reset();

    void update_config(mailbox::SimulationConfig::Snapshot &cfg);
    void push_command(const mailbox::command::Command &cmd);
    const std::vector<float> &read_current_draw();
    mailbox::DrawBuffer::ReadView begin_read_draw();
    void end_read_draw(const mailbox::DrawBuffer::ReadView &view);
    mailbox::SimulationStats::Snapshot get_stats() const;
    mailbox::SimulationConfig::Snapshot get_config() const;
    const World &get_world() const;
    inline RunState get_run_state() const noexcept { return m_t_run_state; }
    void force_stats_update();

  private:
    void clear_world();
    void apply_seed(const mailbox::command::SeedSpec &seed,
                    mailbox::SimulationConfig::Snapshot &cfg);
    void step(mailbox::SimulationConfig::Snapshot &cfg);
    void loop_thread();
    void process_commands(mailbox::SimulationConfig::Snapshot &cfg);
    void publish_draw(mailbox::SimulationConfig::Snapshot &cfg);
    int ensure_pool(int t, mailbox::SimulationConfig::Snapshot &cfg);
    bool can_step() const noexcept;
    void measure_tps(int n_threads,
                     std::chrono::nanoseconds step_diff_ns) noexcept;
    void wait_on_tps(int tps) noexcept;
    void kernel_force(int start, int end, KernelData &data);
    void kernel_pos(int start, int end, KernelData &data);
    void kernel_vel(int start, int end, KernelData &data);

  private:
    World m_world;
    NeighborIndex m_idx;
    std::unique_ptr<SimulationThreadPool> m_pool;
    mailbox::command::QueueV m_mail_cmd;
    mailbox::DrawBuffer m_mail_draw;
    mailbox::SimulationConfig m_mail_cfg;
    mailbox::SimulationStats m_mail_stats;
    std::thread m_thread;
    // seed state
    std::shared_ptr<mailbox::command::SeedSpec> m_initial_seed;
    std::shared_ptr<mailbox::command::SeedSpec> m_current_seed;

    // scratch force buffers (reused every step)
    std::vector<float> m_fx, m_fy;

  private:
    RunState m_t_run_state{RunState::NotStarted};
    int m_t_last_published_tps{0};
    int m_t_window_steps{0};
    std::chrono::steady_clock::time_point m_t_window_start;
    std::chrono::steady_clock::time_point m_t_last_step_time;
};