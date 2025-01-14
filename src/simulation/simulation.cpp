#include "simulation.hpp"

using namespace std::chrono;

constexpr float EPS = 1e-12f;

constexpr int grid_offsets[9][2] = {{-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {0, 0},
                                    {1, 0},   {-1, 1}, {0, 1},  {1, 1}};

inline long long now_ns() {
    return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch())
        .count();
}

Simulation::Simulation(mailbox::SimulationConfig::Snapshot cfg)
    : m_world(), m_grid(), m_pool(std::make_unique<SimulationThreadPool>(1)),
      m_mail_cmd(), m_mail_draw(), m_mail_cfg(), m_mail_stats() {
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

    push_command({mailbox::command::Command::Kind::Quit});
    if (m_thread.joinable())
        m_thread.join();
}

void Simulation::pause() {
    push_command({mailbox::command::Command::Kind::Pause});
}

void Simulation::resume() {
    push_command({mailbox::command::Command::Kind::Resume});
}

void Simulation::reset() {
    push_command({mailbox::command::Command::Kind::ResetWorld});
}

void Simulation::update_config(mailbox::SimulationConfig::Snapshot &cfg) {
    m_mail_cfg.publish(cfg);
}

void Simulation::push_command(const mailbox::command::Command &cmd) {
    m_mail_cmd.push(cmd);
}

const std::vector<float> &Simulation::read_current_draw() {
    return m_mail_draw.read_current_only();
}

mailbox::DrawBuffer::ReadView Simulation::begin_read_draw() {
    return m_mail_draw.begin_read();
}

void Simulation::end_read_draw(const mailbox::DrawBuffer::ReadView &view) {
    m_mail_draw.end_read(view);
}

mailbox::SimulationStats::Snapshot Simulation::get_stats() const {
    return m_mail_stats.acquire();
}

mailbox::SimulationConfig::Snapshot Simulation::get_config() const {
    return m_mail_cfg.acquire();
}

const World &Simulation::get_world() const { return m_world; }

void Simulation::step(mailbox::SimulationConfig::Snapshot &cfg) {
    const int particles_count = m_world.get_particles_count();
    if (particles_count == 0)
        return;

    if ((int)m_fx.size() != particles_count)
        m_fx.resize(particles_count);
    if ((int)m_fy.size() != particles_count)
        m_fy.resize(particles_count);
    std::fill_n(m_fx.data(), particles_count, 0.f);
    std::fill_n(m_fy.data(), particles_count, 0.f);

    KernelData data;
    data.particles_count = particles_count;
    data.k_time_scale = cfg.time_scale;
    data.k_viscosity = cfg.viscosity;
    data.k_inverse_viscosity = 1.f - cfg.viscosity;
    data.k_wall_repel = cfg.wallRepel;
    data.k_wall_strength = cfg.wallStrength;
    data.width = cfg.bounds_width;
    data.height = cfg.bounds_height;
    data.fx = m_fx.data();
    data.fy = m_fy.data();

    float maxR = std::max(1.0f, m_world.max_interaction_radius());

    // FIXME: only resize of bounds, maxR or N changes
    // possibly resize in world reset/update commands etc
    m_grid.resize(cfg.bounds_width, cfg.bounds_height, maxR, particles_count);

    m_grid.build(
        particles_count,
        [this](int i) {
            return this->m_world.get_px(i);
        },
        [this](int i) {
            return this->m_world.get_py(i);
        },
        cfg.bounds_width, cfg.bounds_height);

    data.inverse_cell = m_grid.inv_cell();

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

int Simulation::ensure_pool(int t, mailbox::SimulationConfig::Snapshot &cfg) {
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
    static long long num_steps = 0;
    num_steps++;

    auto now = steady_clock::now();
    if (now - m_t_window_start >= 1s) {
        int secs = (int)duration_cast<seconds>(now - m_t_window_start).count();
        if (secs < 1)
            secs = 1;
        m_t_last_published_tps = m_t_window_steps / secs;

        mailbox::SimulationStats::Snapshot st;
        st.effective_tps = m_t_last_published_tps;
        st.particles = m_world.get_particles_count();
        st.groups = m_world.get_groups_size();
        st.sim_threads = n_threads;
        st.last_step_ns = step_diff_ns.count();
        st.published_ns = now_ns();
        if (can_step()) {
            st.num_steps = num_steps;
        }
        m_mail_stats.publish(st);

        m_t_window_steps = 0;
        m_t_window_start = now;
    }
}

void Simulation::wait_on_tps(int tps) noexcept {
    if (tps <= 0) {
        return;
    }

    const auto target_frame_time = nanoseconds(1'000'000'000LL / tps);
    const auto now = steady_clock::now();
    const auto elapsed = now - m_t_last_step_time;

    if (elapsed < target_frame_time) {
        std::this_thread::sleep_for(target_frame_time - elapsed);
    }

    m_t_last_step_time = steady_clock::now();
}

void Simulation::loop_thread() {
    auto cfg = get_config();

    seed_world(cfg);
    const int particle_count = m_world.get_particles_count();
    m_mail_draw.bootstrap_same_as_current(size_t(particle_count) * 2, now_ns());

    m_t_last_step_time = steady_clock::now();
    m_t_window_start = m_t_last_step_time;
    m_t_window_steps = 0;
    m_t_last_published_tps = 0;
    int last_threads = -9999;

    while (m_t_run_state != RunState::Quit) {
        last_threads = ensure_pool(last_threads, cfg);
        if (can_step()) {
            m_grid.reset();
        }

        process_commands(cfg);

        if (m_t_run_state == RunState::Quit) {
            break;
        }

        auto step_begin_ns = steady_clock::now();
        if (can_step()) {
            step(cfg);
        }
        auto step_end_ns = steady_clock::now();
        m_t_window_steps++;

        publish_draw(cfg);
        measure_tps(last_threads, (step_end_ns - step_begin_ns));
        wait_on_tps(cfg.target_tps);

        if (m_t_run_state == RunState::OneStep) {
            m_t_run_state = RunState::Paused;
        }

        cfg = get_config();
    }
}

void Simulation::process_commands(mailbox::SimulationConfig::Snapshot &cfg) {
    for (const mailbox::command::Command &cmd : m_mail_cmd.drain()) {
        switch (cmd.kind) {
        case mailbox::command::Command::Kind::OneStep:
            m_t_run_state = RunState::OneStep;
            break;
        case mailbox::command::Command::Kind::Pause:
            m_t_run_state = RunState::Paused;
            break;
        case mailbox::command::Command::Kind::Resume:
            m_t_run_state = RunState::Running;
            break;
        case mailbox::command::Command::Kind::ResetWorld:
            seed_world(cfg);
            m_t_window_steps = 0;
            m_t_window_start = steady_clock::now();
            break;

        case mailbox::command::Command::Kind::ApplyRules:
            if (cmd.rules) {
                const int groups_count = m_world.get_groups_size();
                const mailbox::command::RulePatch &p = *cmd.rules;
                auto apply_colors_if_any = [&](int Gnow) {
                    if (!p.colors.empty() && (int)p.colors.size() == Gnow) {
                        for (int i = 0; i < Gnow; ++i)
                            m_world.set_group_color(i, p.colors[i]);
                    }
                };

                if (p.groups == groups_count && p.hot) {
                    // Hot apply: update r2, rules, colors; keep
                    // positions/velocities.
                    for (int g = 0; g < groups_count; ++g)
                        m_world.set_r2(g, p.r2[g]);
                    for (int i = 0; i < groups_count; ++i) {
                        const float *row = p.rules.data() + i * groups_count;
                        for (int j = 0; j < groups_count; ++j)
                            m_world.set_rule(i, j, row[j]);
                    }
                    apply_colors_if_any(groups_count);
                } else {
                    // Cold apply / group structure changed
                    const int Gnow = m_world.get_groups_size();
                    m_world.init_rule_tables(Gnow);
                    for (int g = 0; g < std::min(Gnow, p.groups); ++g)
                        m_world.set_r2(g, p.r2[g]);
                    for (int i = 0; i < std::min(Gnow, p.groups); ++i) {
                        const float *row = p.rules.data() + i * p.groups;
                        for (int j = 0; j < std::min(Gnow, p.groups); ++j)
                            m_world.set_rule(i, j, row[j]);
                    }
                    apply_colors_if_any(Gnow);

                    seed_world(cfg);
                    m_t_window_steps = 0;
                    m_t_window_start = steady_clock::now();
                }
            }
            break;

        case mailbox::command::Command::Kind::AddGroup:
            if (cmd.add_group) {
                const auto &ag = *cmd.add_group;
                m_world.add_group(ag.size, ag.color);
                m_world.finalize_groups();
                m_world.init_rule_tables(m_world.get_groups_size());
                // default: zero rules; set radius for the new group index
                int gn = m_world.get_groups_size() - 1;
                m_world.set_r2(gn, ag.r2);
                // Seed new particles positions/velocities (simple random)
                // Reuse seed_world mechanics but keep existing ones:
                {
                    std::mt19937 rng{std::random_device{}()};
                    std::uniform_real_distribution<float> rx(0.f,
                                                             cfg.bounds_width);
                    std::uniform_real_distribution<float> ry(0.f,
                                                             cfg.bounds_height);
                    const int start = m_world.get_group_start(gn);
                    const int end = m_world.get_group_end(gn);
                    for (int i = start; i < end; ++i) {
                        m_world.set_px(i, rx(rng));
                        m_world.set_py(i, ry(rng));
                        m_world.set_vx(i, 0.f);
                        m_world.set_vy(i, 0.f);
                    }
                }
            }
            break;

        case mailbox::command::Command::Kind::RemoveGroup:
            if (cmd.rem_group) {
                int gi = cmd.rem_group->group_index;
                const int G = m_world.get_groups_size();
                if (gi >= 0 && gi < G) {
                    m_world.remove_group(gi);
                    m_world.finalize_groups();
                    m_world.init_rule_tables(m_world.get_groups_size());
                    seed_world(cfg);
                    m_t_window_steps = 0;
                    m_t_window_start = steady_clock::now();
                }
            }
            break;

        case mailbox::command::Command::Kind::Quit:
            m_t_run_state = RunState::Quit;
            break;
        }
    }
}

void Simulation::publish_draw(mailbox::SimulationConfig::Snapshot &cfg) {
    const int particles_count = m_world.get_particles_count();

    auto &pos = m_mail_draw.begin_write_pos(size_t(particles_count) * 2);
    auto &vel = m_mail_draw.begin_write_vel(size_t(particles_count) * 2);
    auto &grid_frame = m_mail_draw.begin_write_grid(
        m_grid.cols(), m_grid.rows(), particles_count, m_grid.cell_size(),
        m_grid.width(), m_grid.height());

    for (int i = 0; i < particles_count; ++i) {
        const size_t b = size_t(i) * 2;
        pos[b + 0] = m_world.get_px(i);
        pos[b + 1] = m_world.get_py(i);
        vel[b + 0] = m_world.get_vx(i);
        vel[b + 1] = m_world.get_vy(i);
    }

    if (cfg.draw_report.grid_data) {
        grid_frame.head = m_grid.head();
        grid_frame.next = m_grid.next();

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

inline void Simulation::kernel_force(int start, int end, KernelData &data) {
    for (int i = start; i < end; ++i) {
        const float ax = m_world.get_px(i);
        const float ay = m_world.get_py(i);
        const int gi = m_world.group_of(i);
        const float r2 = m_world.r2_of(gi);

        if (r2 <= 0.f) {
            data.fx[i] = 0.f;
            data.fy[i] = 0.f;

            continue;
        }

        float sumx = 0.f, sumy = 0.f;
        int cx = std::min(int(ax * data.inverse_cell), m_grid.cols() - 1);
        int cy = std::min(int(ay * data.inverse_cell), m_grid.rows() - 1);

        const float *__restrict row = m_world.rules_row(gi);

        for (int k = 0; k < 9; ++k) {
            const int nci = m_grid.cell_index(cx + grid_offsets[k][0],
                                              cy + grid_offsets[k][1]);

            if (nci < 0) {
                continue;
            }

            for (int j = m_grid.head_at(nci); j != -1; j = m_grid.next_at(j)) {
                if (j == i) {
                    continue;
                }

                const float bx = m_world.get_px(j);
                const float by = m_world.get_py(j);
                const float dx = ax - bx;
                const float dy = ay - by;
                const float d2 = dx * dx + dy * dy;

                if (d2 > 0.f && d2 < r2) {
                    const int gj = m_world.group_of(j);
                    const float g = row[gj];
                    const float invd =
                        rsqrt_fast(std::max(d2, EPS)); // 1/sqrt(d2)
                    const float F = g * invd;

                    sumx += F * dx;
                    sumy += F * dy;
                }
            }
        }

        if (data.k_wall_repel > 0.f) {
            const float d = data.k_wall_repel;
            const float sW = data.k_wall_strength;

            if (ax < d) {
                sumx += (d - ax) * sW;
            }
            if (ax > data.width - d) {
                sumx += (data.width - d - ax) * sW;
            }
            if (ay < d) {
                sumy += (d - ay) * sW;
            }
            if (ay > data.height - d) {
                sumy += (data.height - d - ay) * sW;
            }
        }

        data.fx[i] = sumx;
        data.fy[i] = sumy;
    }
}

inline void Simulation::kernel_vel(int start, int end, KernelData &data) {
    for (int i = start; i < end; ++i) {
        const float vx = m_world.get_vx(i) * data.k_inverse_viscosity +
                         data.fx[i] * data.k_time_scale;
        const float vy = m_world.get_vy(i) * data.k_inverse_viscosity +
                         data.fy[i] * data.k_time_scale;

        m_world.set_vx(i, vx);
        m_world.set_vy(i, vy);
    }
}

inline void Simulation::kernel_pos(int start, int end, KernelData &data) {
    for (int i = start; i < end; ++i) {
        float x = m_world.get_px(i) + m_world.get_vx(i);
        float y = m_world.get_py(i) + m_world.get_vy(i);
        float vx = m_world.get_vx(i);
        float vy = m_world.get_vy(i);

        if (x < 0.f) {
            x = -x;
            vx = -vx;
        }
        if (x >= data.width) {
            x = 2.f * data.width - x;
            vx = -vx;
        }
        if (y < 0.f) {
            y = -y;
            vy = -vy;
        }
        if (y >= data.height) {
            y = 2.f * data.height - y;
            vy = -vy;
        }

        m_world.set_px(i, x);
        m_world.set_py(i, y);
        m_world.set_vx(i, vx);
        m_world.set_vy(i, vy);
    }
}

void Simulation::seed_world(mailbox::SimulationConfig::Snapshot &cfg) {
    m_world.reset(false);

    std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> rx(0.f, cfg.bounds_width);
    std::uniform_real_distribution<float> ry(0.f, cfg.bounds_height);

    int sz = 1500;
    const int gG = m_world.add_group(sz, {0, 228, 114, 255});
    const int gR = m_world.add_group(sz, {238, 70, 82, 255});
    const int gO = m_world.add_group(sz, {227, 172, 72, 255});
    const int gB = m_world.add_group(sz, {0, 121, 241, 255});
    const int gP = m_world.add_group(sz, {200, 122, 255, 255});

    const int N = m_world.get_particles_count();
    for (int i = 0; i < N; ++i) {
        m_world.set_px(i, rx(rng));
        m_world.set_py(i, ry(rng));
        m_world.set_vx(i, 0.f);
        m_world.set_vy(i, 0.f);
    }

    m_world.finalize_groups();
    const int G = m_world.get_groups_size();
    m_world.init_rule_tables(G);

    auto r = 80.f;
    m_world.set_r2(gG, r * r);
    m_world.set_r2(gR, r * r);
    m_world.set_r2(gO, 96.6f * 96.6f);
    m_world.set_r2(gB, r * r);
    m_world.set_r2(gP, r * r);

    m_world.set_rule(gG, gG, +0.926f);
    m_world.set_rule(gG, gR, -0.834f);
    m_world.set_rule(gG, gO, +0.281f);
    m_world.set_rule(gG, gB, -0.0642730798572301f);
    m_world.set_rule(gG, gP, +0.5173874347821623f);

    m_world.set_rule(gR, gG, -0.4617096465080976f);
    m_world.set_rule(gR, gR, +0.4914243463426828f);
    m_world.set_rule(gR, gO, +0.2760726027190685f);
    m_world.set_rule(gR, gB, +0.6413487386889756f);
    m_world.set_rule(gR, gP, -0.7276545553729321f);

    m_world.set_rule(gO, gG, -0.7874764292500913f);
    m_world.set_rule(gO, gR, +0.2337338547222316f);
    m_world.set_rule(gO, gO, -0.0241123312152922f);
    m_world.set_rule(gO, gB, -0.7487592226825655f);
    m_world.set_rule(gO, gP, +0.2283666329376234f);

    m_world.set_rule(gB, gG, +0.5655814143829048f);
    m_world.set_rule(gB, gR, +0.9484694371931255f);
    m_world.set_rule(gB, gO, -0.3605288732796907f);
    m_world.set_rule(gB, gB, +0.4411409106105566f);
    m_world.set_rule(gB, gP, -0.3176638387632344f);

    m_world.set_rule(gP, gG, std::sin(1.0f));
    m_world.set_rule(gP, gR, std::cos(2.0f));
    m_world.set_rule(gP, gO, +1.0f);
    m_world.set_rule(gP, gB, -1.0f);
    m_world.set_rule(gP, gP, +3.14f);
}