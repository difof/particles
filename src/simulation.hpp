#ifndef __SIMULATION_HPP
#define __SIMULATION_HPP

#include <cmath>
#include <random>
#include <algorithm>
#include <thread>
#include <mutex>
#include <atomic>

#include "math.hpp"
#include "uniformgrid.hpp"
#include "multicore.hpp"
#include "types.hpp"
#include "world.hpp"

// simple seeding
static void seed_world(World &world, const SimConfig &scfg)
{
    world.reset(false);

    std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> rx(0.f, scfg.bounds_width);
    std::uniform_real_distribution<float> ry(0.f, scfg.bounds_height);

    int sz = 1800;
    const int gG = world.add_group(sz, GREEN);
    const int gR = world.add_group(sz, RED);
    const int gO = world.add_group(sz, ORANGE);
    const int gB = world.add_group(sz, BLUE);
    const int gP = world.add_group(sz, PURPLE); // New PURPLE group

    // positions/velocities
    const int N = world.get_particles_size();
    for (int i = 0; i < N; ++i)
    {
        world.set_px(i, rx(rng));
        world.set_py(i, ry(rng));
        world.set_vx(i, 0.f);
        world.set_vy(i, 0.f);
    }

    // finalize per-particle group vector
    world.finalize_groups();

    // Rules matrix + radii² (example values matching your old signs/mags)
    const int G = world.get_groups_size();
    world.init_rule_tables(G);

    // set per-source radii² (JS uses settings.radii2Array[a.group])
    auto r = 100.f; // pick per-group if you like
    world.set_r2(gG, r * r);
    world.set_r2(gR, r * r);
    world.set_r2(gO, r * r);
    world.set_r2(gB, r * r);
    world.set_r2(gP, r * r); // PURPLE radius

    world.set_rule(gG, gG, +0.9261392140761018);
    world.set_rule(gG, gR, -0.8341653244569898);
    world.set_rule(gG, gO, +0.2809289274737239);
    world.set_rule(gG, gB, -0.0642730798572301);
    world.set_rule(gG, gP, +0.5); // G->P: attracted

    world.set_rule(gR, gG, -0.4617096465080976);
    world.set_rule(gR, gR, +0.4914243463426828);
    world.set_rule(gR, gO, +0.2760726027190685);
    world.set_rule(gR, gB, +0.6413487386889756);
    world.set_rule(gR, gP, -0.7); // R->P: repelled

    world.set_rule(gO, gG, -0.7874764292500913);
    world.set_rule(gO, gR, +0.2337338547222316);
    world.set_rule(gO, gO, -0.0241123312152922);
    world.set_rule(gO, gB, -0.7487592226825655);
    world.set_rule(gO, gP, +0.2); // O->P: weakly attracted

    world.set_rule(gB, gG, +0.5655814143829048);
    world.set_rule(gB, gR, +0.9484694371931255);
    world.set_rule(gB, gO, -0.3605288732796907);
    world.set_rule(gB, gB, +0.4411409106105566);
    world.set_rule(gB, gP, -0.3); // B->P: weakly repelled

    // PURPLE's rules (unique/cool):
    world.set_rule(gP, gG, std::sin(1.0)); // P->G: oscillatory
    world.set_rule(gP, gR, std::cos(2.0)); // P->R: oscillatory
    world.set_rule(gP, gO, +1.0);          // P->O: strongly attracted
    world.set_rule(gP, gB, -1.0);          // P->B: strongly repelled
    world.set_rule(gP, gP, +0.0);          // P->P: neutral
}

void simulate_once(World &world, SimConfig &scfg, ThreadPool &pool)
{
    const int N = world.get_particles_size();
    if (N == 0)
        return;

    // snapshot config (avoid repeated atomic loads)
    const float k_time_scale = scfg.time_scale.load(std::memory_order_relaxed);
    const float k_viscosity = scfg.viscosity.load(std::memory_order_relaxed);
    const float k_gravity = scfg.gravity.load(std::memory_order_relaxed);
    const float k_wallRepel = scfg.wallRepel.load(std::memory_order_relaxed);
    const float k_wallStrength = scfg.wallStrength.load(std::memory_order_relaxed);
    const float k_pulse = scfg.pulse.load(std::memory_order_relaxed);
    const float k_pulse_x = scfg.pulse_x.load(std::memory_order_relaxed);
    const float k_pulse_y = scfg.pulse_y.load(std::memory_order_relaxed);

    // Shared force buffers (one per particle, written by disjoint ranges)
    std::vector<float> fx(N, 0.0f), fy(N, 0.0f);

    float maxR = world.max_interaction_radius();
    maxR = std::max(1.0f, maxR);

    // Build the neighbor grid once (read-only after this)
    UniformGrid grid;
    grid.resize(scfg.bounds_width, scfg.bounds_height, maxR, N);
    grid.build(
        N,
        [&world](int i)
        { return world.get_px(i); },
        [&world](int i)
        { return world.get_py(i); },
        scfg.bounds_width, scfg.bounds_height);

    const float *P = world.raw().data(); // px,py,vx,vy packed
    const float invCell = 1.0f / grid.cell;
    static const int OFFS[9][2] = {
        {-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {0, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}};
    constexpr float EPS = 1e-12f;

    // -------- Phase 1: accumulate forces (parallel) --------
    auto force_kernel = [&](int s, int e)
    {
        for (int i = s; i < e; ++i)
        {
            const float ax = P[i * 4 + 0];
            const float ay = P[i * 4 + 1];
            const int gi = world.group_of(i);
            const float r2 = world.r2_of(gi);
            if (r2 <= 0.f)
            {
                fx[i] = 0.f;
                fy[i] = 0.f;
                continue;
            }

            float sumx = 0.f, sumy = 0.f;

            int cx = (int)(ax * invCell);
            int cy = (int)(ay * invCell);
            if (cx >= grid.cols)
                cx = grid.cols - 1;
            if (cy >= grid.rows)
                cy = grid.rows - 1;

            const float *__restrict row = world.rules_row(gi);

            for (int k = 0; k < 9; ++k)
            {
                const int nci = grid.cellIndex(cx + OFFS[k][0], cy + OFFS[k][1]);
                if (nci < 0)
                    continue;

                for (int j = grid.head[nci]; j != -1; j = grid.next[j])
                {
                    if (j == i)
                        continue;

                    const float bx = P[j * 4 + 0];
                    const float by = P[j * 4 + 1];
                    const float dx = ax - bx;
                    const float dy = ay - by;
                    const float d2 = dx * dx + dy * dy;

                    if (d2 > 0.f && d2 < r2)
                    {
                        const int gj = world.group_of(j);
                        const float g = row[gj];
                        const float invd = rsqrt_fast(std::max(d2, EPS)); // 1/sqrt(d2)
                        const float F = g * invd;
                        sumx += F * dx;
                        sumy += F * dy;
                    }
                }
            }

            // Pulse
            if (k_pulse != 0.f)
            {
                const float dx = ax - k_pulse_x;
                const float dy = ay - k_pulse_y;
                const float d2 = dx * dx + dy * dy;
                if (d2 > 0.f)
                {
                    const float invd = rsqrt_fast(std::max(d2, EPS));
                    const float Fp = (100.f * k_pulse * invd) / k_time_scale;
                    sumx += Fp * dx;
                    sumy += Fp * dy;
                }
            }

            // Walls
            if (k_wallRepel > 0.f)
            {
                const float d = k_wallRepel;
                const float sW = k_wallStrength;
                if (ax < d)
                    sumx += (d - ax) * sW;
                if (ax > scfg.bounds_width - d)
                    sumx += (scfg.bounds_width - d - ax) * sW;
                if (ay < d)
                    sumy += (d - ay) * sW;
                if (ay > scfg.bounds_height - d)
                    sumy += (scfg.bounds_height - d - ay) * sW;
            }

            // Gravity
            sumy += k_gravity;

            fx[i] = sumx;
            fy[i] = sumy;
        }
    };

    // -------- Phase 2: velocity update (parallel) --------
    const float vmix = (1.0f - k_viscosity);
    auto vel_kernel = [&](int s, int e)
    {
        for (int i = s; i < e; ++i)
        {
            const float vx = world.get_vx(i) * vmix + fx[i] * k_time_scale;
            const float vy = world.get_vy(i) * vmix + fy[i] * k_time_scale;
            world.set_vx(i, vx);
            world.set_vy(i, vy);
        }
    };

    // -------- Phase 3: position + bounce (parallel) --------
    const float W = scfg.bounds_width;
    const float H = scfg.bounds_height;
    auto pos_kernel = [&](int s, int e)
    {
        for (int i = s; i < e; ++i)
        {
            float x = world.get_px(i) + world.get_vx(i);
            float y = world.get_py(i) + world.get_vy(i);
            float vx = world.get_vx(i);
            float vy = world.get_vy(i);

            if (x < 0.f)
            {
                x = -x;
                vx = -vx;
            }
            if (x >= W)
            {
                x = 2.f * W - x;
                vx = -vx;
            }
            if (y < 0.f)
            {
                y = -y;
                vy = -vy;
            }
            if (y >= H)
            {
                y = 2.f * H - y;
                vy = -vy;
            }

            world.set_px(i, x);
            world.set_py(i, y);
            world.set_vx(i, vx);
            world.set_vy(i, vy);
        }
    };

    pool.parallel_for_n(N, force_kernel);
    pool.parallel_for_n(N, vel_kernel);
    pool.parallel_for_n(N, pos_kernel);
}

void simulation_thread_func(World &world, SimConfig &scfg, DrawBuffers &dbuf)
{
    using clock = std::chrono::steady_clock;
    using namespace std::chrono;

    // persistent pool (created on this sim thread)
    int last_threads = -9999;
    std::unique_ptr<ThreadPool> pool;

    auto ensure_pool = [&]()
    {
        int cfg = scfg.sim_threads.load(std::memory_order_relaxed);
        int desired = (cfg <= 0) ? compute_sim_threads() : std::max(1, cfg);
        if (!pool || desired != last_threads)
        {
            pool = std::make_unique<ThreadPool>(desired);
            last_threads = desired;
        }
    };

    auto next = clock::now();
    auto window_start = next;
    int window_steps = 0;

    // ensure buffers sized
    const int N0 = world.get_particles_size();
    dbuf.pos[0].assign(N0 * 2, 0.f);
    dbuf.pos[1].assign(N0 * 2, 0.f);
    dbuf.front.store(0, std::memory_order_relaxed);

    while (scfg.sim_running.load(std::memory_order_relaxed))
    {
        ensure_pool();

        if (scfg.reset_requested.load(std::memory_order_acquire))
        {
            seed_world(world, scfg);

            const int N = world.get_particles_size();
            dbuf.pos[0].assign(N * 2, 0.f);
            dbuf.pos[1].assign(N * 2, 0.f);
            dbuf.stamp_ns[0].store(0, std::memory_order_relaxed);
            dbuf.stamp_ns[1].store(0, std::memory_order_relaxed);
            dbuf.front.store(0, std::memory_order_release);

            scfg.reset_requested.store(false, std::memory_order_release);
        }

        const int tps = scfg.target_tps.load(std::memory_order_relaxed);

        simulate_once(world, scfg, *pool);
        ++window_steps;

        // --- snapshot publish (unchanged) ---
        int back = 1 - dbuf.front.load(std::memory_order_relaxed);
        const int N = world.get_particles_size();
        if ((int)dbuf.pos[back].size() != N * 2)
            dbuf.pos[back].assign(N * 2, 0.f);
        for (int i = 0; i < N; ++i)
        {
            dbuf.pos[back][i * 2 + 0] = world.get_px(i);
            dbuf.pos[back][i * 2 + 1] = world.get_py(i);
        }
        auto tnow_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                           clock::now().time_since_epoch())
                           .count();
        dbuf.stamp_ns[back].store(tnow_ns, std::memory_order_relaxed);
        dbuf.front.store(back, std::memory_order_release);

        // effective TPS calc
        auto now = clock::now();
        if (now - window_start >= 1s)
        {
            int secs = (int)duration_cast<seconds>(now - window_start).count();
            if (secs < 1)
                secs = 1;
            scfg.effective_tps.store(window_steps / secs, std::memory_order_relaxed);
            window_steps = 0;
            window_start = now;
        }

        if (tps <= 0)
            continue;
        const nanoseconds step = nanoseconds(1'000'000'000LL / tps);
        next += step;
        now = clock::now();
        if (next > now)
            std::this_thread::sleep_until(next);
        else
            next = now;
    }
}

#endif