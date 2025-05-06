#include "particles_renderer.hpp"

#include <algorithm>
#include <cmath>

#include <raylib.h>
#include <raymath.h>

ParticlesRenderer::ParticlesRenderer(const WindowConfig &wcfg)
    : m_wcfg(wcfg),
      m_rt(LoadRenderTexture(wcfg.screen_width, wcfg.screen_height)) {}

ParticlesRenderer::~ParticlesRenderer() {
    UnloadRenderTexture(m_rt);
    if (m_glow_tex_initialized) {
        UnloadTexture(m_glow_tex);
    }
}

void ParticlesRenderer::resize(const WindowConfig &wcfg) {
    UnloadRenderTexture(m_rt);
    m_wcfg = wcfg;
    m_rt = LoadRenderTexture(wcfg.screen_width, wcfg.screen_height);
}

ParticlesRenderer::CameraTransform
ParticlesRenderer::setup_camera_transform(const Context &ctx) const {
    auto &sim = ctx.sim;
    auto &rcfg = ctx.rcfg;

    // Center sim bounds inside render target without scaling
    mailbox::SimulationConfigSnapshot scfg = sim.get_config();
    const float bounds_width = std::max(0.f, scfg.bounds_width);
    const float bounds_height = std::max(0.f, scfg.bounds_height);
    const float render_target_width = (float)m_rt.texture.width;
    const float render_target_height = (float)m_rt.texture.height;
    const float offset_x =
        std::floor((render_target_width - bounds_width) * 0.5f);
    const float offset_y =
        std::floor((render_target_height - bounds_height) * 0.5f);

    // Apply camera transform with proper center-based zooming
    const float zoom_factor = rcfg.camera.zoom();
    const float center_x = bounds_width * 0.5f;
    const float center_y = bounds_height * 0.5f;
    const float camera_offset_x = offset_x + center_x - center_x * zoom_factor -
                                  rcfg.camera.x * zoom_factor;
    const float camera_offset_y = offset_y + center_y - center_y * zoom_factor -
                                  rcfg.camera.y * zoom_factor;

    return {bounds_width,
            bounds_height,
            offset_x,
            offset_y,
            camera_offset_x,
            camera_offset_y,
            zoom_factor,
            bounds_width >= render_target_width &&
                bounds_height >= render_target_height};
}

void ParticlesRenderer::setup_scissor_mode(
    const CameraTransform &transform) const {
    if (transform.use_scissor) {
        BeginScissorMode((int)transform.ox, (int)transform.oy,
                         (int)std::max(0.f, transform.bounds_w),
                         (int)std::max(0.f, transform.bounds_h));
    }
}

void ParticlesRenderer::render_particles(
    const Context &ctx, const CameraTransform &transform) const {
    auto &rcfg = ctx.rcfg;
    auto &view = ctx.view;
    const int group_size = ctx.world_snapshot.get_groups_size();
    const float core_size = rcfg.core_size;

    Texture2D glow{};
    float outer_glow_scale = 0.f, inner_glow_scale = 0.f;
    if (rcfg.glow_enabled) {
        glow = get_glow_tex();
        outer_glow_scale = core_size * rcfg.outer_scale_mul;
        inner_glow_scale = core_size * rcfg.inner_scale_mul;
    }

    if (ctx.can_interpolate) {
        const auto &pos0 = *view.prev;
        const auto &pos1 = *view.curr;
        const float interpolation_alpha =
            std::clamp(ctx.interp_alpha, 0.0f, 1.0f);
        auto posAt = [&](int particle_index) -> Vector2 {
            size_t position_base_index = (size_t)particle_index * 2;
            if (position_base_index + 1 >= pos1.size()) {
                return {0, 0};
            }
            float interpolated_x = pos0[position_base_index + 0] +
                                   (pos1[position_base_index + 0] -
                                    pos0[position_base_index + 0]) *
                                       interpolation_alpha;
            float interpolated_y = pos0[position_base_index + 1] +
                                   (pos1[position_base_index + 1] -
                                    pos0[position_base_index + 1]) *
                                       interpolation_alpha;
            return {interpolated_x, interpolated_y};
        };
        if (rcfg.glow_enabled) {
            draw_particles_with_glow_camera(
                ctx.world_snapshot, group_size, posAt, glow, core_size,
                outer_glow_scale, rcfg.outer_rgb_gain, inner_glow_scale,
                rcfg.inner_rgb_gain, transform.ox_cam, transform.oy_cam,
                transform.bounds_w, transform.bounds_h, transform.zoom);
        } else {
            draw_particles_simple_camera(ctx.world_snapshot, group_size, posAt,
                                         core_size, transform.ox_cam,
                                         transform.oy_cam, transform.bounds_w,
                                         transform.bounds_h, transform.zoom);
        }
    } else {
        const auto &pos = *view.curr;
        auto posAt = [&](int particle_index) -> Vector2 {
            size_t position_base_index = (size_t)particle_index * 2;
            if (position_base_index + 1 >= pos.size()) {
                return {0, 0};
            }
            return {pos[position_base_index + 0], pos[position_base_index + 1]};
        };
        if (rcfg.glow_enabled) {
            draw_particles_with_glow_camera(
                ctx.world_snapshot, group_size, posAt, glow, core_size,
                outer_glow_scale, rcfg.outer_rgb_gain, inner_glow_scale,
                rcfg.inner_rgb_gain, transform.ox_cam, transform.oy_cam,
                transform.bounds_w, transform.bounds_h, transform.zoom);
        } else {
            draw_particles_simple_camera(ctx.world_snapshot, group_size, posAt,
                                         core_size, transform.ox_cam,
                                         transform.oy_cam, transform.bounds_w,
                                         transform.bounds_h, transform.zoom);
        }
    }
}

void ParticlesRenderer::render_grid_overlays(
    const Context &ctx, const CameraTransform &transform) const {
    auto &rcfg = ctx.rcfg;
    auto &view = ctx.view;
    auto grid = view.grid;

    if (!grid) {
        return;
    }

    if (rcfg.show_density_heat) {
        draw_density_heat_camera(*grid, rcfg.heat_alpha, transform.ox_cam,
                                 transform.oy_cam, transform.zoom);
    }

    if (rcfg.show_velocity_field) {
        Color velocity_color = ColorWithA(WHITE, 200);
        draw_velocity_field_camera(*grid, rcfg.vel_scale, rcfg.vel_thickness,
                                   velocity_color, transform.ox_cam,
                                   transform.oy_cam, transform.zoom);
    }

    if (rcfg.show_grid_lines) {
        Color grid_color = ColorWithA(WHITE, 40);
        for (int column_index = 0; column_index <= grid->cols; ++column_index) {
            float line_x = std::min(column_index * grid->cell, grid->width);
            DrawLineEx(
                {line_x * transform.zoom + transform.ox_cam, transform.oy_cam},
                {line_x * transform.zoom + transform.ox_cam,
                 transform.oy_cam + grid->height * transform.zoom},
                1.0f, grid_color);
        }
        for (int row_index = 0; row_index <= grid->rows; ++row_index) {
            float line_y = std::min(row_index * grid->cell, grid->height);
            DrawLineEx(
                {transform.ox_cam, line_y * transform.zoom + transform.oy_cam},
                {transform.ox_cam + grid->width * transform.zoom,
                 line_y * transform.zoom + transform.oy_cam},
                1.0f, grid_color);
        }
    }
}

void ParticlesRenderer::render(Context &ctx) {
    BeginTextureMode(m_rt);
    ClearBackground(ctx.rcfg.background_color);

    auto camera_transform = setup_camera_transform(ctx);
    setup_scissor_mode(camera_transform);

    render_particles(ctx, camera_transform);
    render_grid_overlays(ctx, camera_transform);

    if (camera_transform.use_scissor) {
        EndScissorMode();
    }
    EndTextureMode();
}

Texture2D ParticlesRenderer::get_glow_tex() const {
    if (m_glow_tex_initialized)
        return m_glow_tex;

    const int texture_size = 64;
    Image glow_image = GenImageColor(texture_size, texture_size, BLANK);
    for (int pixel_y = 0; pixel_y < texture_size; ++pixel_y) {
        for (int pixel_x = 0; pixel_x < texture_size; ++pixel_x) {
            float normalized_x =
                (pixel_x + 0.5f - texture_size * 0.5f) / (texture_size * 0.5f);
            float normalized_y =
                (pixel_y + 0.5f - texture_size * 0.5f) / (texture_size * 0.5f);
            float distance_from_center = sqrtf(normalized_x * normalized_x +
                                               normalized_y * normalized_y);
            float alpha_value = 1.0f - distance_from_center;
            if (alpha_value < 0) {
                alpha_value = 0;
            }
            alpha_value = alpha_value * alpha_value;
            unsigned char final_alpha =
                (unsigned char)lrintf(alpha_value * 255.0f);
            ImageDrawPixel(&glow_image, pixel_x, pixel_y,
                           (Color){255, 255, 255, final_alpha});
        }
    }
    m_glow_tex = LoadTextureFromImage(glow_image);
    UnloadImage(glow_image);
    SetTextureFilter(m_glow_tex, TEXTURE_FILTER_BILINEAR);
    m_glow_tex_initialized = true;

    return m_glow_tex;
}

void ParticlesRenderer::draw_density_heat_camera(
    const mailbox::render::GridFrame &g, float alpha, float ox, float oy,
    float zoom) const {
    if (g.cols <= 0 || g.rows <= 0)
        return;
    const int total_cells = g.cols * g.rows;
    int max_particle_count = 1;
    for (int cell_index = 0; cell_index < total_cells; ++cell_index) {
        max_particle_count = std::max(max_particle_count, g.count[cell_index]);
    }
    if (max_particle_count <= 0) {
        return;
    }
    const unsigned char heat_alpha =
        (unsigned char)std::lrint(255.f * std::clamp(alpha, 0.f, 1.f));
    for (int row_index = 0; row_index < g.rows; ++row_index) {
        for (int column_index = 0; column_index < g.cols; ++column_index) {
            int cell_index = row_index * g.cols + column_index;
            float density_ratio =
                (float)g.count[cell_index] / (float)max_particle_count;
            float hue_value = 270.0f - 210.0f * density_ratio;
            Color heat_color = ColorFromHSV(hue_value, 0.85f, 1.0f);
            heat_color.a = heat_alpha;
            float cell_x, cell_y, cell_width, cell_height;
            cell_rect(g, column_index, row_index, cell_x, cell_y, cell_width,
                      cell_height);
            DrawRectangle((int)(cell_x * zoom + ox), (int)(cell_y * zoom + oy),
                          (int)std::ceil(cell_width * zoom),
                          (int)std::ceil(cell_height * zoom), heat_color);
        }
    }
}

void ParticlesRenderer::draw_velocity_field_camera(
    const mailbox::render::GridFrame &g, float scale, float thickness,
    Color col, float ox, float oy, float zoom) const {
    if (g.cols <= 0 || g.rows <= 0)
        return;
    for (int row_index = 0; row_index < g.rows; ++row_index) {
        for (int column_index = 0; column_index < g.cols; ++column_index) {
            int cell_index = row_index * g.cols + column_index;
            int particle_count = g.count[cell_index];
            if (particle_count <= 0) {
                continue;
            }
            float velocity_x = g.sumVx[cell_index] / (float)particle_count;
            float velocity_y = g.sumVy[cell_index] / (float)particle_count;
            float cell_x, cell_y, cell_width, cell_height;
            cell_rect(g, column_index, row_index, cell_x, cell_y, cell_width,
                      cell_height);
            float arrow_start_x = (cell_x + cell_width * 0.5f) * zoom + ox;
            float arrow_start_y = (cell_y + cell_height * 0.5f) * zoom + oy;
            float arrow_end_x = arrow_start_x + velocity_x * scale * zoom;
            float arrow_end_y = arrow_start_y + velocity_y * scale * zoom;
            DrawLineEx({arrow_start_x, arrow_start_y},
                       {arrow_end_x, arrow_end_y}, thickness, col);
            Vector2 velocity_direction =
                Vector2Normalize({velocity_x, velocity_y});
            Vector2 perpendicular_direction = {-velocity_direction.y,
                                               velocity_direction.x};
            float arrow_head_size = 4.0f + 0.5f * thickness;
            Vector2 arrow_tip = {arrow_end_x, arrow_end_y};
            Vector2 arrow_head_point1 = {
                arrow_end_x - velocity_direction.x * arrow_head_size +
                    perpendicular_direction.x * arrow_head_size * 0.5f,
                arrow_end_y - velocity_direction.y * arrow_head_size +
                    perpendicular_direction.y * arrow_head_size * 0.5f};
            Vector2 arrow_head_point2 = {
                arrow_end_x - velocity_direction.x * arrow_head_size -
                    perpendicular_direction.x * arrow_head_size * 0.5f,
                arrow_end_y - velocity_direction.y * arrow_head_size -
                    perpendicular_direction.y * arrow_head_size * 0.5f};
            DrawTriangle(arrow_tip, arrow_head_point1, arrow_head_point2, col);
        }
    }
}