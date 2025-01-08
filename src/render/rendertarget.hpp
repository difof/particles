#ifndef __RENDERER_HPP
#define __RENDERER_HPP

#include "../mailbox/mailbox.hpp"
#include "../simulation/simulation.hpp"
#include "../simulation/world.hpp"
#include "../types.hpp"

const float particle_size = 1.5f;

static inline Color TintRGB(Color c, float k) {
    auto clamp = [](int v) {
        return v < 0 ? 0 : (v > 255 ? 255 : v);
    };
    return Color{(unsigned char)clamp((int)std::lrint(c.r * k)),
                 (unsigned char)clamp((int)std::lrint(c.g * k)),
                 (unsigned char)clamp((int)std::lrint(c.b * k)), 255};
}

static Texture2D get_glow_tex() {
    static bool inited = false;
    static Texture2D tex{};
    if (inited)
        return tex;

    const int S = 64;
    Image img = GenImageColor(S, S, BLANK);

    // Softer, steeper falloff (quartic-ish) so singles pop more
    for (int y = 0; y < S; ++y) {
        for (int x = 0; x < S; ++x) {
            float dx = (x + 0.5f - S * 0.5f) / (S * 0.5f);
            float dy = (y + 0.5f - S * 0.5f) / (S * 0.5f);
            float r = sqrtf(dx * dx + dy * dy);
            float a = 1.0f - r;
            if (a < 0.0f)
                a = 0.0f;
            // steeper center emphasis than quadratic
            // a = a * a * a * a; // quartic
            a = a * a; // quadratic: brighter center, still soft edge
            unsigned char A = (unsigned char)lrintf(a * 255.0f);
            ImageDrawPixel(&img, x, y, (Color){255, 255, 255, A});
        }
    }

    tex = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
    return tex;
}

template <typename PosFn>
static void
draw_particles_with_glow(World &world, int groupsCount, PosFn posAt,
                         Texture2D glow, float coreSize,
                         float outerGlowScale, // big soft halo
                         float outerGlowAlpha, // alpha-blended halo strength
                         float innerGlowScale, // small bright halo
                         float innerAddAlpha   // additive “sparkle”
) {
    const Rectangle src = {0, 0, (float)glow.width, (float)glow.height};
    const Vector2 org = {0, 0};

    // Pass 1: Big soft halo with ALPHA blending (caps cluster brightness)
    BeginBlendMode(BLEND_ALPHA);
    for (int g = 0; g < groupsCount; ++g) {
        const int start = world.get_group_start(g);
        const int end = world.get_group_end(g);
        const Color base = *world.get_group_color(g);
        const Color tint = TintRGB(base, outerGlowAlpha);

        for (int i = start; i < end; ++i) {
            Vector2 p = posAt(i);
            Rectangle dest = {p.x - outerGlowScale, p.y - outerGlowScale,
                              outerGlowScale * 2.0f, outerGlowScale * 2.0f};
            DrawTexturePro(glow, src, dest, org, 0.0f, tint);
        }
    }
    EndBlendMode();

    // Pass 2: Tiny additive inner halo (just a touch so singles pop)
    BeginBlendMode(BLEND_ADDITIVE);
    for (int g = 0; g < groupsCount; ++g) {
        const int start = world.get_group_start(g);
        const int end = world.get_group_end(g);
        const Color base = *world.get_group_color(g);
        const Color tint = TintRGB(base, innerAddAlpha);

        for (int i = start; i < end; ++i) {
            Vector2 p = posAt(i);
            Rectangle dest = {p.x - innerGlowScale, p.y - innerGlowScale,
                              innerGlowScale * 2.0f, innerGlowScale * 2.0f};
            DrawTexturePro(glow, src, dest, org, 0.0f, tint);
        }
    }
    EndBlendMode();

    // Pass 3: Solid core
    for (int g = 0; g < groupsCount; ++g) {
        const int start = world.get_group_start(g);
        const int end = world.get_group_end(g);
        const Color col = *world.get_group_color(g);

        for (int i = start; i < end; ++i) {
            Vector2 p = posAt(i);
            DrawCircleV(p, coreSize, col);
        }
    }
}

void render_tex(World &world, const mailbox::DrawBuffer &dbuf,
                const mailbox::SimulationConfig &scfgb) {
    ClearBackground(Color{0, 0, 0, 255});

    mailbox::SimulationConfig::Snapshot scfg = scfgb.acquire();
    const bool doInterp = scfg.interpolate;

    mailbox::DrawBuffer::ReadView view = dbuf.begin_read();
    const int G = world.get_groups_size();

    Texture2D glow = get_glow_tex();

    /// MARK: Tunables:
    const float coreSize = particle_size;
    const float outerScale = coreSize * 14.0f; // wide soft bed
    const float outerAlpha = 0.60f;            // singles clearly visible
    const float innerScale = coreSize * 3.0f;  // tight inner glow
    const float innerAdd = 0.18f; // brighter core accent (RGB only)

    if (doInterp && view.t0 > 0 && view.t1 > 0 && view.t1 > view.t0 &&
        view.prev && view.curr && view.prev->size() == view.curr->size() &&
        !view.curr->empty()) {

        const std::vector<float> &pos0 = *view.prev;
        const std::vector<float> &pos1 = *view.curr;

        const float delay_ms = scfg.interp_delay_ms;
        const long long now_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count();
        const long long target_ns =
            now_ns - (long long)(delay_ms * 1'000'000.0f);

        float alpha = 0.0f;
        if (target_ns <= view.t0)
            alpha = 0.0f;
        else if (target_ns >= view.t1)
            alpha = 1.0f;
        else
            alpha = float(target_ns - view.t0) / float(view.t1 - view.t0);

        // Lambda to fetch interpolated position for index i
        auto posAt = [&](int i) -> Vector2 {
            const size_t base = (size_t)i * 2;
            if (base + 1 >= pos1.size())
                return Vector2{0, 0};
            const float x0 = pos0[base + 0], y0 = pos0[base + 1];
            const float x1 = pos1[base + 0], y1 = pos1[base + 1];
            return Vector2{x0 + (x1 - x0) * alpha, y0 + (y1 - y0) * alpha};
        };

        draw_particles_with_glow(world, G, posAt, glow, coreSize, outerScale,
                                 outerAlpha, innerScale, innerAdd);

        dbuf.end_read(view);
        return;
    }

    // No interpolation (or mismatched sizes)
    {
        const std::vector<float> &pos = dbuf.read_current_only();

        auto posAt = [&](int i) -> Vector2 {
            const size_t base = (size_t)i * 2;
            if (base + 1 >= pos.size())
                return Vector2{0, 0};
            return Vector2{pos[base + 0], pos[base + 1]};
        };

        draw_particles_with_glow(world, G, posAt, glow, coreSize, outerScale,
                                 outerAlpha, innerScale, innerAdd);
    }

    dbuf.end_read(view);
}

#endif
