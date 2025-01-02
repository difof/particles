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

struct SimConfig
{
    float bounds_x;      // width
    float bounds_y;      // height
    float speed;         // applied to (vel + force)
    float radius;        // single interaction radius
    float bounds_force;  // max repel force at the wall
    float border_radius; // repel fades to 0 over this distance from wall
};

struct ParticlesSoA
{
    std::vector<int> id; // group id per particle [0..groups-1]
    std::vector<float> x;
    std::vector<float> y;
    std::vector<float> vx;
    std::vector<float> vy;

    std::vector<Color> colors;
};

struct Interaction
{
    int source_id; // group A
    int target_id; // group B
    float force;   // coefficient used as (force / distance)
};

struct Range
{
    std::size_t begin, end;
};

// Indexed by source_id -> [begin,end) into interactions
using SourceRanges = std::vector<Range>;

// Indexed by group_id -> [begin,end) into the particle arrays
// We initialize particles grouped (contiguous) by id, so this is trivial.
using ParticleRangesByGroup = std::vector<Range>;

struct InitOut
{
    ParticlesSoA particles;
    ParticleRangesByGroup particle_ranges; // [group] -> [begin,end)
    // Note: you still need to fill interactions + src_ranges separately as you like.
};

static inline void hsv_to_rgb(float H, float S, float V, float &r, float &g, float &b)
{
    // H in [0,1), S,V in [0,1]
    float h = H * 6.0f;
    int i = static_cast<int>(std::floor(h));
    float f = h - static_cast<float>(i);
    float p = V * (1.0f - S);
    float q = V * (1.0f - S * f);
    float t = V * (1.0f - S * (1.0f - f));

    switch ((i % 6 + 6) % 6)
    {
    case 0:
        r = V;
        g = t;
        b = p;
        break;
    case 1:
        r = q;
        g = V;
        b = p;
        break;
    case 2:
        r = p;
        g = V;
        b = t;
        break;
    case 3:
        r = p;
        g = q;
        b = V;
        break;
    case 4:
        r = t;
        g = p;
        b = V;
        break;
    case 5:
        r = V;
        g = p;
        b = q;
        break;
    }
}

static inline void apply_bounds_repel_inplace(
    float &px, float &py, float &vx, float &vy, const SimConfig &cfg)
{
    auto repel = [&](float dist, float maxForce, float nx, float ny, float &ax, float &ay)
    {
        if (dist < 0.0f)
            dist = 0.0f;
        if (dist < cfg.border_radius)
        {
            float k = 1.0f - (dist / cfg.border_radius);
            float f = maxForce * k;
            ax += f * nx;
            ay += f * ny;
        }
    };

    float ax = 0.0f, ay = 0.0f;

    // left wall
    repel(px, cfg.bounds_force, +1.0f, 0.0f, ax, ay);
    // right wall
    repel(cfg.bounds_x - px, cfg.bounds_force, -1.0f, 0.0f, ax, ay);
    // bottom
    repel(py, cfg.bounds_force, 0.0f, +1.0f, ax, ay);
    // top
    repel(cfg.bounds_y - py, cfg.bounds_force, 0.0f, -1.0f, ax, ay);

    vx += ax;
    vy += ay;
}

static inline void compute_one_particle(
    std::size_t i,
    ParticlesSoA &P, // in-place read/write
    const std::vector<Interaction> &interactions,
    const SourceRanges &src_ranges,               // by source_id
    const ParticleRangesByGroup &particle_ranges, // by target_id (group)
    const SimConfig &cfg)
{
    const int pid = P.id[i];

    float px = P.x[i];
    float py = P.y[i];
    float vx = P.vx[i];
    float vy = P.vy[i];

    float fx = 0.0f;
    float fy = 0.0f;

    // interactions for this particle's group id
    const Range &sr = src_ranges[pid];
    for (std::size_t k = sr.begin; k < sr.end; ++k)
    {
        const Interaction &it = interactions[k];
        const Range &pr = particle_ranges[it.target_id];

        for (std::size_t j = pr.begin; j < pr.end; ++j)
        {
            const float tx = P.x[j];
            const float ty = P.y[j];

            float dx = tx - px;
            float dy = ty - py;
            float d2 = dx * dx + dy * dy;
            if (d2 <= 0.0f)
                continue;

            if (d2 <= cfg.radius * cfg.radius)
            {
                float d = std::sqrt(d2);
                float s = (it.force / d);
                fx += s * dx;
                fy += s * dy;
            }
        }
    }

    // velocity update
    vx = (vx + fx) * cfg.speed;
    vy = (vy + fy) * cfg.speed;

    // repel from bounds
    apply_bounds_repel_inplace(px, py, vx, vy, cfg);

    // integrate position (Euler)
    px += vx;
    py += vy;

    // write back in the SAME buffer
    P.x[i] = px;
    P.y[i] = py;
    P.vx[i] = vx;
    P.vy[i] = vy;
    // P.id[i] untouched
}

static inline void step_particles_inplace(
    ParticlesSoA &P,
    const std::vector<Interaction> &interactions,
    const SourceRanges &src_ranges,
    const ParticleRangesByGroup &particle_ranges,
    const SimConfig &cfg)
{
    const std::size_t N = P.x.size();
    for (std::size_t i = 0; i < N; ++i)
    {
        compute_one_particle(i, P, interactions, src_ranges, particle_ranges, cfg);
    }
}

// Creates groups * per_group particles, laid out contiguously per group.
// - Positions: random inside bounds
// - Velocities: zero
// - Colors: bright, distinct-ish per group; copied to each particle in the group.
static inline InitOut init_particles_groups(
    std::size_t groups,
    std::size_t per_group,
    const SimConfig &cfg,
    unsigned int seed = std::random_device{}())
{
    InitOut out;
    auto &P = out.particles;

    const std::size_t N = groups * per_group;

    P.id.resize(N);
    P.x.resize(N);
    P.y.resize(N);
    P.vx.assign(N, 0.0f);
    P.vy.assign(N, 0.0f);
    P.colors.resize(N);

    out.particle_ranges.resize(groups);

    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> ux(0.0f, cfg.bounds_x);
    std::uniform_real_distribution<float> uy(0.0f, cfg.bounds_y);

    // Distinct-ish bright colors via golden angle around HSV wheel
    // High saturation & value to stay bright.
    const float S = 0.85f;
    const float V = 0.95f;
    const float golden = 0.6180339887498949f;

    std::vector<float> groupR(groups), groupG(groups), groupB(groups);
    float hue = std::uniform_real_distribution<float>(0.0f, 1.0f)(rng);

    for (std::size_t g = 0; g < groups; ++g)
    {
        hue = std::fmod(hue + golden, 1.0f);
        float r, gg, b;
        hsv_to_rgb(hue, S, V, r, gg, b);
        groupR[g] = r;
        groupG[g] = gg;
        groupB[g] = b;
    }

    std::size_t idx = 0;
    for (std::size_t g = 0; g < groups; ++g)
    {
        const std::size_t begin = idx;
        for (std::size_t k = 0; k < per_group; ++k, ++idx)
        {
            P.id[idx] = static_cast<int>(g);
            P.x[idx] = ux(rng);
            P.y[idx] = uy(rng);
            // vel already zero
            P.colors[idx] = ColorFromNormalized(Vector4{groupR[g], groupG[g], groupB[g], 1.f});
        }
        const std::size_t end = idx;
        out.particle_ranges[g] = Range{begin, end};
    }

    return out;
}

static inline SourceRanges build_source_ranges(
    const std::vector<Interaction> &interactions,
    std::size_t groups)
{
    SourceRanges R;
    R.resize(groups, Range{0, 0});
    if (interactions.empty())
        return R;

    std::size_t i = 0;
    while (i < interactions.size())
    {
        int sid = interactions[i].source_id;
        std::size_t begin = i;
        while (i < interactions.size() && interactions[i].source_id == sid)
            ++i;
        R[sid] = Range{begin, i};
    }
    return R;
}

void ui()
{
    rlImGuiBegin();

    bool open = true;
    ImGui::ShowDemoWindow(&open);

    open = true;
    if (ImGui::Begin("Test Window", &open))
    {
    }
    ImGui::End();

    rlImGuiEnd();
}

int main()
{
    const int screenW = 1280;
    const int screenH = 720;

    InitWindow(screenW, screenH, "Particles SoA (CPU) - raylib");
    SetTargetFPS(60);

    // --- sim config ---
    SimConfig cfg;
    cfg.bounds_x = static_cast<float>(screenW);
    cfg.bounds_y = static_cast<float>(screenH);
    cfg.speed = 0.98f;         // damp a bit
    cfg.radius = 45.0f;        // interaction radius
    cfg.bounds_force = 0.6f;   // wall repel strength
    cfg.border_radius = 40.0f; // distance from wall where repel acts

    // --- init particles (G groups, P per group) ---
    const std::size_t G = 4;                         // groups
    const std::size_t P = 100;                       // per group
    InitOut init = init_particles_groups(G, P, cfg); // particles + particle_ranges
    ParticlesSoA &part = init.particles;
    ParticleRangesByGroup particle_ranges = init.particle_ranges;

    // --- build interactions (group-to-group) ---
    // Example: simple force pattern:
    //   - slight self-repel
    //   - each group attracts the next group and repels the previous (ring-ish)
    std::vector<Interaction> interactions;
    interactions.reserve(G * 3);
    for (int s = 0; s < static_cast<int>(G); ++s)
    {
        int next = (s + 1) % static_cast<int>(G);
        int prev = (s - 1 + static_cast<int>(G)) % static_cast<int>(G);

        interactions.push_back({s, s, +2.0f});    // self “repel-ish”
        interactions.push_back({s, next, -6.0f}); // attract next
        interactions.push_back({s, prev, +8.0f}); // repel prev
    }

    // sort by source_id so build_source_ranges works
    std::sort(interactions.begin(), interactions.end(),
              [](const Interaction &a, const Interaction &b)
              {
                  if (a.source_id != b.source_id)
                      return a.source_id < b.source_id;
                  return a.target_id < b.target_id;
              });

    SourceRanges src_ranges = build_source_ranges(interactions, G);

    // --- main loop ---
    while (!WindowShouldClose())
    {
        // simple input tweaks
        if (IsKeyPressed(KEY_UP))
            cfg.radius = std::min(cfg.radius + 5.0f, 200.0f);
        if (IsKeyPressed(KEY_DOWN))
            cfg.radius = std::max(cfg.radius - 5.0f, 5.0f);
        if (IsKeyPressed(KEY_RIGHT))
            cfg.speed = std::min(cfg.speed + 0.02f, 1.2f);
        if (IsKeyPressed(KEY_LEFT))
            cfg.speed = std::max(cfg.speed - 0.02f, 0.90f);

        // step (in-place)
        step_particles_inplace(part, interactions, src_ranges, particle_ranges, cfg);

        BeginDrawing();
        ClearBackground({8, 10, 18, 255});

        // draw all particles
        const std::size_t N = part.x.size();
        for (std::size_t i = 0; i < N; ++i)
        {
            // clamp to screen for drawing
            int px = (int)part.x[i];
            int py = (int)part.y[i];
            if ((unsigned)px < (unsigned)screenW && (unsigned)py < (unsigned)screenH)
            {
                // tiny dot (DrawPixel is fine; DrawCircle for bigger blobs)
                // DrawPixel(px, py, part.colors[i]);
                DrawCircle(px, py, 1.5f, part.colors[i]);
            }
        }

        // HUD
        DrawText(TextFormat("radius: %.1f  speed: %.2f  N: %d",
                            cfg.radius, cfg.speed, (int)N),
                 10, 10, 16, RAYWHITE);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}