#include <stdio.h>
#include <raylib.h>
#include <imgui.h>
#include <rlImGui.h>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <cstddef>
#include <cmath>
#include <random>
#include <algorithm>
#include <thread>
#include <mutex>
#include <atomic>

// Choose at build: -DUSE_X86_SSE or -DUSE_ARM_NEON
#if defined(USE_X86_SSE)
#include <xmmintrin.h>
#elif defined(USE_ARM_NEON)
#include <arm_neon.h>
#endif

inline float rsqrt_nr_once(float x, float y0)
{
    // One Newton–Raphson refinement: y_{n+1} = y_n * (1.5 - 0.5*x*y_n^2)
    return y0 * (1.5f - 0.5f * x * y0 * y0);
}

inline float rsqrt_fast(float x)
{
#if defined(USE_X86_SSE)
    __m128 vx = _mm_set_ss(x);
    __m128 y = _mm_rsqrt_ss(vx); // ~12-bit accurate
    float y0 = _mm_cvtss_f32(y);
    // One NR step → ~1e-4 relative error
    return rsqrt_nr_once(x, y0);
#elif defined(USE_ARM_NEON)
    float32x2_t vx = vdup_n_f32(x);
    float32x2_t y = vrsqrte_f32(vx); // initial approx
    // One NR step using NEON recip-sqrt iterations
    // y = y * (1.5 - 0.5*x*y*y)
    float32x2_t y2 = vmul_f32(y, y);
    float32x2_t halfx = vmul_n_f32(vx, 0.5f);
    float32x2_t threehalves = vdup_n_f32(1.5f);
    y = vmul_f32(y, vsub_f32(threehalves, vmul_f32(halfx, y2)));
    return vget_lane_f32(y, 0);
#else
    // Scalar fallback: Quake-style bit hack + one NR step
    // Good speedup on -O3; accurate enough for forces
    float xhalf = 0.5f * x;
    int i = *(int *)&x; // type-pun (OK on most compilers; or use std::bit_cast in C++20)
    i = 0x5f3759df - (i >> 1);
    float y = *(float *)&i;
    y = y * (1.5f - xhalf * y * y); // one NR step
    return y;
#endif
}

struct UniformGrid
{
    float cell = 64.f;      // side length of a cell (auto-picked)
    int cols = 1, rows = 1; // grid size
    std::vector<int> head;  // size cols*rows, index of first particle in cell (-1 if none)
    std::vector<int> next;  // size N, next particle in same cell (-1 if none)

    inline int cellIndex(int cx, int cy) const
    {
        if (cx < 0 || cy < 0 || cx >= cols || cy >= rows)
            return -1;
        return cy * cols + cx;
    }

    inline void resize(float W, float H, float cellSize, int N)
    {
        cell = std::max(1.0f, cellSize);
        cols = std::max(1, (int)std::floor(W / cell));
        rows = std::max(1, (int)std::floor(H / cell));
        head.assign(cols * rows, -1);
        next.assign(N, -1);
    }

    template <typename GetX, typename GetY>
    inline void build(int N, GetX getx, GetY gety, float W, float H)
    {
        // assume resize already called for current N and bounds
        std::fill(head.begin(), head.end(), -1);
        if ((int)next.size() != N)
            next.assign(N, -1);

        for (int i = 0; i < N; ++i)
        {
            float x = getx(i);
            float y = gety(i);
            // Clamp to [0, W/H) so indices are valid (positions bounce into range)
            x = std::min(std::max(0.0f, x), std::nextafter(W, 0.0f));
            y = std::min(std::max(0.0f, y), std::nextafter(H, 0.0f));
            int cx = (int)std::floor(x / cell);
            int cy = (int)std::floor(y / cell);
            int ci = cellIndex(cx, cy);
            // push-front into list
            next[i] = head[ci];
            head[ci] = i;
        }
    }
};

struct WindowConfig
{
    int screen_width, screen_height, panel_width, render_width;
};

struct DrawBuffers
{
    std::vector<float> pos[2]; // [i*2+0]=px, [i*2+1]=py
    std::atomic<int> front{0}; // index of readable buffer
};

struct SimConfig
{
    float bounds_width, bounds_height;

    // Tunables (atomic -> UI writes are race-free)
    std::atomic<float> time_scale{1.0f};   // settings.time_scale
    std::atomic<float> viscosity{0.0f};    // 0..1
    std::atomic<float> gravity{0.0f};      // adds to fy each step
    std::atomic<float> wallRepel{0.0f};    // margin distance; 0 disables
    std::atomic<float> wallStrength{0.1f}; // strength at the wall

    // Pulse (0 disables)
    std::atomic<float> pulse{0.0f};
    std::atomic<float> pulse_x{0.0f};
    std::atomic<float> pulse_y{0.0f};

    std::atomic<bool> sim_running{true};
    std::atomic<int> target_tps{0};
    std::atomic<int> effective_tps{0};
    std::atomic<bool> interpolate{false};

    // UI -> sim handoff for safe world reseed
    std::atomic<bool> reset_requested{false};
};

struct Interaction
{
    int g_src, g_dst;
    float force;
};

class World
{
    // each particle takes 4 items
    // px, py, vx, vy
    std::vector<float> particles;

    // each group takes 2 items
    // p_start, p_end
    std::vector<int> groups;

    std::vector<Color> g_colors;
    std::vector<int> p_group;  // size N: group index per particle
    std::vector<float> rules;  // size G*G: rules[src*G + dst]
    std::vector<float> radii2; // size G: interaction radius^2 for source group
public:
    void finalize_groups()
    {
        const int G = get_groups_size();
        p_group.assign(get_particles_size(), 0);
        for (int g = 0; g < G; ++g)
        {
            for (int i = get_group_start(g); i < get_group_end(g); ++i)
                p_group[i] = g;
        }
    }
    void init_rule_tables(int G)
    {
        rules.assign(G * G, 0.f);
        radii2.assign(G, 0.f);
    }
    void set_rule(int g_src, int g_dst, float v) { rules[g_src * get_groups_size() + g_dst] = v; }
    void set_r2(int g_src, float r2) { radii2[g_src] = r2; }

    int group_of(int i) const { return p_group[i]; }
    float rule_val(int gsrc, int gdst) const { return rules[gsrc * get_groups_size() + gdst]; }
    float r2_of(int gsrc) const { return radii2[gsrc]; }

    const float *rules_row(int gsrc) const
    {
        return &rules[gsrc * get_groups_size()];
    }

    float max_interaction_radius() const
    {
        float maxr2 = 0.f;
        for (float v : radii2)
            maxr2 = std::max(maxr2, v);
        return (maxr2 > 0.f) ? std::sqrt(maxr2) : 0.f;
    }

    int add_group(int count, Color color)
    {
        if (count <= 0)
        {
            return -1;
        }

        int start = get_particles_size();
        groups.push_back(start);
        particles.resize(particles.size() + count * 4, 0.);
        groups.push_back(get_particles_size());
        g_colors.push_back(color);
        return g_colors.size() - 1;
    }

    void reset(bool shrink = false)
    {
        particles.clear();
        groups.clear();
        g_colors.clear();
        p_group.clear();
        rules.clear();
        radii2.clear();

        if (shrink)
        {
            std::vector<float>().swap(particles);
            std::vector<int>().swap(groups);
            std::vector<Color>().swap(g_colors);
            std::vector<int>().swap(p_group);
            std::vector<float>().swap(rules);
            std::vector<float>().swap(radii2);
        }
    }

    Color *get_group_color(int g) { return &g_colors[g]; }

    int get_groups_size() const { return (int)groups.size() / 2; }
    int get_group_start(int g) const { return groups[g * 2 + 0]; }
    int get_group_end(int g) const { return groups[g * 2 + 1]; }
    int get_group_size(int g) const { return get_group_end(g) - get_group_start(g); }

    int get_particles_size() { return particles.size() / 4; }

    inline float get_px(int idx) const { return particles[idx * 4 + 0]; }
    inline float get_py(int idx) const { return particles[idx * 4 + 1]; }
    inline float get_vx(int idx) const { return particles[idx * 4 + 2]; }
    inline float get_vy(int idx) const { return particles[idx * 4 + 3]; }
    inline void set_px(int idx, float v) { particles[idx * 4 + 0] = v; }
    inline void set_py(int idx, float v) { particles[idx * 4 + 1] = v; }
    inline void set_vx(int idx, float v) { particles[idx * 4 + 2] = v; }
    inline void set_vy(int idx, float v) { particles[idx * 4 + 3] = v; }

    const std::vector<Color> &colors() const { return g_colors; }
    const std::vector<int> &group_spans() const { return groups; }
    const std::vector<float> &raw() const { return particles; } // for rendering if needed
};

void render_ui(const WindowConfig &wcfg, World &world, SimConfig &scfg)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.);
    ImGui::Begin("main", NULL, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);
    ImGui::SetWindowPos(ImVec2{0.f, 0.f}, ImGuiCond_Always);
    ImGui::SetWindowSize(ImVec2{(float)wcfg.panel_width, (float)wcfg.screen_height}, ImGuiCond_Always);

    {
        ImGui::SeparatorText("Stats");
        ImGui::Text("FPS: %d", GetFPS());
        ImGui::SameLine();
        ImGui::Text("TPS: %d", scfg.effective_tps.load(std::memory_order_relaxed));
        ImGui::Text("Sim Bounds: %.0f x %.0f", scfg.bounds_width, scfg.bounds_height);

        const int G = world.get_groups_size();
        const int N = world.get_particles_size();
        ImGui::Text("Num particles: %d", N);
        ImGui::Text("Num groups: %d", G);
        if (G > 0)
        {
            ImGui::TextUnformatted("Particles per group:");
            for (int g = 0; g < G; ++g)
                ImGui::BulletText("G%d: %d", g, world.get_group_size(g));
        }

        ImGui::SeparatorText("Sim Config");

        // Target TPS
        int tps = scfg.target_tps.load(std::memory_order_relaxed);
        if (ImGui::SliderInt("Target TPS", &tps, 0, 240, "%d", ImGuiSliderFlags_AlwaysClamp))
            scfg.target_tps.store(tps, std::memory_order_relaxed);

        // Interpolate (currently unused in draw, but exposed)
        bool interpolate = scfg.interpolate.load(std::memory_order_relaxed);
        if (ImGui::Checkbox("Interpolate", &interpolate))
            scfg.interpolate.store(interpolate, std::memory_order_relaxed);

        // Time scale
        float time_scale = scfg.time_scale.load(std::memory_order_relaxed);
        if (ImGui::SliderFloat("Time Scale", &time_scale, 0.01f, 2.0f, "%.3f", ImGuiSliderFlags_Logarithmic))
            scfg.time_scale.store(time_scale, std::memory_order_relaxed);

        // Viscosity
        float viscosity = scfg.viscosity.load(std::memory_order_relaxed);
        if (ImGui::SliderFloat("Viscosity", &viscosity, 0.0f, 1.0f, "%.3f"))
            scfg.viscosity.store(viscosity, std::memory_order_relaxed);

        // Gravity
        float gravity = scfg.gravity.load(std::memory_order_relaxed);
        if (ImGui::SliderFloat("Gravity", &gravity, -2.0f, 2.0f, "%.3f"))
            scfg.gravity.store(gravity, std::memory_order_relaxed);

        // Walls
        float wallRepel = scfg.wallRepel.load(std::memory_order_relaxed);
        if (ImGui::SliderFloat("Wall Repel (px)", &wallRepel, 0.0f, 200.0f, "%.1f"))
            scfg.wallRepel.store(wallRepel, std::memory_order_relaxed);

        float wallStrength = scfg.wallStrength.load(std::memory_order_relaxed);
        if (ImGui::SliderFloat("Wall Strength", &wallStrength, 0.0f, 1.0f, "%.3f"))
            scfg.wallStrength.store(wallStrength, std::memory_order_relaxed);

        // Pulse
        float pulse = scfg.pulse.load(std::memory_order_relaxed);
        if (ImGui::SliderFloat("Pulse", &pulse, -2.0f, 2.0f, "%.3f"))
            scfg.pulse.store(pulse, std::memory_order_relaxed);

        // Pulse position (clamped to bounds)
        float px = scfg.pulse_x.load(std::memory_order_relaxed);
        float py = scfg.pulse_y.load(std::memory_order_relaxed);
        if (ImGui::DragFloat2("Pulse Pos (x,y)", &px, 1.0f, 0.0f, 0.0f, "%.0f"))
        {
            px = std::clamp(px, 0.0f, scfg.bounds_width);
            py = std::clamp(py, 0.0f, scfg.bounds_height);
            scfg.pulse_x.store(px, std::memory_order_relaxed);
            scfg.pulse_y.store(py, std::memory_order_relaxed);
        }

        // Safe reset/reseed (handled by sim thread)
        if (ImGui::Button("Reset world"))
            scfg.reset_requested.store(true, std::memory_order_release);
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

// simple seeding
static void seed_world(World &world, const SimConfig &scfg)
{
    world.reset(false);

    std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> rx(0.f, scfg.bounds_width);
    std::uniform_real_distribution<float> ry(0.f, scfg.bounds_height);

    int sz = 1500;
    const int gG = world.add_group(sz, GREEN);
    const int gR = world.add_group(sz, RED);
    const int gO = world.add_group(sz, ORANGE);
    const int gB = world.add_group(sz, BLUE);

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

    world.set_rule(gG, gG, +0.9261392140761018);
    world.set_rule(gG, gR, -0.8341653244569898);
    world.set_rule(gG, gO, +0.2809289274737239);
    world.set_rule(gG, gB, -0.0642730798572301);

    world.set_rule(gR, gG, -0.4617096465080976);
    world.set_rule(gR, gR, +0.4914243463426828);
    world.set_rule(gR, gO, +0.2760726027190685);
    world.set_rule(gR, gB, +0.6413487386889756);

    world.set_rule(gO, gG, -0.7874764292500913);
    world.set_rule(gO, gR, +0.2337338547222316);
    world.set_rule(gO, gO, -0.0241123312152922);
    world.set_rule(gO, gB, -0.7487592226825655);

    world.set_rule(gB, gG, +0.5655814143829048);
    world.set_rule(gB, gR, +0.9484694371931255);
    world.set_rule(gB, gO, -0.3605288732796907);
    world.set_rule(gB, gB, +0.4411409106105566);
}

void simulate_once(World &world, SimConfig &scfg)
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

    static thread_local std::vector<float> fx, fy;
    fx.assign(N, 0.0f);
    fy.assign(N, 0.0f);

    float maxR = world.max_interaction_radius();
    maxR = std::max(1.0f, maxR);

    static thread_local UniformGrid grid;
    grid.resize(scfg.bounds_width, scfg.bounds_height, maxR, N);
    grid.build(
        N,
        [&world](int i)
        { return world.get_px(i); },
        [&world](int i)
        { return world.get_py(i); },
        scfg.bounds_width, scfg.bounds_height);

    const float *P = world.raw().data();
    const float invCell = 1.0f / grid.cell;
    static const int OFFS[9][2] = {{-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {0, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}};
    constexpr float EPS = 1e-12f;

    // accumulate forces
    for (int i = 0; i < N; ++i)
    {
        const float ax = P[i * 4 + 0];
        const float ay = P[i * 4 + 1];
        const int gi = world.group_of(i);
        const float r2 = world.r2_of(gi);
        if (r2 <= 0.f)
        {
            fx[i] = fy[i] = 0.f;
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
                    // const float invd = 1.0f / std::sqrt(std::max(d2, EPS));
                    const float invd = rsqrt_fast(std::max(d2, EPS));

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
                // const float invd = 1.0f / std::sqrt(std::max(d2, EPS));
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
            const float s = k_wallStrength;
            if (ax < d)
                sumx += (d - ax) * s;
            if (ax > scfg.bounds_width - d)
                sumx += (scfg.bounds_width - d - ax) * s;
            if (ay < d)
                sumy += (d - ay) * s;
            if (ay > scfg.bounds_height - d)
                sumy += (scfg.bounds_height - d - ay) * s;
        }

        // Gravity
        sumy += k_gravity;

        fx[i] = sumx;
        fy[i] = sumy;
    }

    // velocity update
    const float vmix = (1.0f - k_viscosity);
    for (int i = 0; i < N; ++i)
    {
        const float vx = world.get_vx(i) * vmix + fx[i] * k_time_scale;
        const float vy = world.get_vy(i) * vmix + fy[i] * k_time_scale;
        world.set_vx(i, vx);
        world.set_vy(i, vy);
    }

    // position update (unchanged)
    const float W = scfg.bounds_width;
    const float H = scfg.bounds_height;
    for (int i = 0; i < N; ++i)
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
}

void simulation_thread_func(World &world, SimConfig &scfg, DrawBuffers &dbuf)
{
    using clock = std::chrono::steady_clock;
    using namespace std::chrono;

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
        // --- apply reset request (UI -> sim) ---
        if (scfg.reset_requested.load(std::memory_order_acquire))
        {
            seed_world(world, scfg); // mutate world only on sim thread

            const int N = world.get_particles_size();
            dbuf.pos[0].assign(N * 2, 0.f);
            dbuf.pos[1].assign(N * 2, 0.f);
            dbuf.front.store(0, std::memory_order_release);

            scfg.reset_requested.store(false, std::memory_order_release);
        }

        const int tps = scfg.target_tps.load(std::memory_order_relaxed);

        simulate_once(world, scfg);
        ++window_steps;

        // fill back buffer
        int back = 1 - dbuf.front.load(std::memory_order_relaxed);
        const int N = world.get_particles_size();
        if ((int)dbuf.pos[back].size() != N * 2)
            dbuf.pos[back].assign(N * 2, 0.f);

        for (int i = 0; i < N; ++i)
        {
            dbuf.pos[back][i * 2 + 0] = world.get_px(i);
            dbuf.pos[back][i * 2 + 1] = world.get_py(i);
        }
        dbuf.front.store(back, std::memory_order_release);

        // effective TPS
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
            continue; // unlimited

        const nanoseconds step = nanoseconds(1'000'000'000LL / tps);
        next += step;
        now = clock::now();
        if (next > now)
            std::this_thread::sleep_until(next);
        else
            next = now; // drop drift
    }
}

void render_tex(World &world, const DrawBuffers &dbuf)
{
    ClearBackground(BLACK);
    // read which buffer is front
    const int front = dbuf.front.load(std::memory_order_acquire);
    const std::vector<float> &pos = dbuf.pos[front];

    // draw each group with its color
    const int G = world.get_groups_size();
    for (int g = 0; g < G; ++g)
    {
        const int start = world.get_group_start(g);
        const int end = world.get_group_end(g);
        const Color col = *world.get_group_color(g);

        for (int i = start; i < end; ++i)
        {
            // guard in case sizes changed (they don't in this demo)
            const size_t base = (size_t)i * 2;
            if (base + 1 >= pos.size())
                break;
            const float x = pos[base + 0];
            const float y = pos[base + 1];

            // small point (DrawPixelV could alias; small circle is clearer)
            DrawCircleV(Vector2{x, y}, 1.5f, col);
        }
    }
}

void run()
{
    int screenW = 2000;
    int screenH = 900;
    int panelW = 400; // screenW * .23;
    int texW = screenW - panelW;
    WindowConfig wcfg = {screenW, screenH, panelW, texW};

    SimConfig scfg = {};
    {
        scfg.bounds_width = (float)wcfg.render_width;
        scfg.bounds_height = (float)wcfg.screen_height;

        scfg.target_tps.store(0, std::memory_order_relaxed);
        scfg.interpolate.store(false, std::memory_order_relaxed);

        scfg.time_scale.store(1.0f, std::memory_order_relaxed);
        scfg.viscosity.store(0.1f, std::memory_order_relaxed);
        scfg.gravity.store(0.0f, std::memory_order_relaxed);
        scfg.wallRepel.store(40.0f, std::memory_order_relaxed);
        scfg.wallStrength.store(0.1f, std::memory_order_relaxed);
        scfg.pulse.store(0.0f, std::memory_order_relaxed);
        scfg.pulse_x.store(scfg.bounds_width * 0.5f, std::memory_order_relaxed);
        scfg.pulse_y.store(scfg.bounds_height * 0.5f, std::memory_order_relaxed);

        scfg.reset_requested.store(true);
    }

    DrawBuffers dbuf;
    World world;

    SetWindowMonitor(1);
    InitWindow(wcfg.screen_width, wcfg.screen_height, "Particles");
    SetTargetFPS(60);
    rlImGuiSetup(true);

    RenderTexture2D tex = LoadRenderTexture(wcfg.render_width, wcfg.screen_height);

    std::thread sim_thread(simulation_thread_func, std::ref(world), std::ref(scfg), std::ref(dbuf));

    while (!WindowShouldClose())
    {
        BeginTextureMode(tex);
        render_tex(world, dbuf);
        EndTextureMode();

        BeginDrawing();
        ClearBackground(BLACK);

        // render texture must be y-flipped due to default OpenGL coordinates (left-bottom)
        DrawTextureRec(tex.texture, (Rectangle){0, 0, (float)tex.texture.width, (float)-tex.texture.height}, (Vector2){wcfg.panel_width, 0}, WHITE);

        rlImGuiBegin();
        render_ui(wcfg, world, scfg);
        rlImGuiEnd();

        EndDrawing();
    }

    scfg.sim_running.store(false, std::memory_order_relaxed);
    sim_thread.join();

    rlImGuiShutdown();
    UnloadRenderTexture(tex);
    CloseWindow();
}

int main()
{
    run();
    return 0;
}