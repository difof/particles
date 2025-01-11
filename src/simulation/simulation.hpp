#ifndef __SIMULATION_HPP
#define __SIMULATION_HPP

#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>
#include <random>
#include <thread>

#include "../mailbox/mailbox.hpp"
#include "../types.hpp"
#include "math.hpp"
#include "multicore.hpp"
#include "uniformgrid.hpp"
#include "world.hpp"

inline long long now_ns() {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch())
        .count();
}

// simple seeding
static void seed_world(World &world,
                       mailbox::SimulationConfig::Snapshot &scfg) {
    world.reset(false);

    std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> rx(0.f, scfg.bounds_width);
    std::uniform_real_distribution<float> ry(0.f, scfg.bounds_height);

    int sz = 1500;
    const int gG = world.add_group(sz, GREEN);
    const int gR = world.add_group(sz, RED);
    const int gO = world.add_group(sz, ORANGE);
    const int gB = world.add_group(sz, BLUE);
    const int gP = world.add_group(sz, PURPLE);

    const int N = world.get_particles_size();
    for (int i = 0; i < N; ++i) {
        world.set_px(i, rx(rng));
        world.set_py(i, ry(rng));
        world.set_vx(i, 0.f);
        world.set_vy(i, 0.f);
    }

    world.finalize_groups();

    const int G = world.get_groups_size();
    world.init_rule_tables(G);

    auto r = 80.f;
    world.set_r2(gG, r * r);
    world.set_r2(gR, r * r);
    world.set_r2(gO, r * r);
    world.set_r2(gB, r * r);
    world.set_r2(gP, r * r);

    world.set_rule(gG, gG, +0.9261392140761018);
    world.set_rule(gG, gR, -0.8341653244569898);
    world.set_rule(gG, gO, +0.2809289274737239);
    world.set_rule(gG, gB, -0.0642730798572301);
    world.set_rule(gG, gP, +0.5173874347821623);

    world.set_rule(gR, gG, -0.4617096465080976);
    world.set_rule(gR, gR, +0.4914243463426828);
    world.set_rule(gR, gO, +0.2760726027190685);
    world.set_rule(gR, gB, +0.6413487386889756);
    world.set_rule(gR, gP, -0.7276545553729321);

    world.set_rule(gO, gG, -0.7874764292500913);
    world.set_rule(gO, gR, +0.2337338547222316);
    world.set_rule(gO, gO, -0.0241123312152922);
    world.set_rule(gO, gB, -0.7487592226825655);
    world.set_rule(gO, gP, +0.2283666329376234);

    world.set_rule(gB, gG, +0.5655814143829048);
    world.set_rule(gB, gR, +0.9484694371931255);
    world.set_rule(gB, gO, -0.3605288732796907);
    world.set_rule(gB, gB, +0.4411409106105566);
    world.set_rule(gB, gP, -0.3176638387632344);

    world.set_rule(gP, gG, std::sin(1.0));
    world.set_rule(gP, gR, std::cos(2.0));
    world.set_rule(gP, gO, +1.0);
    world.set_rule(gP, gB, -1.0);
    world.set_rule(gP, gP, +3.14);
}

void simulate_once(World &world, UniformGrid &grid,
                   mailbox::SimulationConfig::Snapshot &scfg,
                   SimulationThreadPool &pool) {
    const int particles_count = world.get_particles_size();
    if (particles_count == 0) {
        return;
    }

    // snapshot config (avoid repeated atomic loads)
    const float k_time_scale = scfg.time_scale;
    const float k_viscosity = scfg.viscosity;
    const float k_wallRepel = scfg.wallRepel;
    const float k_wallStrength = scfg.wallStrength;

    // Shared force buffers (one per particle, written by disjoint ranges)
    std::vector<float> fx(particles_count, 0.0f), fy(particles_count, 0.0f);

    float maxR = world.max_interaction_radius();
    maxR = std::max(1.0f, maxR);

    // Build the neighbor grid once (read-only after this)
    grid.resize(scfg.bounds_width, scfg.bounds_height, maxR, particles_count);
    grid.build(
        particles_count,
        [&world](int i) {
            return world.get_px(i);
        },
        [&world](int i) {
            return world.get_py(i);
        },
        scfg.bounds_width, scfg.bounds_height);

    const float invCell = 1.0f / grid.cell();
    static const int OFFS[9][2] = {{-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {0, 0},
                                   {1, 0},   {-1, 1}, {0, 1},  {1, 1}};
    constexpr float EPS = 1e-12f;

    // -------- Phase 1: accumulate forces (parallel) --------
    auto force_kernel = [&](int s, int e) {
        for (int i = s; i < e; ++i) {
            const float ax = world.get_px(i);
            const float ay = world.get_py(i);
            const int gi = world.group_of(i);
            const float r2 = world.r2_of(gi);

            if (r2 <= 0.f) {
                fx[i] = 0.f;
                fy[i] = 0.f;
                continue;
            }

            float sumx = 0.f, sumy = 0.f;
            int cx = (int)(ax * invCell);
            int cy = (int)(ay * invCell);

            if (cx >= grid.cols()) {
                cx = grid.cols() - 1;
            }
            if (cy >= grid.rows()) {
                cy = grid.rows() - 1;
            }

            const float *__restrict row = world.rules_row(gi);

            for (int k = 0; k < 9; ++k) {
                const int nci =
                    grid.cellIndex(cx + OFFS[k][0], cy + OFFS[k][1]);

                if (nci < 0) {
                    continue;
                }

                for (int j = grid.head_at(nci); j != -1; j = grid.next_at(j)) {
                    if (j == i) {
                        continue;
                    }

                    const float bx = world.get_px(j);
                    const float by = world.get_py(j);
                    const float dx = ax - bx;
                    const float dy = ay - by;
                    const float d2 = dx * dx + dy * dy;

                    if (d2 > 0.f && d2 < r2) {
                        const int gj = world.group_of(j);
                        const float g = row[gj];
                        const float invd =
                            rsqrt_fast(std::max(d2, EPS)); // 1/sqrt(d2)
                        const float F = g * invd;

                        sumx += F * dx;
                        sumy += F * dy;
                    }
                }
            }

            // Walls
            if (k_wallRepel > 0.f) {
                const float d = k_wallRepel;
                const float sW = k_wallStrength;

                if (ax < d) {
                    sumx += (d - ax) * sW;
                }
                if (ax > scfg.bounds_width - d) {
                    sumx += (scfg.bounds_width - d - ax) * sW;
                }
                if (ay < d) {
                    sumy += (d - ay) * sW;
                }
                if (ay > scfg.bounds_height - d) {
                    sumy += (scfg.bounds_height - d - ay) * sW;
                }
            }

            fx[i] = sumx;
            fy[i] = sumy;
        }
    };

    // -------- Phase 2: velocity update (parallel) --------
    const float vmix = (1.0f - k_viscosity);
    auto vel_kernel = [&](int s, int e) {
        for (int i = s; i < e; ++i) {
            const float vx = world.get_vx(i) * vmix + fx[i] * k_time_scale;
            const float vy = world.get_vy(i) * vmix + fy[i] * k_time_scale;

            world.set_vx(i, vx);
            world.set_vy(i, vy);
        }
    };

    // -------- Phase 3: position + bounce (parallel) --------
    const float width = scfg.bounds_width;
    const float height = scfg.bounds_height;

    auto pos_kernel = [&](int s, int e) {
        for (int i = s; i < e; ++i) {
            float x = world.get_px(i) + world.get_vx(i);
            float y = world.get_py(i) + world.get_vy(i);
            float vx = world.get_vx(i);
            float vy = world.get_vy(i);

            if (x < 0.f) {
                x = -x;
                vx = -vx;
            }
            if (x >= width) {
                x = 2.f * width - x;
                vx = -vx;
            }
            if (y < 0.f) {
                y = -y;
                vy = -vy;
            }
            if (y >= height) {
                y = 2.f * height - y;
                vy = -vy;
            }

            world.set_px(i, x);
            world.set_py(i, y);
            world.set_vx(i, vx);
            world.set_vy(i, vy);
        }
    };

    pool.parallel_for_n(particles_count, force_kernel);
    pool.parallel_for_n(particles_count, vel_kernel);
    pool.parallel_for_n(particles_count, pos_kernel);
}

void publish_draw(World &world, UniformGrid &ug, mailbox::DrawBuffer &dbuf) {
    using clock = std::chrono::steady_clock;

    const int N = world.get_particles_size();

    auto &pos = dbuf.begin_write_pos(size_t(N) * 2);
    auto &vel = dbuf.begin_write_vel(size_t(N) * 2);
    auto &g = dbuf.begin_write_grid(ug.cols(), ug.rows(), N, ug.cell(),
                                    ug.width(), ug.height());

    for (int i = 0; i < N; ++i) {
        const size_t b = size_t(i) * 2;
        pos[b + 0] = world.get_px(i);
        pos[b + 1] = world.get_py(i);
        vel[b + 0] = world.get_vx(i);
        vel[b + 1] = world.get_vy(i);
    }

    g.head = ug.head(); // size cols*rows
    g.next = ug.next(); // size N

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

    const long long tnow_ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            clock::now().time_since_epoch())
            .count();

    dbuf.publish(tnow_ns);
}

void simulation_thread_func(World &world, mailbox::SimulationConfig &scfgb,
                            mailbox::DrawBuffer &dbuf,
                            mailbox::command::Queue &cmdq,
                            mailbox::SimulationStats &statsb) {
    using namespace std::chrono;
    using clock = steady_clock;

    int last_threads = -9999;
    std::unique_ptr<SimulationThreadPool> pool;
    mailbox::SimulationConfig::Snapshot scfg = scfgb.acquire();

    auto ensure_pool = [&]() {
        int cfg = scfg.sim_threads;
        int desired = (cfg <= 0) ? compute_sim_threads() : std::max(1, cfg);
        if (!pool || desired != last_threads) {
            pool = std::make_unique<SimulationThreadPool>(desired);
            last_threads = desired;
        }
    };

    auto next = clock::now();
    auto window_start = next;
    int window_steps = 0;

    const int N0 = world.get_particles_size();
    dbuf.bootstrap_same_as_current(size_t(N0) * 2, now_ns());

    seed_world(world, scfg);

    bool running = true;
    while (running) {
        ensure_pool();
        UniformGrid grid;

        // process any pending UI commands
        for (const mailbox::command::Command &c : cmdq.drain()) {
            switch (c.kind) {
            case mailbox::command::Command::Kind::ResetWorld:
                seed_world(world, scfg);
                window_steps = 0;
                window_start = clock::now();
                break;

            case mailbox::command::Command::Kind::ApplyRules:
                if (c.rules) {
                    const int G = world.get_groups_size();
                    const mailbox::command::RulePatch &p = *c.rules;
                    auto apply_colors_if_any = [&](int Gnow) {
                        if (!p.colors.empty() && (int)p.colors.size() == Gnow) {
                            for (int i = 0; i < Gnow; ++i)
                                world.set_group_color(i, p.colors[i]);
                        }
                    };

                    if (p.groups == G && p.hot) {
                        // Hot apply: update r2, rules, colors; keep
                        // positions/velocities.
                        for (int g = 0; g < G; ++g)
                            world.set_r2(g, p.r2[g]);
                        for (int i = 0; i < G; ++i) {
                            const float *row = p.rules.data() + i * G;
                            for (int j = 0; j < G; ++j)
                                world.set_rule(i, j, row[j]);
                        }
                        apply_colors_if_any(G);
                    } else {
                        // Cold apply / group structure changed
                        const int Gnow = world.get_groups_size();
                        world.init_rule_tables(Gnow);
                        for (int g = 0; g < std::min(Gnow, p.groups); ++g)
                            world.set_r2(g, p.r2[g]);
                        for (int i = 0; i < std::min(Gnow, p.groups); ++i) {
                            const float *row = p.rules.data() + i * p.groups;
                            for (int j = 0; j < std::min(Gnow, p.groups); ++j)
                                world.set_rule(i, j, row[j]);
                        }
                        apply_colors_if_any(Gnow);

                        seed_world(world, scfg);
                        window_steps = 0;
                        window_start = clock::now();
                    }
                }
                break;

            case mailbox::command::Command::Kind::AddGroup:
                if (c.add_group) {
                    const auto &ag = *c.add_group;
                    world.add_group(ag.size, ag.color);
                    world.finalize_groups(); // updates starts/ends
                    world.init_rule_tables(world.get_groups_size());
                    // default: zero rules; set radius for the new group index
                    int gn = world.get_groups_size() - 1;
                    world.set_r2(gn, ag.r2);
                    // Seed new particles positions/velocities (simple random)
                    // Reuse seed_world mechanics but keep existing ones:
                    {
                        std::mt19937 rng{std::random_device{}()};
                        std::uniform_real_distribution<float> rx(
                            0.f, scfg.bounds_width);
                        std::uniform_real_distribution<float> ry(
                            0.f, scfg.bounds_height);
                        const int start = world.get_group_start(gn);
                        const int end = world.get_group_end(gn);
                        for (int i = start; i < end; ++i) {
                            world.set_px(i, rx(rng));
                            world.set_py(i, ry(rng));
                            world.set_vx(i, 0.f);
                            world.set_vy(i, 0.f);
                        }
                    }
                }
                break;

            case mailbox::command::Command::Kind::RemoveGroup:
                if (c.rem_group) {
                    int gi = c.rem_group->group_index;
                    const int G = world.get_groups_size();
                    if (gi >= 0 && gi < G) {
                        world.remove_group(gi);
                        world.finalize_groups();
                        world.init_rule_tables(
                            world.get_groups_size()); // rules resized; values
                                                      // zeroed
                        // safest: reseed since order changed & counts moved
                        seed_world(world, scfg);
                        window_steps = 0;
                        window_start = clock::now();
                    }
                }
                break;

            case mailbox::command::Command::Kind::Quit:
                running = false;
                break;
            }
        }

        if (!running) {
            break;
        }

        const int tps = scfg.target_tps;

        // measure step time
        auto step_begin_ns = now_ns();
        simulate_once(world, grid, scfg, *pool);
        auto step_end_ns = now_ns();
        ++window_steps;

        publish_draw(world, grid, dbuf);

        // publish stats once per second
        auto now = clock::now();
        static int last_published_tps = 0;
        if (now - window_start >= 1s) {
            int secs = (int)duration_cast<seconds>(now - window_start).count();
            if (secs < 1)
                secs = 1;
            last_published_tps = window_steps / secs;

            mailbox::SimulationStats::Snapshot st;
            st.effective_tps = last_published_tps;
            st.particles = world.get_particles_size();
            st.groups = world.get_groups_size();
            st.sim_threads = last_threads;
            st.last_step_ns =
                (step_end_ns - step_begin_ns); // last step duration
            st.published_ns = now_ns();
            statsb.publish(st);

            window_steps = 0;
            window_start = now;
        }

        // pacing
        if (tps > 0) {
            const nanoseconds step = nanoseconds(1'000'000'000LL / tps);
            static auto next = clock::now();
            next += step;
            auto nowc = clock::now();
            if (next > nowc)
                std::this_thread::sleep_until(next);
            else
                next = nowc;
        }

        // pick up any new UI config for next tick
        scfg = scfgb.acquire();
    }
}

#endif