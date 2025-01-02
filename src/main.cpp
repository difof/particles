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
    float bounds_force, bounds_radius;
    float interact_radius;

    std::atomic<bool> sim_running{true};
    std::atomic<int> target_tps;    // set from UI
    std::atomic<int> effective_tps; // read in UI
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

public:
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

    const float R = scfg.interact_radius;
    const float R2 = R * R;

    const int rules = world.get_interactions_size();
    for (int ridx = 0; ridx < rules; ++ridx)
    {
        const Interaction &rule = world.interaction_at(ridx);
        const int a0 = world.get_group_start(rule.g_src);
        const int a1 = world.get_group_end(rule.g_src);
        const int b0 = world.get_group_start(rule.g_dst);
        const int b1 = world.get_group_end(rule.g_dst);

        for (int i = a0; i < a1; ++i)
        {
            const float pix = world.get_px(i);
            const float piy = world.get_py(i);

            float accx = 0.f, accy = 0.f;

            for (int j = b0; j < b1; ++j)
            {
                if (rule.g_src == rule.g_dst && j == i)
                {
                    // self
                    continue;
                }
                const float pjx = world.get_px(j);
                const float pjy = world.get_py(j);

                const float dx = pix - pjx;
                const float dy = piy - pjy;
                const float d2 = dx * dx + dy * dy;

                if (d2 > 0.f && d2 < R2)
                {
                    const float invd = 1.0f / std::sqrt(d2);
                    const float F = rule.force * invd;
                    accx += F * dx;
                    accy += F * dy;
                }
            }

            fx[i] += accx;
            fy[i] += accy;
        }
    }

    // integrate + bounds repel + damping
    for (int i = 0; i < N; ++i)
    {
        float px = world.get_px(i);
        float py = world.get_py(i);
        float vx = world.get_vx(i);
        float vy = world.get_vy(i);

        // apply_bounds_repel_inplace(px, py, vx, vy, scfg);

        vx = (vx + fx[i]) * 0.5f;
        vy = (vy + fy[i]) * 0.5f;
        px += vx;
        py += vy;

        // clamp to bounds
        if (px < 0)
            px = 0;
        else if (px > scfg.bounds_width)
            px = scfg.bounds_width;
        if (py < 0)
            py = 0;
        else if (py > scfg.bounds_height)
            py = scfg.bounds_height;

        world.set_px(i, px);
        world.set_py(i, py);
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

    const int gA = world.add_group(200, GOLD);
    const int gB = world.add_group(200, SKYBLUE);
    const int gC = world.add_group(200, LIME);

    const int N = world.get_particles_size();
    for (int i = 0; i < N; ++i)
    {
        world.set_px(i, rx(rng));
        world.set_py(i, ry(rng));
        world.set_vx(i, 0.f);
        world.set_vy(i, 0.f);
    }

    // interactions (signed force; + attract, - repel)
    world.add_interaction(gA, gA, -20.f);
    world.add_interaction(gA, gB, +10.f);
    world.add_interaction(gA, gC, -15.f);

    world.add_interaction(gB, gA, -10.f);
    world.add_interaction(gB, gB, -20.f);
    world.add_interaction(gB, gC, +12.f);

    world.add_interaction(gC, gA, +15.f);
    world.add_interaction(gC, gB, -12.f);
    world.add_interaction(gC, gC, -20.f);
}

void run()
{
    int screenW = 1280;
    int screenH = 800;
    int panelW = screenW * .23;
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