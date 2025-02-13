#include "simulation.hpp"

using namespace std::chrono;

constexpr float EPS = 1e-12f;

constexpr int grid_offsets[9][2] = {{-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {0, 0},
                                    {1, 0},   {-1, 1}, {0, 1},  {1, 1}};

inline long long now_ns() {
    return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch())
        .count();
}

Simulation::Simulation(mailbox::SimulationConfigSnapshot cfg)
    : m_world(), m_idx(), m_pool(std::make_unique<SimulationThreadPool>(1)),
      m_mail_cmd(), m_mail_draw(), m_mail_cfg(), m_mail_stats(),
      m_mail_world() {
    LOG_INFO("Initializing simulation");

    mailbox::SimulationConfigSnapshot default_config = {};
    default_config.bounds_width = default_config.bounds_height = 0.f;
    default_config.time_scale = 1.f;
    default_config.viscosity = 0.1f;
    default_config.wall_repel = 0.f;
    default_config.wall_strength = 0.f;
    default_config.gravity_x = 0.f;
    default_config.gravity_y = 0.f;
    default_config.target_tps = 0;
    default_config.sim_threads = 1;
    default_config.draw_report = {false};

    m_mail_cfg.publish(default_config);
    m_mail_cfg.publish(default_config);

    mailbox::SimulationStatsSnapshot default_stats = {};
    m_mail_stats.publish(default_stats);
    m_mail_stats.publish(default_stats);

    update_config(cfg);
}

Simulation::~Simulation() { end(); }

void Simulation::begin() {
    if (m_t_run_state != RunState::NotStarted) {
        return;
    }

    m_thread = std::thread(&Simulation::loop_thread, this);
    resume();
}

void Simulation::end() {
    if (m_t_run_state != RunState::NotStarted &&
        m_t_run_state != RunState::Quit && !m_thread.joinable()) {
        return;
    }

    push_command(mailbox::command::Quit{});
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void Simulation::pause() { push_command(mailbox::command::Pause{}); }

void Simulation::resume() { push_command(mailbox::command::Resume{}); }

void Simulation::reset() { push_command(mailbox::command::ResetWorld{}); }

void Simulation::update_config(mailbox::SimulationConfigSnapshot &cfg) {
    if (cfg.bounds_width <= 0 || cfg.bounds_height <= 0) {
        throw particles::ConfigError(
            "Invalid bounds: " + std::to_string(cfg.bounds_width) + "x" +
            std::to_string(cfg.bounds_height));
    }

    if (cfg.time_scale < 0) {
        throw particles::ConfigError("Invalid time scale: " +
                                     std::to_string(cfg.time_scale));
    }

    if (cfg.viscosity < 0 || cfg.viscosity > 1) {
        throw particles::ConfigError("Invalid viscosity: " +
                                     std::to_string(cfg.viscosity));
    }

    if (cfg.sim_threads < -1) {
        throw particles::ConfigError("Invalid thread count: " +
                                     std::to_string(cfg.sim_threads));
    }

    LOG_DEBUG(
        "Updating simulation config: " + std::to_string(cfg.bounds_width) +
        "x" + std::to_string(cfg.bounds_height) +
        ", threads=" + std::to_string(cfg.sim_threads));

    m_mail_cfg.publish(cfg);
}

void Simulation::push_command(const mailbox::command::Command &cmd) {
    m_mail_cmd.push(cmd);
}

const std::vector<float> &Simulation::read_current_draw() {
    return m_mail_draw.read_current_only();
}

mailbox::render::ReadView Simulation::begin_read_draw() {
    return m_mail_draw.begin_read();
}

void Simulation::end_read_draw(const mailbox::render::ReadView &view) {
    m_mail_draw.end_read(view);
}

mailbox::SimulationStatsSnapshot Simulation::get_stats() const {
    return m_mail_stats.acquire();
}

mailbox::SimulationConfigSnapshot Simulation::get_config() const {
    return m_mail_cfg.acquire();
}

void Simulation::force_stats_publish() {
    mailbox::SimulationStatsSnapshot st;
    st.effective_tps = m_t_last_published_tps;
    st.particles = m_world.get_particles_size();
    st.groups = m_world.get_groups_size();
    st.sim_threads = 1;  // Default value for forced update
    st.last_step_ns = 0; // Not applicable for forced update
    st.published_ns = now_ns();
    st.num_steps = m_total_steps; // Publish actual step count
    m_mail_stats.publish(st);
}

void Simulation::step(mailbox::SimulationConfigSnapshot &cfg) {
    const int particles_count = m_world.get_particles_size();
    if (particles_count == 0) {
        return;
    }

    if ((int)m_fx.size() != particles_count) {
        m_fx.resize(particles_count);
    }
    if ((int)m_fy.size() != particles_count) {
        m_fy.resize(particles_count);
    }
    std::fill_n(m_fx.data(), particles_count, 0.f);
    std::fill_n(m_fy.data(), particles_count, 0.f);

    KernelData data;
    data.particles_count = particles_count;
    data.k_time_scale = cfg.time_scale;
    data.k_viscosity = cfg.viscosity;
    data.k_inverse_viscosity = 1.f - cfg.viscosity;
    data.k_wall_repel = cfg.wall_repel;
    data.k_wall_strength = cfg.wall_strength;
    data.k_gravity_x = cfg.gravity_x;
    data.k_gravity_y = cfg.gravity_y;
    data.width = cfg.bounds_width;
    data.height = cfg.bounds_height;
    data.fx = m_fx.data();
    data.fy = m_fy.data();

    float maxR = std::max(1.0f, m_world.max_interaction_radius());
    data.inverse_cell =
        m_idx.ensure(m_world, cfg.bounds_width, cfg.bounds_height, maxR);

    // accumulate forces
    m_pool->parallel_for_n(
        [&](int s, int e) {
            kernel_force(s, e, data);
        },
        particles_count);

    // velocity update
    m_pool->parallel_for_n(
        [&](int s, int e) {
            kernel_vel(s, e, data);
        },
        particles_count);

    // position + bounce
    m_pool->parallel_for_n(
        [&](int s, int e) {
            kernel_pos(s, e, data);
        },
        particles_count);
}

int Simulation::ensure_pool(int t, mailbox::SimulationConfigSnapshot &cfg) {
    int desired = (cfg.sim_threads <= 0) ? compute_sim_threads()
                                         : std::max(1, cfg.sim_threads);
    if (!m_pool || desired != t) {
        m_pool = std::make_unique<SimulationThreadPool>(desired);
        return desired;
    }

    return t;
}

inline bool Simulation::can_step() const noexcept {
    return m_t_run_state == RunState::Running ||
           m_t_run_state == RunState::OneStep;
}

void Simulation::measure_tps(int n_threads, nanoseconds step_diff_ns) noexcept {
    auto now = steady_clock::now();
    if (now - m_t_window_start >= 1s) {
        int secs = (int)duration_cast<seconds>(now - m_t_window_start).count();
        if (secs < 1)
            secs = 1;
        m_t_last_published_tps = m_t_window_steps / secs;

        mailbox::SimulationStatsSnapshot st;
        st.effective_tps = m_t_last_published_tps;
        st.particles = m_world.get_particles_size();
        st.groups = m_world.get_groups_size();
        st.sim_threads = n_threads;
        st.last_step_ns = step_diff_ns.count();
        st.published_ns = now_ns();
        // Always publish the step count, regardless of run state
        st.num_steps = m_total_steps;
        m_mail_stats.publish(st);

        m_t_window_steps = 0;
        m_t_window_start = now;
    }
}

void Simulation::publish_stats_immediately(int n_threads,
                                           nanoseconds step_diff_ns) noexcept {
    mailbox::SimulationStatsSnapshot st;
    st.effective_tps = m_t_last_published_tps;
    st.particles = m_world.get_particles_size();
    st.groups = m_world.get_groups_size();
    st.sim_threads = n_threads;
    st.last_step_ns = step_diff_ns.count();
    st.published_ns = now_ns();
    st.num_steps = m_total_steps;
    m_mail_stats.publish(st);
}

void Simulation::wait_on_tps(int target_tps) noexcept {
    if (target_tps <= 0) {
        return;
    }

    const auto target_frame_time = nanoseconds(1'000'000'000LL / target_tps);
    const auto now = steady_clock::now();
    const auto elapsed = now - m_t_last_step_time;

    if (elapsed < target_frame_time) {
        std::this_thread::sleep_for(target_frame_time - elapsed);
    }

    m_t_last_step_time = steady_clock::now();
}

void Simulation::loop_thread() {
    auto current_config = get_config();
    // no auto seeding; wait for a seed command or reset
    m_world.reset(false);
    m_mail_draw.bootstrap_same_as_current(0, now_ns());

    m_t_last_step_time = steady_clock::now();
    m_t_window_start = m_t_last_step_time;
    m_t_window_steps = 0;
    m_t_last_published_tps = 0;
    m_total_steps = 0;
    int current_thread_count = -9999;

    while (m_t_run_state != RunState::Quit) {
        current_thread_count =
            ensure_pool(current_thread_count, current_config);

        process_commands(current_config);

        if (m_t_run_state == RunState::Quit) {
            break;
        }

        auto step_begin_time = steady_clock::now();
        if (can_step()) {
            step(current_config);
            m_t_window_steps++;
            m_total_steps++;
        }
        auto step_end_time = steady_clock::now();

        publish_draw(current_config);
        publish_world_snapshot();
        measure_tps(current_thread_count, (step_end_time - step_begin_time));

        // Publish stats more frequently for better responsiveness
        publish_stats_immediately(current_thread_count,
                                  (step_end_time - step_begin_time));

        wait_on_tps(current_config.target_tps);

        if (m_t_run_state == RunState::OneStep) {
            m_t_run_state = RunState::Paused;
        }

        current_config = get_config();
    }
}

void Simulation::process_commands(mailbox::SimulationConfigSnapshot &cfg) {
    for (const auto &cmd : m_mail_cmd.drain()) {
        std::visit(
            [&](auto &&c) {
                using T = std::decay_t<decltype(c)>;
                if constexpr (std::is_same_v<T, mailbox::command::SeedWorld>) {
                    handle_seed_world(c, cfg);
                } else if constexpr (std::is_same_v<
                                         T, mailbox::command::OneStep>) {
                    handle_one_step();
                } else if constexpr (std::is_same_v<T,
                                                    mailbox::command::Pause>) {
                    handle_pause();
                } else if constexpr (std::is_same_v<T,
                                                    mailbox::command::Resume>) {
                    handle_resume();
                } else if constexpr (std::is_same_v<
                                         T, mailbox::command::ResetWorld>) {
                    handle_reset_world(cfg);
                } else if constexpr (std::is_same_v<
                                         T, mailbox::command::ApplyRules>) {
                    handle_apply_rules(c, cfg);
                } else if constexpr (std::is_same_v<
                                         T, mailbox::command::AddGroup>) {
                    handle_add_group(c, cfg);
                } else if constexpr (std::is_same_v<
                                         T, mailbox::command::RemoveGroup>) {
                    handle_remove_group(c);
                } else if constexpr (std::is_same_v<
                                         T,
                                         mailbox::command::RemoveAllGroups>) {
                    handle_remove_all_groups();
                } else if constexpr (std::is_same_v<
                                         T, mailbox::command::ResizeGroup>) {
                    handle_resize_group(c, cfg);
                } else if constexpr (std::is_same_v<T,
                                                    mailbox::command::Quit>) {
                    handle_quit();
                }
            },
            cmd);
    }
}

void Simulation::publish_draw(mailbox::SimulationConfigSnapshot &cfg) {
    const int particles_count = m_world.get_particles_size();

    auto &pos = m_mail_draw.begin_write_pos(size_t(particles_count) * 2);
    auto &vel = m_mail_draw.begin_write_vel(size_t(particles_count) * 2);
    auto &grid_frame = m_mail_draw.begin_write_grid(
        m_idx.grid.cols(), m_idx.grid.rows(), particles_count,
        m_idx.grid.cell_size(), m_idx.grid.width(), m_idx.grid.height());

    // Use SoA bulk operations for better performance
    const float *const px_array = m_world.get_px_array();
    const float *const py_array = m_world.get_py_array();
    const float *const vx_array = m_world.get_vx_array();
    const float *const vy_array = m_world.get_vy_array();

    for (int i = 0; i < particles_count; ++i) {
        const size_t b = size_t(i) * 2;
        pos[b + 0] = px_array[i];
        pos[b + 1] = py_array[i];
        vel[b + 0] = vx_array[i];
        vel[b + 1] = vy_array[i];
    }

    if (cfg.draw_report.grid_data) {
        grid_frame.head = m_idx.grid.head();
        grid_frame.next = m_idx.grid.next();

        const int grid_size = grid_frame.cols * grid_frame.rows;
        for (int ci = 0; ci < grid_size; ++ci) {
            int cell_count = 0;
            float sx = 0.f, sy = 0.f;

            for (int p = grid_frame.head[ci]; p != -1; p = grid_frame.next[p]) {
                const size_t b = size_t(p) * 2;
                sx += vel[b + 0];
                sy += vel[b + 1];
                ++cell_count;
            }

            grid_frame.count[ci] = cell_count;
            grid_frame.sumVx[ci] = sx;
            grid_frame.sumVy[ci] = sy;
        }
    }

    m_mail_draw.publish(now_ns());
}

void Simulation::clear_world() { m_world.reset(false); }

void Simulation::apply_seed(const mailbox::command::SeedSpec &seed,
                            mailbox::SimulationConfigSnapshot &cfg) {
    m_world.reset(false);

    const int G = (int)seed.sizes.size();
    if (G <= 0) {
        // empty seed leaves world cleared
        return;
    }

    for (int g = 0; g < G; ++g) {
        Color col = (g < (int)seed.colors.size()) ? seed.colors[g] : WHITE;
        m_world.add_group(seed.sizes[g], col);
    }

    const int N = m_world.get_particles_size();
    std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> rx(0.f, cfg.bounds_width);
    std::uniform_real_distribution<float> ry(0.f, cfg.bounds_height);
    for (int i = 0; i < N; ++i) {
        m_world.set_px(i, rx(rng));
        m_world.set_py(i, ry(rng));
        m_world.set_vx(i, 0.f);
        m_world.set_vy(i, 0.f);
    }

    m_world.finalize_groups();
    m_world.init_rule_tables(G);

    // r2
    for (int g = 0; g < G; ++g) {
        float r2 = (g < (int)seed.r2.size()) ? seed.r2[g] : 80.f * 80.f;
        m_world.set_r2(g, r2);
    }

    // rules
    if ((int)seed.rules.size() == G * G) {
        for (int i = 0; i < G; ++i) {
            for (int j = 0; j < G; ++j) {
                m_world.set_rule(i, j, seed.rules[i * G + j]);
            }
        }
    }

    // enabled state
    for (int g = 0; g < G; ++g) {
        bool enabled = (g < (int)seed.enabled.size()) ? seed.enabled[g] : true;
        m_world.set_group_enabled(g, enabled);
    }
}

inline void Simulation::kernel_force(int start, int end, KernelData &data) {
    // Get SoA arrays for better cache locality and potential vectorization
    const float *const px_array = m_world.get_px_array();
    const float *const py_array = m_world.get_py_array();

    for (int i = start; i < end; ++i) {
        const float particle_x = px_array[i];
        const float particle_y = py_array[i];
        const int group_index = m_world.group_of(i);
        const float interaction_radius_squared = m_world.r2_of(group_index);

        // skip disabled groups
        if (!m_world.is_group_enabled(group_index)) {
            data.fx[i] = 0.f;
            data.fy[i] = 0.f;
            continue;
        }

        if (interaction_radius_squared <= 0.f) {
            data.fx[i] = 0.f;
            data.fy[i] = 0.f;
            continue;
        }

        float force_x = 0.f, force_y = 0.f;
        int cell_x = std::min(int(particle_x * data.inverse_cell),
                              m_idx.grid.cols() - 1);
        int cell_y = std::min(int(particle_y * data.inverse_cell),
                              m_idx.grid.rows() - 1);

        const auto interaction_rules = m_world.rules_of(group_index);

        for (int k = 0; k < 9; ++k) {
            const int neighbor_cell_index = m_idx.grid.cell_index(
                cell_x + grid_offsets[k][0], cell_y + grid_offsets[k][1]);

            if (neighbor_cell_index < 0) {
                continue;
            }

            const int cell_start =
                m_idx.grid.cell_start_at(neighbor_cell_index);
            const int cell_count =
                m_idx.grid.cell_count_at(neighbor_cell_index);
            const auto &particle_indices = m_idx.grid.indices();
            const int cell_end = cell_start + cell_count;
            for (int pos = cell_start; pos < cell_end; ++pos) {
                const int j = particle_indices[pos];
                if (j == i) {
                    continue;
                }
                const float other_particle_x = px_array[j];
                const float other_particle_y = py_array[j];
                const float dx = particle_x - other_particle_x;
                const float dy = particle_y - other_particle_y;
                const float distance_squared = dx * dx + dy * dy;
                if (distance_squared > 0.f &&
                    distance_squared < interaction_radius_squared) {
                    const int other_group_index = m_world.group_of(j);
                    // skip if target particle's group is disabled
                    if (!m_world.is_group_enabled(other_group_index)) {
                        continue;
                    }
                    const float interaction_strength =
                        interaction_rules.get(other_group_index);
                    const float inv_distance =
                        rsqrt_fast(std::max(distance_squared, EPS));
                    const float force_magnitude =
                        interaction_strength * inv_distance;
                    force_x += force_magnitude * dx;
                    force_y += force_magnitude * dy;
                }
            }
        }

        if (data.k_wall_repel > 0.f) {
            const float wall_repel_distance = data.k_wall_repel;
            const float wall_strength = data.k_wall_strength;

            if (particle_x < wall_repel_distance) {
                force_x += (wall_repel_distance - particle_x) * wall_strength;
            }
            if (particle_x > data.width - wall_repel_distance) {
                force_x += (data.width - wall_repel_distance - particle_x) *
                           wall_strength;
            }
            if (particle_y < wall_repel_distance) {
                force_y += (wall_repel_distance - particle_y) * wall_strength;
            }
            if (particle_y > data.height - wall_repel_distance) {
                force_y += (data.height - wall_repel_distance - particle_y) *
                           wall_strength;
            }
        }

        // apply gravity
        force_x += data.k_gravity_x;
        force_y += data.k_gravity_y;

        data.fx[i] = force_x;
        data.fy[i] = force_y;
    }
}

inline void Simulation::kernel_vel(int start, int end, KernelData &data) {
    // Get SoA arrays for better cache locality and potential vectorization
    float *const vx_array = m_world.get_vx_array_mut();
    float *const vy_array = m_world.get_vy_array_mut();

    for (int i = start; i < end; ++i) {
        const float new_velocity_x = vx_array[i] * data.k_inverse_viscosity +
                                     data.fx[i] * data.k_time_scale;
        const float new_velocity_y = vy_array[i] * data.k_inverse_viscosity +
                                     data.fy[i] * data.k_time_scale;

        vx_array[i] = new_velocity_x;
        vy_array[i] = new_velocity_y;
    }
}

inline void Simulation::kernel_pos(int start, int end, KernelData &data) {
    // Get SoA arrays for better cache locality and potential vectorization
    const float *const px_array = m_world.get_px_array();
    const float *const py_array = m_world.get_py_array();
    const float *const vx_array = m_world.get_vx_array();
    const float *const vy_array = m_world.get_vy_array();
    float *const px_array_mut = m_world.get_px_array_mut();
    float *const py_array_mut = m_world.get_py_array_mut();
    float *const vx_array_mut = m_world.get_vx_array_mut();
    float *const vy_array_mut = m_world.get_vy_array_mut();

    for (int i = start; i < end; ++i) {
        float new_x = px_array[i] + vx_array[i];
        float new_y = py_array[i] + vy_array[i];
        float new_velocity_x = vx_array[i];
        float new_velocity_y = vy_array[i];

        if (new_x < 0.f) {
            new_x = -new_x;
            new_velocity_x = -new_velocity_x;
        }
        if (new_x >= data.width) {
            new_x = 2.f * data.width - new_x;
            new_velocity_x = -new_velocity_x;
        }
        if (new_y < 0.f) {
            new_y = -new_y;
            new_velocity_y = -new_velocity_y;
        }
        if (new_y >= data.height) {
            new_y = 2.f * data.height - new_y;
            new_velocity_y = -new_velocity_y;
        }

        px_array_mut[i] = new_x;
        py_array_mut[i] = new_y;
        vx_array_mut[i] = new_velocity_x;
        vy_array_mut[i] = new_velocity_y;
    }
}

// Command handler implementations
void Simulation::handle_seed_world(const mailbox::command::SeedWorld &cmd,
                                   mailbox::SimulationConfigSnapshot &cfg) {
    if (cmd.seed) {
        m_initial_seed = *cmd.seed;
        m_current_seed = *cmd.seed;
        apply_seed(*cmd.seed, cfg);
        m_t_window_steps = 0;
        m_t_window_start = steady_clock::now();
        m_total_steps = 0;

        // Publish stats immediately after seeding to reflect the reset step
        // count
        publish_stats_immediately(1, std::chrono::nanoseconds(0));
    }
}

void Simulation::handle_one_step() { m_t_run_state = RunState::OneStep; }

void Simulation::handle_pause() { m_t_run_state = RunState::Paused; }

void Simulation::handle_resume() { m_t_run_state = RunState::Running; }

void Simulation::handle_reset_world(mailbox::SimulationConfigSnapshot &cfg) {
    if (m_initial_seed.has_value()) {
        apply_seed(m_initial_seed.value(), cfg);
    } else {
        clear_world();
    }
    m_t_window_steps = 0;
    m_t_window_start = steady_clock::now();
    m_total_steps = 0;

    // Publish stats immediately after reset to reflect the reset step count
    publish_stats_immediately(1, std::chrono::nanoseconds(0));
}

void Simulation::handle_apply_rules(const mailbox::command::ApplyRules &cmd,
                                    mailbox::SimulationConfigSnapshot &cfg) {
    if (!cmd.patch) {
        return;
    }

    const int groups_count = m_world.get_groups_size();
    const mailbox::command::RulePatch &p = *cmd.patch;

    auto apply_colors_if_any = [&](int group_count) {
        if (!p.colors.empty() && (int)p.colors.size() == group_count) {
            for (int i = 0; i < group_count; ++i) {
                m_world.set_group_color(i, p.colors[i]);
            }
        }
    };

    auto apply_enabled_if_any = [&](int group_count) {
        if (!p.enabled.empty() && (int)p.enabled.size() == group_count) {
            for (int i = 0; i < group_count; ++i) {
                m_world.set_group_enabled(i, p.enabled[i]);
            }
        }
    };

    if (p.groups == groups_count && p.hot) {
        for (int g = 0; g < groups_count; ++g) {
            m_world.set_r2(g, p.r2[g]);
        }
        for (int i = 0; i < groups_count; ++i) {
            const float *row = p.rules.data() + i * groups_count;
            for (int j = 0; j < groups_count; ++j) {
                m_world.set_rule(i, j, row[j]);
            }
        }
        apply_colors_if_any(groups_count);
        apply_enabled_if_any(groups_count);
    } else {
        const int Gnow = m_world.get_groups_size();
        mailbox::command::SeedSpec new_seed;
        new_seed.sizes.resize(Gnow);
        new_seed.colors.resize(Gnow);
        new_seed.r2.resize(Gnow);
        new_seed.rules.resize(Gnow * Gnow);

        for (int g = 0; g < Gnow; ++g) {
            const int start = m_world.get_group_start(g);
            const int end = m_world.get_group_end(g);
            new_seed.sizes[g] = end - start;
            if (!p.colors.empty() && (int)p.colors.size() == Gnow) {
                new_seed.colors[g] = p.colors[g];
            } else {
                new_seed.colors[g] = m_world.get_group_color(g);
            }
            if (!p.r2.empty() && (int)p.r2.size() == Gnow) {
                new_seed.r2[g] = p.r2[g];
            } else {
                new_seed.r2[g] = m_world.r2_of(g);
            }
        }

        if (!p.rules.empty() && (int)p.rules.size() == Gnow * Gnow) {
            for (int i = 0; i < Gnow; ++i) {
                const float *row = p.rules.data() + i * Gnow;
                for (int j = 0; j < Gnow; ++j) {
                    new_seed.rules[i * Gnow + j] = row[j];
                }
            }
        } else {
            for (int i = 0; i < Gnow; ++i) {
                const auto rowv = m_world.rules_of(i);
                for (int j = 0; j < Gnow; ++j) {
                    new_seed.rules[i * Gnow + j] = rowv.get(j);
                }
            }
        }

        m_current_seed = new_seed;
        m_initial_seed = new_seed;
        apply_colors_if_any(Gnow);
        apply_enabled_if_any(Gnow);
        apply_seed(m_current_seed.value(), cfg);
        m_t_window_steps = 0;
        m_t_window_start = steady_clock::now();
        m_total_steps = 0;
    }
}

void Simulation::handle_add_group(const mailbox::command::AddGroup &cmd,
                                  mailbox::SimulationConfigSnapshot &cfg) {
    const int old_group_count = m_world.get_groups_size();
    m_world.add_group(cmd.size, cmd.color);
    m_world.finalize_groups();

    // only initialize rule tables if this is the first group
    if (old_group_count == 0) {
        m_world.init_rule_tables(m_world.get_groups_size());
    } else {
        // preserve existing rules when adding to existing groups
        m_world.preserve_rules_on_add_group();
    }

    int new_group_index = m_world.get_groups_size() - 1;
    m_world.set_r2(new_group_index, cmd.r2);

    std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> rx(0.f, cfg.bounds_width);
    std::uniform_real_distribution<float> ry(0.f, cfg.bounds_height);
    const int start = m_world.get_group_start(new_group_index);
    const int end = m_world.get_group_end(new_group_index);
    for (int i = start; i < end; ++i) {
        m_world.set_px(i, rx(rng));
        m_world.set_py(i, ry(rng));
        m_world.set_vx(i, 0.f);
        m_world.set_vy(i, 0.f);
    }
}

void Simulation::handle_remove_group(const mailbox::command::RemoveGroup &cmd) {
    int group_index = cmd.group_index;
    const int total_groups = m_world.get_groups_size();
    if (group_index >= 0 && group_index < total_groups) {
        // backup data before removing the group
        std::vector<float> old_rules;
        std::vector<float> old_radii2;
        std::vector<bool> old_enabled;
        if (total_groups > 1) {
            // backup current state
            for (int i = 0; i < total_groups; ++i) {
                old_radii2.push_back(m_world.r2_of(i));
                old_enabled.push_back(m_world.is_group_enabled(i));
                for (int j = 0; j < total_groups; ++j) {
                    old_rules.push_back(m_world.rule_val(i, j));
                }
            }
        }

        m_world.remove_group(group_index);
        m_world.finalize_groups();

        // restore rules if we had multiple groups
        if (total_groups > 1) {
            m_world.init_rule_tables(m_world.get_groups_size());

            // restore old rules, skipping the removed group
            for (int i = 0; i < total_groups; ++i) {
                if (i == group_index) {
                    continue; // skip the removed group
                }

                int new_i = (i > group_index)
                                ? i - 1
                                : i; // adjust index for removed group

                for (int j = 0; j < total_groups; ++j) {
                    if (j == group_index) {
                        continue; // skip the removed group
                    }

                    int new_j = (j > group_index)
                                    ? j - 1
                                    : j; // adjust index for removed group
                    m_world.set_rule(new_i, new_j,
                                     old_rules[i * total_groups + j]);
                }
                m_world.set_r2(new_i, old_radii2[i]);
                m_world.set_group_enabled(new_i, old_enabled[i]);
            }
        } else {
            m_world.init_rule_tables(m_world.get_groups_size());
        }

        // don't reseed - just clear the current seed since it's no longer valid
        m_current_seed = std::nullopt;
        m_t_window_steps = 0;
        m_t_window_start = steady_clock::now();
        m_total_steps = 0;
    }
}

void Simulation::handle_remove_all_groups() {
    m_world.reset(true);
    m_world.init_rule_tables(0);
    m_current_seed = std::nullopt;
    m_t_window_steps = 0;
    m_t_window_start = steady_clock::now();
    m_total_steps = 0;
}

void Simulation::handle_resize_group(const mailbox::command::ResizeGroup &cmd,
                                     mailbox::SimulationConfigSnapshot &cfg) {
    int group_index = cmd.group_index;
    int new_size = cmd.new_size;
    const int total_groups = m_world.get_groups_size();
    if (group_index >= 0 && group_index < total_groups && new_size >= 0) {
        const int current_size = m_world.get_group_size(group_index);
        const int start = m_world.get_group_start(group_index);

        m_world.resize_group(group_index, new_size);

        // initialize new particles if we added any
        if (new_size > current_size) {
            std::mt19937 rng{std::random_device{}()};
            std::uniform_real_distribution<float> rx(0.f, cfg.bounds_width);
            std::uniform_real_distribution<float> ry(0.f, cfg.bounds_height);

            for (int i = start + current_size; i < start + new_size; ++i) {
                m_world.set_px(i, rx(rng));
                m_world.set_py(i, ry(rng));
                m_world.set_vx(i, 0.f);
                m_world.set_vy(i, 0.f);
            }
        }

        m_t_window_steps = 0;
        m_t_window_start = steady_clock::now();
        m_total_steps = 0;
    }
}

void Simulation::handle_quit() { m_t_run_state = RunState::Quit; }

void Simulation::publish_world_snapshot() {
    mailbox::WorldSnapshot snapshot;
    snapshot.group_count = m_world.get_groups_size();
    snapshot.particles_count = m_world.get_particles_size();
    snapshot.set_group_ranges(m_world.get_group_ranges());       // Copy data
    snapshot.set_group_colors(m_world.get_group_colors());       // Copy data
    snapshot.set_group_radii2(m_world.get_group_radii2());       // Copy data
    snapshot.set_group_enabled(m_world.get_group_enabled());     // Copy data
    snapshot.set_rules(m_world.get_rules());                     // Copy data
    snapshot.set_particle_groups(m_world.get_particle_groups()); // Copy data
    m_mail_world.publish(snapshot);
}

mailbox::WorldSnapshot Simulation::get_world_snapshot() const {
    return m_mail_world.acquire();
}
