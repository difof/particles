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
    const int particles_count = m_world.get_particles_size();
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
        st.particles = m_world.get_particles_size();
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
    const int particle_count = m_world.get_particles_size();
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
    const int particles_count = m_world.get_particles_size();

    auto &pos = m_mail_draw.begin_write_pos(size_t(particles_count) * 2);
    auto &vel = m_mail_draw.begin_write_vel(size_t(particles_count) * 2);
    auto &grid_frame = m_mail_draw.begin_write_grid(
        m_grid.cols(), m_grid.rows(), particles_count, m_grid.cell(),
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

        if (data.k_wallRepel > 0.f) {
            const float d = data.k_wallRepel;
            const float sW = data.k_wallStrength;

            // FIXME: use clamp/min/max instead of branch
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

        // FIXME: use clamp/min/max instead of branch
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

/// MARK: "Cell-like creatures" config
// Groups by index; comments show biological-ish names
// void Simulation::seed_world(mailbox::SimulationConfig::Snapshot &cfg) {
//     m_world.reset(false);

//     std::mt19937 rng{std::random_device{}()};
//     std::uniform_real_distribution<float> rx(0.f, cfg.bounds_width);
//     std::uniform_real_distribution<float> ry(0.f, cfg.bounds_height);
//     std::normal_distribution<float> vth(0.f, 0.25f); // thermal jitter
//     std::normal_distribution<float> n01(0.f, 1.0f);  // placement noise

//     // --- Group sizes (tweak to taste) ---
//     // We'll build ~6 cells; each ring ~120-150 membrane particles.
//     const float n_scale = 2.f;
//     const int G_cells = 6;
//     const int membrane_total = G_cells * 140 * n_scale;
//     const int cyto_total = G_cells * 180 * n_scale;
//     const int nucleus_total = G_cells * 40 * n_scale;
//     const int organelle_total = G_cells * 60 * n_scale;
//     const int signal_total =
//         G_cells * 24 *
//         n_scale; // "chemotactic signals" that tug organelles/cyto

//     // --- Palette (RGBA) ---
//     // 0: membrane, 1: cytoplasm, 2: nucleus, 3: organelle, 4: signal
//     const int g0 = m_world.add_group(membrane_total,
//                                      {230, 70, 90, 255}); // Membrane –
//                                      pink/red
//     const int g1 = m_world.add_group(
//         cyto_total, {255, 190, 60, 255}); // Cytoplasm – warm yellow
//     const int g2 = m_world.add_group(nucleus_total,
//                                      {120, 90, 200, 255}); // Nucleus –
//                                      purple
//     const int g3 = m_world.add_group(organelle_total,
//                                      {80, 200, 160, 255}); // Organelles –
//                                      teal
//     const int g4 = m_world.add_group(signal_total,
//                                      {255, 255, 255, 255}); // Signals –
//                                      white

//     // Layout helpers
//     auto clampf = [](float v, float lo, float hi) {
//         return std::max(lo, std::min(hi, v));
//     };

//     // Precompute cell centers and radii
//     const float W = cfg.bounds_width;
//     const float H = cfg.bounds_height;
//     const float minDim = std::max(60.f, std::min(W, H));

//     std::vector<std::pair<float, float>> centers;
//     centers.reserve(G_cells);
//     {
//         std::uniform_real_distribution<float> cx(0.15f * W, 0.85f * W);
//         std::uniform_real_distribution<float> cy(0.15f * H, 0.85f * H);
//         for (int c = 0; c < G_cells; ++c) {
//             centers.emplace_back(cx(rng), cy(rng));
//         }
//     }

//     // Place particles:
//     //  - Membrane as noisy rings (g0)
//     //  - Cytoplasm as filled disks (g1)
//     //  - Nucleus as tighter inner blobs (g2)
//     //  - Organelles sprinkled inside each cell (g3)
//     //  - Signals: small ring outside membrane to tug organelles/cytoplasm
//     (g4) m_world.finalize_groups();

//     // Ranges per group
//     auto s0 = m_world.get_group_start(g0), e0 = m_world.get_group_end(g0);
//     auto s1 = m_world.get_group_start(g1), e1 = m_world.get_group_end(g1);
//     auto s2 = m_world.get_group_start(g2), e2 = m_world.get_group_end(g2);
//     auto s3 = m_world.get_group_start(g3), e3 = m_world.get_group_end(g3);
//     auto s4 = m_world.get_group_start(g4), e4 = m_world.get_group_end(g4);

//     // --- Radii per cell (outer membrane radius, inner etc.) ---
//     // Slight randomization per cell to avoid uniformity
//     std::vector<float> R_outer(G_cells), R_inner(G_cells),
//     R_nucleus(G_cells),
//         R_signal(G_cells);
//     {
//         std::uniform_real_distribution<float> Rbase(0.08f * minDim,
//                                                     0.12f * minDim);
//         for (int c = 0; c < G_cells; ++c) {
//             float R = 90.f + (float)c * 2.f + Rbase(rng); // base outer
//             radius R_outer[c] = R; R_inner[c] = 0.55f * R;   // cytoplasm
//             “radius” R_nucleus[c] = 0.28f * R; // nucleus radius R_signal[c]
//             = 1.25f * R;  // signals sit just outside the membrane
//         }
//     }

//     // ---- Seed membrane (ring points) ----
//     {
//         int per_cell = membrane_total / G_cells;
//         int idx = s0;
//         for (int c = 0; c < G_cells; ++c) {
//             const auto [cx, cy] = centers[c];
//             const float R = R_outer[c];
//             for (int k = 0; k < per_cell && idx < e0; ++k, ++idx) {
//                 float t = (2.f * float(M_PI) * k) / std::max(1, per_cell);
//                 float jitter_r = 1.5f * n01(rng);
//                 float jitter_t = 0.05f * n01(rng);
//                 float ct = std::cos(t + jitter_t);
//                 float st = std::sin(t + jitter_t);
//                 float x = cx + (R + jitter_r) * ct;
//                 float y = cy + (R + jitter_r) * st;

//                 m_world.set_px(idx, clampf(x, 2.f, W - 2.f));
//                 m_world.set_py(idx, clampf(y, 2.f, H - 2.f));

//                 // Tangential initial velocity -> gentle ring circulation
//                 float vx = -st * 0.30f + vth(rng);
//                 float vy = ct * 0.30f + vth(rng);
//                 m_world.set_vx(idx, vx);
//                 m_world.set_vy(idx, vy);
//             }
//         }
//         // Any leftovers random
//         for (int i = idx; i < e0; ++i) {
//             m_world.set_px(i, rx(rng));
//             m_world.set_py(i, ry(rng));
//             m_world.set_vx(i, vth(rng));
//             m_world.set_vy(i, vth(rng));
//         }
//     }

//     // ---- Seed cytoplasm (disk fill) ----
//     {
//         int per_cell = cyto_total / G_cells;
//         int idx = s1;
//         std::uniform_real_distribution<float> ur(0.f, 1.f);
//         for (int c = 0; c < G_cells; ++c) {
//             const auto [cx, cy] = centers[c];
//             const float R = R_inner[c];
//             for (int k = 0; k < per_cell && idx < e1; ++k, ++idx) {
//                 // random point in disk
//                 float u = ur(rng);
//                 float r = std::sqrt(u) * R;
//                 float t = ur(rng) * 2.f * float(M_PI);
//                 float x = cx + r * std::cos(t) + 1.0f * n01(rng);
//                 float y = cy + r * std::sin(t) + 1.0f * n01(rng);

//                 m_world.set_px(idx, clampf(x, 2.f, W - 2.f));
//                 m_world.set_py(idx, clampf(y, 2.f, H - 2.f));
//                 m_world.set_vx(idx, vth(rng));
//                 m_world.set_vy(idx, vth(rng));
//             }
//         }
//         for (int i = idx; i < e1; ++i) {
//             m_world.set_px(i, rx(rng));
//             m_world.set_py(i, ry(rng));
//             m_world.set_vx(i, vth(rng));
//             m_world.set_vy(i, vth(rng));
//         }
//     }

//     // ---- Seed nucleus (tight inner blob) ----
//     {
//         int per_cell = nucleus_total / G_cells;
//         int idx = s2;
//         std::uniform_real_distribution<float> ur(0.f, 1.f);
//         for (int c = 0; c < G_cells; ++c) {
//             const auto [cx, cy] = centers[c];
//             const float R = R_nucleus[c];
//             for (int k = 0; k < per_cell && idx < e2; ++k, ++idx) {
//                 float u = ur(rng);
//                 float r = std::sqrt(u) * R * 0.9f;
//                 float t = ur(rng) * 2.f * float(M_PI);
//                 float x = cx + r * std::cos(t) + 0.6f * n01(rng);
//                 float y = cy + r * std::sin(t) + 0.6f * n01(rng);

//                 m_world.set_px(idx, clampf(x, 2.f, W - 2.f));
//                 m_world.set_py(idx, clampf(y, 2.f, H - 2.f));
//                 m_world.set_vx(idx, vth(rng));
//                 m_world.set_vy(idx, vth(rng));
//             }
//         }
//         for (int i = idx; i < e2; ++i) {
//             m_world.set_px(i, rx(rng));
//             m_world.set_py(i, ry(rng));
//             m_world.set_vx(i, vth(rng));
//             m_world.set_vy(i, vth(rng));
//         }
//     }

//     // ---- Seed organelles (inside, near inner rim) ----
//     {
//         int per_cell = organelle_total / G_cells;
//         int idx = s3;
//         std::uniform_real_distribution<float> ur(0.f, 1.f);
//         for (int c = 0; c < G_cells; ++c) {
//             const auto [cx, cy] = centers[c];
//             const float R = 0.8f * R_inner[c];
//             for (int k = 0; k < per_cell && idx < e3; ++k, ++idx) {
//                 float u = ur(rng);
//                 float r = std::sqrt(u) * R;
//                 float t = ur(rng) * 2.f * float(M_PI);
//                 float x = cx + r * std::cos(t) + 0.8f * n01(rng);
//                 float y = cy + r * std::sin(t) + 0.8f * n01(rng);

//                 m_world.set_px(idx, clampf(x, 2.f, W - 2.f));
//                 m_world.set_py(idx, clampf(y, 2.f, H - 2.f));
//                 // slight drift to keep cytoplasm alive
//                 m_world.set_vx(idx, 0.15f * std::cos(t) + vth(rng));
//                 m_world.set_vy(idx, 0.15f * std::sin(t) + vth(rng));
//             }
//         }
//         for (int i = idx; i < e3; ++i) {
//             m_world.set_px(i, rx(rng));
//             m_world.set_py(i, ry(rng));
//             m_world.set_vx(i, vth(rng));
//             m_world.set_vy(i, vth(rng));
//         }
//     }

//     // ---- Seed signals (thin ring just outside membrane) ----
//     {
//         int per_cell = signal_total / G_cells;
//         int idx = s4;
//         for (int c = 0; c < G_cells; ++c) {
//             const auto [cx, cy] = centers[c];
//             const float R = R_signal[c];
//             for (int k = 0; k < per_cell && idx < e4; ++k, ++idx) {
//                 float t = (2.f * float(M_PI) * k) / std::max(1, per_cell);
//                 float ct = std::cos(t);
//                 float st = std::sin(t);
//                 float x = cx + (R + 2.0f * n01(rng)) * ct;
//                 float y = cy + (R + 2.0f * n01(rng)) * st;

//                 m_world.set_px(idx, clampf(x, 2.f, W - 2.f));
//                 m_world.set_py(idx, clampf(y, 2.f, H - 2.f));
//                 // slow outward/inward pulsing
//                 m_world.set_vx(idx, 0.06f * ct + vth(rng));
//                 m_world.set_vy(idx, 0.06f * st + vth(rng));
//             }
//         }
//         for (int i = idx; i < e4; ++i) {
//             m_world.set_px(i, rx(rng));
//             m_world.set_py(i, ry(rng));
//             m_world.set_vx(i, vth(rng));
//             m_world.set_vy(i, vth(rng));
//         }
//     }

//     // --- Interaction radii (short-range; membrane slightly longer) ---
//     m_world.init_rule_tables(m_world.get_groups_size());
//     auto r_mem = 78.f;
//     auto r_cyt = 64.f;
//     auto r_nuc = 48.f;
//     auto r_org = 56.f;
//     auto r_sig = 70.f;

//     m_world.set_r2(g0, r_mem * r_mem); // membrane
//     m_world.set_r2(g1, r_cyt * r_cyt); // cytoplasm
//     m_world.set_r2(g2, r_nuc * r_nuc); // nucleus
//     m_world.set_r2(g3, r_org * r_org); // organelles
//     m_world.set_r2(g4, r_sig * r_sig); // signals

//     // --- Rule matrix: positive attract, negative repel ---
//     // Self-cohesion
//     m_world.set_rule(g0, g0, +0.95f); // membrane chains into a ring
//     m_world.set_rule(g1, g1, +0.55f); // cytoplasm gels
//     m_world.set_rule(g2, g2, +0.90f); // nucleus is tight
//     m_world.set_rule(
//         g3, g3, +0.10f); // organelles mild cohesion to avoid scattering too
//         far
//     m_world.set_rule(g4, g4, +0.05f); // signals loosely stick

//     // Membrane <-> others
//     m_world.set_rule(
//         g0, g1,
//         +0.40f); // membrane attracts cytoplasm (keeps it hugging inside)
//     m_world.set_rule(g1, g0,
//                      +0.32f); // slightly weaker back-attraction (edge
//                      tension)
//     m_world.set_rule(g0, g2,
//                      -0.28f); // membrane repels nucleus (pushes it to
//                      center)
//     m_world.set_rule(g2, g0, -0.22f);
//     m_world.set_rule(
//         g0, g3,
//         +0.18f); // organelles like hanging near membrane (cortical motion)
//     m_world.set_rule(g3, g0, +0.22f);
//     m_world.set_rule(
//         g0, g4, -0.45f); // membrane repels external signals (keeps
//         “outside”)
//     m_world.set_rule(g4, g0, -0.10f); // signals don't cross in much

//     // Cytoplasm relations
//     m_world.set_rule(g1, g2, +0.42f); // cytoplasm pulls nucleus
//     m_world.set_rule(g2, g1, +0.50f); // nucleus strongly pulled by cytoplasm
//     m_world.set_rule(
//         g1, g3, +0.30f); // cytoplasm attracts organelles (keeps them inside)
//     m_world.set_rule(g3, g1, +0.36f);
//     m_world.set_rule(
//         g1, g4,
//         +0.20f); // cytoplasm mildly follows signals -> pseudopod-ish bulges
//     m_world.set_rule(g4, g1, +0.12f);

//     // Nucleus vs organelles
//     m_world.set_rule(g2, g3, -0.12f); // slight avoidance to keep nucleus
//     clear m_world.set_rule(g3, g2, -0.10f);

//     // Organelles <-> signals (drives motion)
//     m_world.set_rule(g3, g4, +0.55f); // organelles chase signals
//     m_world.set_rule(g4, g3, +0.40f);

//     // Signals gently attract each other to form moving hotspots
//     m_world.set_rule(g4, g4, +0.08f);

//     // A few asymmetries to keep the system from “freezing”
//     m_world.set_rule(g1, g0, +0.31f); // tiny tweak
//     m_world.set_rule(g3, g1, +0.35f);

//     // Initial low velocities for everyone else (already set) + small thermal
//     // jitter
//     const int N = m_world.get_particles_size();
//     for (int i = 0; i < N; ++i) {
//         // If any particle missed a velocity above, add gentle jitter
//         if (std::abs(m_world.get_vx(i)) + std::abs(m_world.get_vy(i)) <
//         1e-6f) {
//             m_world.set_vx(i, vth(rng));
//             m_world.set_vy(i, vth(rng));
//         }
//     }
// }

/// MARK: "Hot metallic atoms" config
// Groups by index, comments show element names
void Simulation::seed_world(mailbox::SimulationConfig::Snapshot &cfg) {
    m_world.reset(false);

    std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> rx(0.f, cfg.bounds_width);
    std::uniform_real_distribution<float> ry(0.f, cfg.bounds_height);
    // Small thermal jitter for a "hot" feel (keep it gentle)
    std::normal_distribution<float> vth(0.f, 0.45f);

    // Particle count per group (tweak to taste)
    const int sz = 2000;

    // Molten palette (RGBA): whites → yellows → oranges → reds
    const int g0 =
        m_world.add_group(sz, {255, 109, 0, 255}); // Fe (iron) – orange-hot
    const int g1 = m_world.add_group(
        sz, {255, 153, 51, 255}); // Cu (copper) – bright orange
    const int g2 =
        m_world.add_group(sz, {255, 216, 102, 255}); // Ni (nickel) – yellow
    const int g3 = m_world.add_group(
        sz, {255, 245, 235, 255}); // Al (aluminum) – white-hot
    const int g4 = m_world.add_group(
        sz, {255, 64, 32, 255}); // W  (tungsten) – deep red/white core

    // Seed positions & a touch of "temperature" in velocities
    const int N = m_world.get_particles_size();
    for (int i = 0; i < N; ++i) {
        m_world.set_px(i, rx(rng));
        m_world.set_py(i, ry(rng));
        m_world.set_vx(i, vth(rng));
        m_world.set_vy(i, vth(rng));
    }

    m_world.finalize_groups();
    const int G = m_world.get_groups_size();
    m_world.init_rule_tables(G);

    // Short interaction radii (hot liquid metals are very short-range)
    auto rFe = 70.f, rCu = 65.f, rNi = 60.f, rAl = 55.f, rW = 75.f;
    m_world.set_r2(g0, rFe * rFe); // Fe
    m_world.set_r2(g1, rCu * rCu); // Cu
    m_world.set_r2(g2, rNi * rNi); // Ni
    m_world.set_r2(g3, rAl * rAl); // Al
    m_world.set_r2(g4, rW * rW);   // W

    // ---- Interaction rules (cohesion/repulsion) ----
    // Convention: set_rule(sourceGroup, targetGroup, strength)
    // Positive = attraction, Negative = repulsion

    // Self-cohesion (surface tension feel)
    m_world.set_rule(g0, g0, +0.80f); // Fe–Fe
    m_world.set_rule(g1, g1, +0.60f); // Cu–Cu
    m_world.set_rule(g2, g2, +0.70f); // Ni–Ni
    m_world.set_rule(g3, g3, +0.45f); // Al–Al (weaker cohesion)
    m_world.set_rule(g4, g4, +0.90f); // W–W  (dense, tight clusters)

    // Fe with others
    m_world.set_rule(g0, g1, +0.32f); // Fe→Cu (mild alloying)
    m_world.set_rule(g0, g2, +0.50f); // Fe→Ni (stronger affinity)
    m_world.set_rule(g0, g3, +0.12f); // Fe→Al (weak)
    m_world.set_rule(g0, g4, -0.20f); // Fe→W  (phase separation tendency)

    // Cu with others
    m_world.set_rule(g1, g0, +0.28f); // Cu→Fe
    m_world.set_rule(g1, g2, +0.35f); // Cu→Ni
    m_world.set_rule(g1, g3, +0.25f); // Cu→Al
    m_world.set_rule(g1, g4, -0.40f); // Cu→W

    // Ni with others
    m_world.set_rule(g2, g0, +0.48f); // Ni→Fe
    m_world.set_rule(g2, g1, +0.33f); // Ni→Cu
    m_world.set_rule(g2, g3, +0.20f); // Ni→Al
    m_world.set_rule(g2, g4, -0.30f); // Ni→W

    // Al with others (generally weaker/looser)
    m_world.set_rule(g3, g0, +0.10f); // Al→Fe
    m_world.set_rule(g3, g1, +0.22f); // Al→Cu
    m_world.set_rule(g3, g2, +0.18f); // Al→Ni
    m_world.set_rule(g3, g4, -0.45f); // Al→W

    // W with others (tends to repel, dense self-attraction)
    m_world.set_rule(g4, g0, -0.25f); // W→Fe
    m_world.set_rule(g4, g1, -0.42f); // W→Cu
    m_world.set_rule(g4, g2, -0.35f); // W→Ni
    m_world.set_rule(g4, g3, -0.48f); // W→Al

    // Optional subtle asymmetries to keep it lively (prevents dead
    // equilibria)
    // (If you want perfectly symmetric matrices, delete this block.)
    m_world.set_rule(g1, g0, +0.26f); // Cu→Fe slightly < Fe→Cu
    m_world.set_rule(g3, g1, +0.20f); // Al→Cu slightly < Cu→Al
    m_world.set_rule(g2, g0, +0.47f); // Ni→Fe tiny tweak

    // Done: molten alloy vibes with filamenting + coalescing droplets
}

/// MARK: Random soup
// void Simulation::seed_world(mailbox::SimulationConfig::Snapshot &cfg) {
//     m_world.reset(false);

//     std::mt19937 rng{std::random_device{}()};
//     std::uniform_real_distribution<float> rx(0.f, cfg.bounds_width);
//     std::uniform_real_distribution<float> ry(0.f, cfg.bounds_height);

//     int sz = 1500;
//     const int gG = m_world.add_group(sz, {0, 228, 114, 255});
//     const int gR = m_world.add_group(sz, {238, 70, 82, 255});
//     const int gO = m_world.add_group(sz, {227, 172, 72, 255});
//     const int gB = m_world.add_group(sz, {0, 121, 241, 255});
//     const int gP = m_world.add_group(sz, {200, 122, 255, 255});

//     const int N = m_world.get_particles_size();
//     for (int i = 0; i < N; ++i) {
//         m_world.set_px(i, rx(rng));
//         m_world.set_py(i, ry(rng));
//         m_world.set_vx(i, 0.f);
//         m_world.set_vy(i, 0.f);
//     }

//     m_world.finalize_groups();
//     const int G = m_world.get_groups_size();
//     m_world.init_rule_tables(G);

//     auto r = 80.f;
//     m_world.set_r2(gG, r * r);
//     m_world.set_r2(gR, r * r);
//     m_world.set_r2(gO, 96.6f * 96.6f);
//     m_world.set_r2(gB, r * r);
//     m_world.set_r2(gP, r * r);

//     m_world.set_rule(gG, gG, +0.926f);
//     m_world.set_rule(gG, gR, -0.834f);
//     m_world.set_rule(gG, gO, +0.281f);
//     m_world.set_rule(gG, gB, -0.0642730798572301f);
//     m_world.set_rule(gG, gP, +0.5173874347821623f);

//     m_world.set_rule(gR, gG, -0.4617096465080976f);
//     m_world.set_rule(gR, gR, +0.4914243463426828f);
//     m_world.set_rule(gR, gO, +0.2760726027190685f);
//     m_world.set_rule(gR, gB, +0.6413487386889756f);
//     m_world.set_rule(gR, gP, -0.7276545553729321f);

//     m_world.set_rule(gO, gG, -0.7874764292500913f);
//     m_world.set_rule(gO, gR, +0.2337338547222316f);
//     m_world.set_rule(gO, gO, -0.0241123312152922f);
//     m_world.set_rule(gO, gB, -0.7487592226825655f);
//     m_world.set_rule(gO, gP, +0.2283666329376234f);

//     m_world.set_rule(gB, gG, +0.5655814143829048f);
//     m_world.set_rule(gB, gR, +0.9484694371931255f);
//     m_world.set_rule(gB, gO, -0.3605288732796907f);
//     m_world.set_rule(gB, gB, +0.4411409106105566f);
//     m_world.set_rule(gB, gP, -0.3176638387632344f);

//     m_world.set_rule(gP, gG, std::sin(1.0f));
//     m_world.set_rule(gP, gR, std::cos(2.0f));
//     m_world.set_rule(gP, gO, +1.0f);
//     m_world.set_rule(gP, gB, -1.0f);
//     m_world.set_rule(gP, gP, +3.14f);
// }