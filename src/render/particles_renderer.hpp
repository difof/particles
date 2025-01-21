#ifndef __PARTICLES_RENDERER_HPP
#define __PARTICLES_RENDERER_HPP

#include <raylib.h>
#include <raymath.h>

#include "../types.hpp"
#include "renderconfig.hpp"
#include "renderer.hpp"

class ParticlesRenderer : public IRenderer {
  public:
    ParticlesRenderer(const WindowConfig &wcfg)
        : m_rt(LoadRenderTexture(wcfg.render_width, wcfg.screen_height)) {}

    ~ParticlesRenderer() override { UnloadRenderTexture(m_rt); }

    RenderTexture2D &texture() { return m_rt; }
    const RenderTexture2D &texture() const { return m_rt; }

    void render(RenderContext &ctx) override {
        auto &sim = ctx.sim;
        auto &rcfg = ctx.rcfg;
        auto &view = ctx.view;

        BeginTextureMode(m_rt);
        ClearBackground(Color{0, 0, 0, 255});

        const int group_size = sim.get_world().get_groups_size();
        const float core_size = rcfg.core_size;

        Texture2D glow{};
        float outer_scale = 0.f, inner_scale = 0.f;
        if (rcfg.glow_enabled) {
            glow = get_glow_tex();
            outer_scale = core_size * rcfg.outer_scale_mul;
            inner_scale = core_size * rcfg.inner_scale_mul;
        }

        if (ctx.can_interpolate) {
            const auto &pos0 = *view.prev;
            const auto &pos1 = *view.curr;
            const float a = std::clamp(ctx.interp_alpha, 0.0f, 1.0f);
            auto posAt = [&](int i) -> Vector2 {
                size_t b = (size_t)i * 2;
                if (b + 1 >= pos1.size())
                    return {0, 0};
                float x = pos0[b + 0] + (pos1[b + 0] - pos0[b + 0]) * a;
                float y = pos0[b + 1] + (pos1[b + 1] - pos0[b + 1]) * a;
                return {x, y};
            };
            if (rcfg.glow_enabled) {
                draw_particles_with_glow(sim.get_world(), group_size, posAt,
                                         glow, core_size, outer_scale,
                                         rcfg.outer_rgb_gain, inner_scale,
                                         rcfg.inner_rgb_gain);
            } else {
                draw_particles_simple(sim.get_world(), group_size, posAt,
                                      core_size);
            }
        } else {
            const auto &pos = *view.curr;
            auto posAt = [&](int i) -> Vector2 {
                size_t b = (size_t)i * 2;
                if (b + 1 >= pos.size())
                    return {0, 0};
                return {pos[b + 0], pos[b + 1]};
            };
            if (rcfg.glow_enabled) {
                draw_particles_with_glow(sim.get_world(), group_size, posAt,
                                         glow, core_size, outer_scale,
                                         rcfg.outer_rgb_gain, inner_scale,
                                         rcfg.inner_rgb_gain);
            } else {
                draw_particles_simple(sim.get_world(), group_size, posAt,
                                      core_size);
            }
        }

        auto grid = view.grid;
        if (grid) {
            if (rcfg.show_density_heat) {
                draw_density_heat(*grid, rcfg.heat_alpha);
            }
            if (rcfg.show_velocity_field) {
                Color velCol = ColorWithA(WHITE, 200);
                draw_velocity_field(*grid, rcfg.vel_scale, rcfg.vel_thickness,
                                    velCol);
            }
            if (rcfg.show_grid_lines) {
                Color gc = ColorWithA(WHITE, 40);
                for (int cx = 0; cx <= grid->cols; ++cx) {
                    float x = std::min(cx * grid->cell, grid->width);
                    DrawLineEx({x, 0}, {x, grid->height}, 1.0f, gc);
                }
                for (int cy = 0; cy <= grid->rows; ++cy) {
                    float y = std::min(cy * grid->cell, grid->height);
                    DrawLineEx({0, y}, {grid->width, y}, 1.0f, gc);
                }
            }
        }

        EndTextureMode();
    }

  private:
    RenderTexture2D m_rt{};
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
                a = a * a;
                unsigned char A = (unsigned char)lrintf(a * 255.0f);
                ImageDrawPixel(&img, x, y, (Color){255, 255, 255, A});
            }
        tex = LoadTextureFromImage(img);
        UnloadImage(img);
        SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
        inited = true;
        return tex;
    }

    static inline Color ColorWithA(Color c, unsigned char a) {
        c.a = a;
        return c;
    }

    static inline void cell_rect(const mailbox::DrawBuffer::GridFrame &g,
                                 int cx, int cy, float &x, float &y, float &w,
                                 float &h) {
        x = cx * g.cell;
        y = cy * g.cell;
        float maxW = g.width;
        float maxH = g.height;
        w = g.cell;
        h = g.cell;
        if (cx == g.cols - 1)
            w = std::max(0.f, maxW - x);
        if (cy == g.rows - 1)
            h = std::max(0.f, maxH - y);
    }

    static void draw_density_heat(const mailbox::DrawBuffer::GridFrame &g,
                                  float alpha) {
        if (g.cols <= 0 || g.rows <= 0)
            return;
        const int C = g.cols * g.rows;
        int maxCount = 1;
        for (int i = 0; i < C; ++i)
            maxCount = std::max(maxCount, g.count[i]);
        if (maxCount <= 0)
            return;
        const unsigned char A =
            (unsigned char)std::lrint(255.f * std::clamp(alpha, 0.f, 1.f));
        for (int cy = 0; cy < g.rows; ++cy) {
            for (int cx = 0; cx < g.cols; ++cx) {
                int idx = cy * g.cols + cx;
                float t = (float)g.count[idx] / (float)maxCount;
                float hue = 270.0f - 210.0f * t;
                Color c = ColorFromHSV(hue, 0.85f, 1.0f);
                c.a = A;
                float x, y, w, h;
                cell_rect(g, cx, cy, x, y, w, h);
                DrawRectangle((int)x, (int)y, (int)std::ceil(w),
                              (int)std::ceil(h), c);
            }
        }
    }

    static void draw_velocity_field(const mailbox::DrawBuffer::GridFrame &g,
                                    float scale, float thickness, Color col) {
        if (g.cols <= 0 || g.rows <= 0)
            return;
        for (int cy = 0; cy < g.rows; ++cy) {
            for (int cx = 0; cx < g.cols; ++cx) {
                int idx = cy * g.cols + cx;
                int cnt = g.count[idx];
                if (cnt <= 0)
                    continue;
                float vx = g.sumVx[idx] / (float)cnt;
                float vy = g.sumVy[idx] / (float)cnt;
                float x, y, w, h;
                cell_rect(g, cx, cy, x, y, w, h);
                float x0 = x + w * 0.5f;
                float y0 = y + h * 0.5f;
                float x1 = x0 + vx * scale;
                float y1 = y0 + vy * scale;
                DrawLineEx({x0, y0}, {x1, y1}, thickness, col);
                Vector2 dir = Vector2Normalize({vx, vy});
                Vector2 ort = {-dir.y, dir.x};
                float ah = 4.0f + 0.5f * thickness;
                Vector2 p1 = {x1, y1};
                Vector2 p2 = {x1 - dir.x * ah + ort.x * ah * 0.5f,
                              y1 - dir.y * ah + ort.y * ah * 0.5f};
                Vector2 p3 = {x1 - dir.x * ah - ort.x * ah * 0.5f,
                              y1 - dir.y * ah - ort.y * ah * 0.5f};
                DrawTriangle(p1, p2, p3, col);
            }
        }
    }

    template <typename PosFn>
    static void draw_particles_with_glow(const World &world, int groupsCount,
                                         PosFn posAt, Texture2D glow,
                                         float coreSize, float outerScale,
                                         float outerRGBGain, float innerScale,
                                         float innerRGBGain) {
        const Rectangle src = {0, 0, (float)glow.width, (float)glow.height};
        const Vector2 org = {0, 0};
        BeginBlendMode(BLEND_ALPHA);
        for (int g = 0; g < groupsCount; ++g) {
            const int start = world.get_group_start(g);
            const int end = world.get_group_end(g);
            const Color tint = TintRGB(world.get_group_color(g), outerRGBGain);
            for (int i = start; i < end; ++i) {
                Vector2 p = posAt(i);
                Rectangle dest = {p.x - outerScale, p.y - outerScale,
                                  outerScale * 2, outerScale * 2};
                DrawTexturePro(glow, src, dest, org, 0, tint);
            }
        }
        EndBlendMode();
        BeginBlendMode(BLEND_ALPHA);
        for (int g = 0; g < groupsCount; ++g) {
            const int start = world.get_group_start(g);
            const int end = world.get_group_end(g);
            const Color tint = TintRGB(world.get_group_color(g), innerRGBGain);
            for (int i = start; i < end; ++i) {
                Vector2 p = posAt(i);
                Rectangle dest = {p.x - innerScale, p.y - innerScale,
                                  innerScale * 2, innerScale * 2};
                DrawTexturePro(glow, src, dest, org, 0, tint);
            }
        }
        EndBlendMode();
        for (int g = 0; g < groupsCount; ++g) {
            const int start = world.get_group_start(g);
            const int end = world.get_group_end(g);
            const Color col = world.get_group_color(g);
            for (int i = start; i < end; ++i) {
                Vector2 p = posAt(i);
                DrawCircleV(p, coreSize, col);
            }
        }
    }

    template <typename PosFn>
    static void draw_particles_simple(const World &world, int groupsCount,
                                      PosFn posAt, float coreSize) {
        for (int g = 0; g < groupsCount; ++g) {
            const int start = world.get_group_start(g);
            const int end = world.get_group_end(g);
            const Color col = world.get_group_color(g);
            for (int i = start; i < end; ++i) {
                Vector2 p = posAt(i);
                DrawCircleV(p, coreSize, col);
            }
        }
    }
};

#endif
