#ifndef __RENDERTARGET_HPP
#define __RENDERTARGET_HPP

#include "../mailbox/mailbox.hpp"
#include "../simulation/world.hpp"
#include "renderconfig.hpp"
#include <raylib.h>

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
    for (int y = 0; y < S; ++y)
        for (int x = 0; x < S; ++x) {
            float dx = (x + 0.5f - S * 0.5f) / (S * 0.5f);
            float dy = (y + 0.5f - S * 0.5f) / (S * 0.5f);
            float r = sqrtf(dx * dx + dy * dy);
            float a = 1.0f - r;
            if (a < 0)
                a = 0;
            a = a * a; // quadratic
            unsigned char A = (unsigned char)lrintf(a * 255.0f);
            ImageDrawPixel(&img, x, y, (Color){255, 255, 255, A});
        }
    tex = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
    return tex;
}

template <typename PosFn>
static void draw_particles_with_glow(World &world, int groupsCount, PosFn posAt,
                                     Texture2D glow, float coreSize,
                                     float outerScale, float outerRGBGain,
                                     float innerScale, float innerRGBGain) {
    const Rectangle src = {0, 0, (float)glow.width, (float)glow.height};
    const Vector2 org = {0, 0};

    // Pass 1: big soft halo (alpha blend)
    BeginBlendMode(BLEND_ALPHA);
    for (int g = 0; g < groupsCount; ++g) {
        const int start = world.get_group_start(g);
        const int end = world.get_group_end(g);
        const Color tint = TintRGB(*world.get_group_color(g), outerRGBGain);
        for (int i = start; i < end; ++i) {
            Vector2 p = posAt(i);
            Rectangle dest = {p.x - outerScale, p.y - outerScale,
                              outerScale * 2, outerScale * 2};
            DrawTexturePro(glow, src, dest, org, 0, tint);
        }
    }
    EndBlendMode();

    // Pass 2: small bright halo (alpha too; we mod RGB gain only)
    BeginBlendMode(BLEND_ALPHA);
    for (int g = 0; g < groupsCount; ++g) {
        const int start = world.get_group_start(g);
        const int end = world.get_group_end(g);
        const Color tint = TintRGB(*world.get_group_color(g), innerRGBGain);
        for (int i = start; i < end; ++i) {
            Vector2 p = posAt(i);
            Rectangle dest = {p.x - innerScale, p.y - innerScale,
                              innerScale * 2, innerScale * 2};
            DrawTexturePro(glow, src, dest, org, 0, tint);
        }
    }
    EndBlendMode();

    // Pass 3: solid core
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

inline void render_tex(World &world, const mailbox::DrawBuffer &dbuf,
                       const RenderConfig &rcfg) {
    ClearBackground(Color{0, 0, 0, 255});

    auto view = dbuf.begin_read();
    const int G = world.get_groups_size();

    Texture2D glow = get_glow_tex();
    const float coreSize = rcfg.core_size;
    const float outerScale = coreSize * rcfg.outer_scale_mul;
    const float innerScale = coreSize * rcfg.inner_scale_mul;

    const bool doInterp = rcfg.interpolate && view.t0 > 0 && view.t1 > 0 &&
                          view.t1 > view.t0 && view.prev && view.curr &&
                          view.prev->size() == view.curr->size() &&
                          !view.curr->empty();

    if (doInterp) {
        const auto &pos0 = *view.prev;
        const auto &pos1 = *view.curr;

        const long long now_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count();
        const long long target_ns =
            now_ns - (long long)(rcfg.interp_delay_ms * 1'000'000.0f);

        float a = (target_ns <= view.t0) ? 0.0f
                  : (target_ns >= view.t1)
                      ? 1.0f
                      : float(target_ns - view.t0) / float(view.t1 - view.t0);

        auto posAt = [&](int i) -> Vector2 {
            size_t b = (size_t)i * 2;
            if (b + 1 >= pos1.size())
                return {0, 0};
            float x = pos0[b + 0] + (pos1[b + 0] - pos0[b + 0]) * a;
            float y = pos0[b + 1] + (pos1[b + 1] - pos0[b + 1]) * a;
            return {x, y};
        };

        draw_particles_with_glow(world, G, posAt, glow, coreSize, outerScale,
                                 rcfg.outer_rgb_gain, innerScale,
                                 rcfg.inner_rgb_gain);

        dbuf.end_read(view);
        return;
    }

    // No interpolation
    {
        const auto &pos = dbuf.read_current_only();
        auto posAt = [&](int i) -> Vector2 {
            size_t b = (size_t)i * 2;
            if (b + 1 >= pos.size())
                return {0, 0};
            return {pos[b + 0], pos[b + 1]};
        };

        draw_particles_with_glow(world, G, posAt, glow, coreSize, outerScale,
                                 rcfg.outer_rgb_gain, innerScale,
                                 rcfg.inner_rgb_gain);
    }

    dbuf.end_read(view);
}

#endif
