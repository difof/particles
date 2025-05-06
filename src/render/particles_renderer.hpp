#pragma once

#include <algorithm>
#include <cmath>

#include <raylib.h>
#include <raymath.h>

#include "irenderer.hpp"
#include "types/config.hpp"
#include "types/window.hpp"

/**
 * @brief Concept for position functions used in particle rendering
 * @details A position function must accept an integer index and return a
 * Vector2 representing the particle's position
 */
template <typename F>
concept PosFn = requires(F f, int i) {
    { f(i) } -> std::same_as<Vector2>;
};

/**
 * @brief Renders particle systems with support for glow effects, interpolation,
 * and camera transforms
 *
 * This renderer handles the visualization of particle systems with various
 * rendering modes including simple circles, glow effects, density heat maps,
 * velocity fields, and grid overlays. It supports camera transformations,
 * interpolation between frames, and scissor mode for bounds.
 */
class ParticlesRenderer : public IRenderer {
  public:
    ParticlesRenderer(const WindowConfig &wcfg);
    ~ParticlesRenderer() override;
    ParticlesRenderer(const ParticlesRenderer &) = delete;
    ParticlesRenderer(ParticlesRenderer &&) = delete;
    ParticlesRenderer &operator=(const ParticlesRenderer &) = delete;
    ParticlesRenderer &operator=(ParticlesRenderer &&) = delete;

    /**
     * @brief Resizes the render texture to match new window dimensions
     * @param wcfg New window configuration
     */
    void resize(const WindowConfig &wcfg);

    /**
     * @brief Gets the render texture for external use
     * @return Reference to the render texture
     */
    RenderTexture2D &texture() { return m_rt; }

    /**
     * @brief Gets the render texture for external use (const version)
     * @return Const reference to the render texture
     */
    const RenderTexture2D &texture() const { return m_rt; }

    /**
     * @brief Renders the particle system to the render texture
     * @param ctx Rendering context containing simulation data and configuration
     */
    void render(Context &ctx) override;

  private:
    /**
     * @brief Camera transformation data for rendering
     */
    struct CameraTransform {
        float bounds_w;
        float bounds_h;
        float ox;
        float oy;
        float ox_cam;
        float oy_cam;
        float zoom;
        bool use_scissor;
    };

  private:
    /**
     * @brief Tints a color by multiplying RGB components by a factor
     * @param c Original color
     * @param k Tint factor (1.0 = no change)
     * @return Tinted color with alpha preserved
     */
    static inline Color TintRGB(Color c, float k) {
        auto clamp = [](int v) {
            return v < 0 ? 0 : (v > 255 ? 255 : v);
        };
        return Color{(unsigned char)clamp((int)std::lrint(c.r * k)),
                     (unsigned char)clamp((int)std::lrint(c.g * k)),
                     (unsigned char)clamp((int)std::lrint(c.b * k)), 255};
    }

    /**
     * @brief Creates a color with modified alpha value
     * @param c Original color
     * @param a New alpha value
     * @return Color with modified alpha
     */
    static inline Color ColorWithA(Color c, unsigned char a) {
        c.a = a;
        return c;
    }

    /**
     * @brief Calculates the rectangle bounds for a grid cell
     * @param g Grid frame data
     * @param cx Column index
     * @param cy Row index
     * @param x Output x coordinate
     * @param y Output y coordinate
     * @param w Output width
     * @param h Output height
     */
    static inline void cell_rect(const mailbox::render::GridFrame &g, int cx,
                                 int cy, float &x, float &y, float &w,
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

    /**
     * @brief Sets up camera transformation parameters
     * @param ctx Rendering context
     * @return Camera transformation data
     */
    CameraTransform setup_camera_transform(const Context &ctx) const;

    /**
     * @brief Sets up scissor mode for bounds clipping
     * @param transform Camera transformation data
     */
    void setup_scissor_mode(const CameraTransform &transform) const;

    /**
     * @brief Renders particles with appropriate effects
     * @param ctx Rendering context
     * @param transform Camera transformation data
     */
    void render_particles(const Context &ctx,
                          const CameraTransform &transform) const;

    /**
     * @brief Renders grid overlays (density heat, velocity field, grid lines)
     * @param ctx Rendering context
     * @param transform Camera transformation data
     */
    void render_grid_overlays(const Context &ctx,
                              const CameraTransform &transform) const;

    /**
     * @brief Gets or creates the glow texture for particle effects
     * @return Glow texture with radial gradient
     */
    Texture2D get_glow_tex() const;

    /**
     * @brief Draws density heat map overlay using camera-aware positioning
     * @param g Grid frame containing density data
     * @param alpha Heat map alpha transparency
     * @param ox X camera offset
     * @param oy Y camera offset
     * @param zoom Camera zoom factor
     */
    void draw_density_heat_camera(const mailbox::render::GridFrame &g,
                                  float alpha, float ox, float oy,
                                  float zoom) const;

    /**
     * @brief Draws velocity field arrows using camera-aware positioning
     * @param g Grid frame containing velocity data
     * @param scale Arrow length scale factor
     * @param thickness Arrow line thickness
     * @param col Arrow color
     * @param ox X camera offset
     * @param oy Y camera offset
     * @param zoom Camera zoom factor
     */
    void draw_velocity_field_camera(const mailbox::render::GridFrame &g,
                                    float scale, float thickness, Color col,
                                    float ox, float oy, float zoom) const;

    /**
     * @brief Draws particles with glow effects using camera-aware positioning
     * @tparam PosFn Position function type
     * @param world_snapshot World state snapshot
     * @param groupsCount Number of particle groups
     * @param posAt Position function for particles
     * @param glow Glow texture
     * @param coreSize Core particle size
     * @param outerScale Outer glow scale
     * @param outerRGBGain Outer glow RGB gain
     * @param innerScale Inner glow scale
     * @param innerRGBGain Inner glow RGB gain
     * @param ox X camera offset
     * @param oy Y camera offset
     * @param bw Bounds width
     * @param bh Bounds height
     * @param zoom Camera zoom factor
     */
    template <PosFn PosFn>
    void draw_particles_with_glow_camera(
        const mailbox::WorldSnapshot &world_snapshot, int groupsCount,
        PosFn posAt, Texture2D glow, float coreSize, float outerScale,
        float outerRGBGain, float innerScale, float innerRGBGain, float ox,
        float oy, float bw, float bh, float zoom) const {
        const Rectangle src = {0, 0, (float)glow.width, (float)glow.height};
        const Vector2 org = {0, 0};
        BeginBlendMode(BLEND_ALPHA);
        for (int g = 0; g < groupsCount; ++g) {
            // Skip disabled groups
            if (!world_snapshot.is_group_enabled(g)) {
                continue;
            }
            const int start = world_snapshot.get_group_start(g);
            const int end = world_snapshot.get_group_end(g);
            const Color tint =
                TintRGB(world_snapshot.get_group_color(g), outerRGBGain);
            for (int i = start; i < end; ++i) {
                Vector2 p = posAt(i);
                if (p.x < 0 || p.y < 0 || p.x >= bw - 1 || p.y >= bh - 1)
                    continue;
                Vector2 p_screen = {p.x * zoom + ox, p.y * zoom + oy};
                Rectangle dest = {p_screen.x - outerScale,
                                  p_screen.y - outerScale, outerScale * 2,
                                  outerScale * 2};
                DrawTexturePro(glow, src, dest, org, 0, tint);
            }
        }
        EndBlendMode();
        BeginBlendMode(BLEND_ALPHA);
        for (int g = 0; g < groupsCount; ++g) {
            // Skip disabled groups
            if (!world_snapshot.is_group_enabled(g)) {
                continue;
            }
            const int start = world_snapshot.get_group_start(g);
            const int end = world_snapshot.get_group_end(g);
            const Color tint =
                TintRGB(world_snapshot.get_group_color(g), innerRGBGain);
            for (int i = start; i < end; ++i) {
                Vector2 p = posAt(i);
                if (p.x < 0 || p.y < 0 || p.x >= bw - 1 || p.y >= bh - 1)
                    continue;
                Vector2 p_screen = {p.x * zoom + ox, p.y * zoom + oy};
                Rectangle dest = {p_screen.x - innerScale,
                                  p_screen.y - innerScale, innerScale * 2,
                                  innerScale * 2};
                DrawTexturePro(glow, src, dest, org, 0, tint);
            }
        }
        EndBlendMode();
        for (int g = 0; g < groupsCount; ++g) {
            // Skip disabled groups
            if (!world_snapshot.is_group_enabled(g)) {
                continue;
            }
            const int start = world_snapshot.get_group_start(g);
            const int end = world_snapshot.get_group_end(g);
            const Color col = world_snapshot.get_group_color(g);
            for (int i = start; i < end; ++i) {
                Vector2 p = posAt(i);
                if (p.x < 0 || p.y < 0 || p.x >= bw - 1 || p.y >= bh - 1)
                    continue;
                Vector2 p_screen = {p.x * zoom + ox, p.y * zoom + oy};
                DrawCircleV(p_screen, coreSize, col);
            }
        }
    }

    /**
     * @brief Draws particles as simple circles using camera-aware positioning
     * @tparam PosFn Position function type
     * @param world_snapshot World state snapshot
     * @param groupsCount Number of particle groups
     * @param posAt Position function for particles
     * @param coreSize Core particle size
     * @param ox X camera offset
     * @param oy Y camera offset
     * @param bw Bounds width
     * @param bh Bounds height
     * @param zoom Camera zoom factor
     */
    template <PosFn PosFn>
    void
    draw_particles_simple_camera(const mailbox::WorldSnapshot &world_snapshot,
                                 int groupsCount, PosFn posAt, float coreSize,
                                 float ox, float oy, float bw, float bh,
                                 float zoom) const {
        for (int g = 0; g < groupsCount; ++g) {
            // Skip disabled groups
            if (!world_snapshot.is_group_enabled(g)) {
                continue;
            }
            const int start = world_snapshot.get_group_start(g);
            const int end = world_snapshot.get_group_end(g);
            const Color col = world_snapshot.get_group_color(g);
            for (int i = start; i < end; ++i) {
                Vector2 p = posAt(i);
                if (p.x < 0 || p.y < 0 || p.x >= bw - 1 || p.y >= bh - 1)
                    continue;
                Vector2 p_screen = {p.x * zoom + ox, p.y * zoom + oy};
                DrawCircleV(p_screen, coreSize, col);
            }
        }
    }

  private:
    WindowConfig m_wcfg;
    RenderTexture2D m_rt{};
    mutable Texture2D m_glow_tex{};
    mutable bool m_glow_tex_initialized{false};
};
