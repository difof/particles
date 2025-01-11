#include "simulation.hpp"

constexpr float EPS = 1e-12f;

constexpr int grid_offsets[9][2] = {{-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {0, 0},
                                    {1, 0},   {-1, 1}, {0, 1},  {1, 1}};

inline long long now_ns() {
    using namespace std::chrono;
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
    if (m_t_running) {
        return;
    }
    m_t_running = true;
    m_thread = std::thread(&Simulation::loop_thread, this);
    resume();
}

void Simulation::end() {
    if (!m_t_running && !m_thread.joinable()) {
        return;
    }
    push_command({mailbox::command::Command::Kind::Quit});
    if (m_thread.joinable())
        m_thread.join();
    m_t_running = false;
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

void Simulation::seed_world(mailbox::SimulationConfig::Snapshot &cfg) {
    m_world.reset(false);

    std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> rx(0.f, cfg.bounds_width);
    std::uniform_real_distribution<float> ry(0.f, cfg.bounds_height);

    int sz = 1500;
    const int gG = m_world.add_group(sz, GREEN);
    const int gR = m_world.add_group(sz, RED);
    const int gO = m_world.add_group(sz, ORANGE);
    const int gB = m_world.add_group(sz, BLUE);
    const int gP = m_world.add_group(sz, PURPLE);

    const int N = m_world.get_particles_size();
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
    m_world.set_r2(gO, r * r);
    m_world.set_r2(gB, r * r);
    m_world.set_r2(gP, r * r);

    m_world.set_rule(gG, gG, +0.9261392140761018f);
    m_world.set_rule(gG, gR, -0.8341653244569898f);
    m_world.set_rule(gG, gO, +0.2809289274737239f);
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

void Simulation::step(mailbox::SimulationConfig::Snapshot &cfg) {
    const int particles_count = m_world.get_particles_size();
    if (particles_count == 0)
        return;

    // reuse scratch buffers (no alloc per frame)
    if ((int)m_fx.size() != particles_count)
        m_fx.resize(particles_count);
    if ((int)m_fy.size() != particles_count)
        m_fy.resize(particles_count);
    // zero cheaply
    std::fill_n(m_fx.data(), particles_count, 0.f);
    std::fill_n(m_fy.data(), particles_count, 0.f);

    KernelData data;
    data.particles_count = particles_count;
    data.k_time_scale = cfg.time_scale;
    data.k_viscosity = cfg.viscosity;
    data.k_inverse_viscosity = 1.f - cfg.viscosity;
    data.k_wallRepel = cfg.wallRepel;
    data.k_wallStrength = cfg.wallStrength;
    data.width = cfg.bounds_width;
    data.height = cfg.bounds_height;
    data.fx = m_fx.data();
    data.fy = m_fy.data();

    float maxR = std::max(1.0f, m_world.max_interaction_radius());

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

    data.inverse_cell = 1.0f / m_grid.cell();

    // -------- Phase 1: accumulate forces (parallel) --------
    m_pool->parallel_for_n(
        [&](int s, int e) {
            kernel_force(s, e, data);
        },
        particles_count);
    // -------- Phase 2: velocity update (parallel) --------
    m_pool->parallel_for_n(
        [&](int s, int e) {
            kernel_vel(s, e, data);
        },
        particles_count);
    // -------- Phase 3: position + bounce (parallel) --------
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

// FIXME: I resize pos/vel/grid every frame to the exact size. That’s fine; they
// keep capacity, but I still zero them.

void Simulation::loop_thread() {
    using namespace std::chrono;
    using clock = steady_clock;

    auto cfg = get_config();

    // seed first, THEN bootstrap draw buffers with correct size
    seed_world(cfg);
    const int particle_count = m_world.get_particles_size();
    m_mail_draw.bootstrap_same_as_current(size_t(particle_count) * 2, now_ns());

    auto next = clock::now();
    m_t_window_start = next;
    m_t_window_steps = 0;
    m_t_last_published_tps = 0;
    int last_threads = -9999;

    m_t_running = true;
    while (m_t_running) {
        last_threads = ensure_pool(last_threads, cfg);
        if (!m_t_paused) {
            m_grid.reset();
        }

        process_commands(cfg);

        if (!m_t_running) {
            break;
        }

        const int tps = cfg.target_tps;

        auto step_begin_ns = now_ns();
        if (!m_t_paused) {
            step(cfg);
        }
        auto step_end_ns = now_ns();
        ++m_t_window_steps;

        publish_draw(cfg);

        auto now = clock::now();
        if (now - m_t_window_start >= 1s) {
            int secs =
                (int)duration_cast<seconds>(now - m_t_window_start).count();
            if (secs < 1)
                secs = 1;
            m_t_last_published_tps = m_t_window_steps / secs;

            mailbox::SimulationStats::Snapshot st;
            st.effective_tps = m_t_last_published_tps;
            st.particles = m_world.get_particles_size();
            st.groups = m_world.get_groups_size();
            st.sim_threads = last_threads;
            st.last_step_ns = (step_end_ns - step_begin_ns);
            st.published_ns = now_ns();
            m_mail_stats.publish(st);

            m_t_window_steps = 0;
            m_t_window_start = now;
        }

        if (tps > 0) {
            const nanoseconds step = nanoseconds(1'000'000'000LL / tps);
            m_t_tps_next = clock::now();
            m_t_tps_next += step;
            auto nowc = clock::now();
            if (m_t_tps_next > nowc)
                std::this_thread::sleep_until(m_t_tps_next);
            else
                m_t_tps_next = nowc;
        }

        cfg = get_config();
    }
}

void Simulation::process_commands(mailbox::SimulationConfig::Snapshot &cfg) {
    for (const mailbox::command::Command &cmd : m_mail_cmd.drain()) {
        switch (cmd.kind) {
        case mailbox::command::Command::Kind::Pause:
            m_t_paused = true;
            break;
        case mailbox::command::Command::Kind::Resume:
            m_t_paused = false;
            break;
        case mailbox::command::Command::Kind::ResetWorld:
            seed_world(cfg);
            m_t_window_steps = 0;
            m_t_window_start = std::chrono::steady_clock::now();
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
                    m_t_window_start = std::chrono::steady_clock::now();
                }
            }
            break;

        case mailbox::command::Command::Kind::AddGroup:
            if (cmd.add_group) {
                const auto &ag = *cmd.add_group;
                m_world.add_group(ag.size, ag.color);
                m_world.finalize_groups(); // updates starts/ends
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
                    m_world.init_rule_tables(
                        m_world.get_groups_size()); // rules resized; values
                                                    // zeroed
                    // safest: reseed since order changed & counts moved
                    seed_world(cfg);
                    m_t_window_steps = 0;
                    m_t_window_start = std::chrono::steady_clock::now();
                }
            }
            break;

        case mailbox::command::Command::Kind::Quit:
            m_t_running = false;
            break;
        }
    }
}

void Simulation::publish_draw(mailbox::SimulationConfig::Snapshot &cfg) {
    using clock = std::chrono::steady_clock;

    const int N = m_world.get_particles_size();

    auto &pos = m_mail_draw.begin_write_pos(size_t(N) * 2);
    auto &vel = m_mail_draw.begin_write_vel(size_t(N) * 2);
    auto &g = m_mail_draw.begin_write_grid(m_grid.cols(), m_grid.rows(), N,
                                           m_grid.cell(), m_grid.width(),
                                           m_grid.height());

    for (int i = 0; i < N; ++i) {
        const size_t b = size_t(i) * 2;
        pos[b + 0] = m_world.get_px(i);
        pos[b + 1] = m_world.get_py(i);

        if (cfg.draw_report.velocity_data) {
            vel[b + 0] = m_world.get_vx(i);
            vel[b + 1] = m_world.get_vy(i);
        }
    }

    if (cfg.draw_report.grid_data) {
        g.head = m_grid.head();
        g.next = m_grid.next();

        // compute per-cell counts and velocity sums
        const int C = g.cols * g.rows;
        for (int ci = 0; ci < C; ++ci) {
            int cnt = 0;
            float sx = 0.f, sy = 0.f;

            for (int p = g.head[ci]; p != -1; p = g.next[p]) {
                const size_t b = size_t(p) * 2;
                sx += vel[b + 0];
                sy += vel[b + 1];
                ++cnt;
            }

            g.count[ci] = cnt;
            g.sumVx[ci] = sx;
            g.sumVy[ci] = sy;
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
            const int nci = m_grid.cellIndex(cx + grid_offsets[k][0],
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

        // branch-light wall repel: use max() to accumulate only when outside
        // margin
        if (data.k_wallRepel > 0.f) {
            const float d = data.k_wallRepel;
            const float sW = data.k_wallStrength;

            // left   : + (d - ax) when ax < d  -> max(0, d - ax)
            // right  : + (W - d - ax) when ax > W - d -> -(ax - (W - d))
            sumx += std::max(0.f, d - ax) * sW;
            sumx += -std::max(0.f, ax - (data.width - d)) * sW;

            // bottom : + (d - ay)
            // top    : + (H - d - ay) -> -(ay - (H - d))
            sumy += std::max(0.f, d - ay) * sW;
            sumy += -std::max(0.f, ay - (data.height - d)) * sW;
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

        // bounce using clamp & reflect; single inequality check per axis
        const float nx = std::clamp(x, 0.f, data.width);
        if (nx != x) {
            vx = -vx;
            x = 2.f * nx - x; // reflect
        }
        const float ny = std::clamp(y, 0.f, data.height);
        if (ny != y) {
            vy = -vy;
            y = 2.f * ny - y; // reflect
        }

        m_world.set_px(i, x);
        m_world.set_py(i, y);
        m_world.set_vx(i, vx);
        m_world.set_vy(i, vy);
    }
}