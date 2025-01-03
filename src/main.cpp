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
    float bounds_force, bounds_radius; // (unused by JS exactness; keep if you want)
    float interact_radius;             // (unused when using per-group radii2)

    // --- JS parameters ---
    float time_scale = 1.0f;   // settings.time_scale
    float viscosity = 0.0f;    // settings.viscosity  (0..1)
    float gravity = 0.0f;      // settings.gravity    (adds to fy)
    float wallRepel = 0.0f;    // settings.wallRepel  (margin d); 0 disables
    float wallStrength = 0.1f; // strength constant used in JS

    // optional pulse (set to 0 to disable)
    float pulse = 0.0f;
    float pulse_x = 0.0f;
    float pulse_y = 0.0f;

    std::atomic<bool> sim_running{true};
    std::atomic<int> target_tps{0};
    std::atomic<int> effective_tps{0};
    std::atomic<bool> interpolate{false};
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

    std::vector<Interaction> interactions;

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

    int add_interaction(int g_src, int g_dst, float force)
    {
        interactions.push_back({g_src, g_dst, force});
        return interactions.size() - 1;
    }

    // TODO: group/interaction add/remove

    Interaction *get_interaction(int i) { return &interactions[i]; }
    const Interaction &interaction_at(int i) const { return interactions[i]; }
    const Interaction *get_interaction(int i) const { return &interactions[i]; }
    Color *get_group_color(int g) { return &g_colors[g]; }

    int get_groups_size() const { return (int)groups.size() / 2; }
    int get_group_start(int g) const { return groups[g * 2 + 0]; }
    int get_group_end(int g) const { return groups[g * 2 + 1]; }
    int get_group_size(int g) const { return get_group_end(g) - get_group_start(g); }

    int get_interactions_size() { return interactions.size(); }
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
    ImGui::SetWindowPos(ImVec2{0., 0.}, ImGuiCond_Always);
    ImGui::SetWindowSize(ImVec2{static_cast<float>(wcfg.panel_width), static_cast<float>(wcfg.screen_height)}, ImGuiCond_Always);
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
            {
                ImGui::BulletText("G%d: %d", g, world.get_group_size(g));
            }
        }

        ImGui::SeparatorText("Sim Config");
        int tps = scfg.target_tps.load(std::memory_order_relaxed);
        if (ImGui::SliderInt("Target TPS", &tps, 0, 60, "%d", ImGuiSliderFlags_AlwaysClamp))
        {
            scfg.target_tps.store(tps, std::memory_order_relaxed);
        }

        ImGui::SliderFloat("Interact radius", &scfg.interact_radius, 10.f, 200.f, "%.0f");
        ImGui::SliderFloat("Bounds force", &scfg.bounds_force, 0.f, 200.f, "%.0f");
        ImGui::SliderFloat("Bounds radius", &scfg.bounds_radius, 0.f, 200.f, "%.0f");
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void simulate_once(World &world, SimConfig &scfg)
{
    const int N = world.get_particles_size();
    if (N == 0)
        return;

    static thread_local std::vector<float> fx, fy;
    fx.assign(N, 0.0f);
    fy.assign(N, 0.0f);

    const int G = world.get_groups_size();

    // === 1) accumulate forces (matches JS) ===
    for (int i = 0; i < N; ++i)
    {
        const float ax = world.get_px(i);
        const float ay = world.get_py(i);
        const int gi = world.group_of(i);
        const float r2 = world.r2_of(gi);

        float sumx = 0.f, sumy = 0.f;

        // pairwise interactions
        for (int j = 0; j < N; ++j)
        {
            const float bx = world.get_px(j);
            const float by = world.get_py(j);

            const float dx = ax - bx;
            const float dy = ay - by;

            // skip exact self only
            if (dx == 0.f && dy == 0.f)
                continue;

            const float d2 = dx * dx + dy * dy;
            if (d2 < r2 && d2 > 0.f)
            {
                const int gj = world.group_of(j);
                const float g = world.rule_val(gi, gj); // rules[src][dst]
                const float invd = 1.0f / std::sqrt(d2);
                const float F = g * invd;
                sumx += F * dx;
                sumy += F * dy;
            }
        }

        // pulse, if enabled
        if (scfg.pulse != 0.f)
        {
            const float dx = ax - scfg.pulse_x;
            const float dy = ay - scfg.pulse_y;
            const float d2 = dx * dx + dy * dy;
            if (d2 > 0.f)
            {
                // JS: F = 100 * pulse / (d * time_scale) with d = sqrt(d2)
                const float d = std::sqrt(d2);
                const float Fp = 100.f * scfg.pulse / (d * scfg.time_scale);
                sumx += Fp * dx;
                sumy += Fp * dy;
            }
        }

        // wall repel, if enabled (linear inside margin d)
        if (scfg.wallRepel > 0.f)
        {
            const float d = scfg.wallRepel;
            const float strength = scfg.wallStrength; // JS used 0.1
            if (ax < d)
                sumx += (d - ax) * strength;
            if (ax > scfg.bounds_width - d)
                sumx += (scfg.bounds_width - d - ax) * strength;
            if (ay < d)
                sumy += (d - ay) * strength;
            if (ay > scfg.bounds_height - d)
                sumy += (scfg.bounds_height - d - ay) * strength;
        }

        // gravity (adds to fy)
        sumy += scfg.gravity;

        fx[i] = sumx;
        fy[i] = sumy;
    }

    // === 2) velocity update (viscosity mix + time_scale) ===
    const float vmix = (1.0f - scfg.viscosity);
    for (int i = 0; i < N; ++i)
    {
        const float vx = world.get_vx(i) * vmix + fx[i] * scfg.time_scale;
        const float vy = world.get_vy(i) * vmix + fy[i] * scfg.time_scale;
        world.set_vx(i, vx);
        world.set_vy(i, vy);
    }

    // === 3) position update + mirror bounce (exact JS borders) ===
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
    const int N = world.get_particles_size();
    dbuf.pos[0].assign(N * 2, 0.f);
    dbuf.pos[1].assign(N * 2, 0.f);
    dbuf.front.store(0, std::memory_order_relaxed);

    while (scfg.sim_running.load(std::memory_order_relaxed))
    {
        const int tps = scfg.target_tps.load(std::memory_order_relaxed);

        simulate_once(world, scfg);
        ++window_steps;

        // fill back buffer
        int back = 1 - dbuf.front.load(std::memory_order_relaxed);
        if ((int)dbuf.pos[back].size() != world.get_particles_size() * 2)
        {
            dbuf.pos[back].assign(world.get_particles_size() * 2, 0.f);
        }
        for (int i = 0, n = world.get_particles_size(); i < n; ++i)
        {
            dbuf.pos[back][i * 2 + 0] = world.get_px(i);
            dbuf.pos[back][i * 2 + 1] = world.get_py(i);
        }
        // publish
        dbuf.front.store(back, std::memory_order_release);

        // update effective TPS once per second window
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

        // unlimited mode: no sleep
        if (tps <= 0)
            continue;

        // schedule next tick and sleep
        const nanoseconds step = nanoseconds(1'000'000'000LL / tps);
        next += step;
        now = clock::now();
        if (next > now)
        {
            std::this_thread::sleep_until(next);
        }
        else
        {
            next = now; // drop drift
        }
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

// simple seeding
static void seed_world(World &world, const SimConfig &scfg)
{
    std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> rx(0.f, scfg.bounds_width);
    std::uniform_real_distribution<float> ry(0.f, scfg.bounds_height);

    const int gG = world.add_group(500, GREEN);
    const int gR = world.add_group(500, RED);
    const int gO = world.add_group(500, ORANGE);
    const int gB = world.add_group(500, BLUE);

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
    auto r = 80.f; // pick per-group if you like
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

void run()
{
    int screenW = 2000;
    int screenH = 900;
    int panelW = 400; // screenW * .23;
    int texW = screenW - panelW;
    WindowConfig wcfg = {screenW, screenH, panelW, texW};

    SimConfig scfg = {};
    {
        scfg.bounds_width = static_cast<float>(wcfg.render_width);
        scfg.bounds_height = static_cast<float>(wcfg.screen_height);
        scfg.bounds_force = 1.;
        scfg.bounds_radius = 40.;
        scfg.interact_radius = 80.;
        scfg.target_tps = 30;
        scfg.interpolate = false;

        scfg.time_scale = 1.0f;
        scfg.viscosity = 0.05f;   // try 0.0..0.2 like the JS UI
        scfg.gravity = 0.0f;      // same default as JS unless you set it
        scfg.wallRepel = 0.0f;    // set >0 to enable (pixels)
        scfg.wallStrength = 0.1f; // JS constant
        scfg.pulse = 0.0f;        // set to non-zero to match JS pulse feature
    }

    DrawBuffers dbuf;
    World world;
    seed_world(world, scfg);

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