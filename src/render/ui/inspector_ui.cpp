#include "inspector_ui.hpp"

#include <imgui.h>
#include <raylib.h>
#include <rlImGui.h>

static inline Rectangle normalize_rectangle(Rectangle r) {
    if (r.width < 0) {
        r.x += r.width;
        r.width = -r.width;
    }
    if (r.height < 0) {
        r.y += r.height;
        r.height = -r.height;
    }

    return r;
}

static inline Rectangle selection_to_texture_src(const Rectangle &screen_sel,
                                                 const RenderTexture2D &rt) {
    Rectangle r = normalize_rectangle(screen_sel);
    const float sx = (float)rt.texture.width / (float)GetScreenWidth();
    const float sy = (float)rt.texture.height / (float)GetScreenHeight();

    Rectangle src;
    src.x = r.x * sx;
    src.y = r.y * sy;
    src.width = r.width * sx;
    src.height = -(r.height * sy);

    if (src.x < 0) {
        src.width += src.x;
        src.x = 0;
    }
    if (src.y < -(float)rt.texture.height) {
        src.height = -((float)rt.texture.height - src.y);
    }
    if (src.x + src.width > rt.texture.width) {
        src.width = rt.texture.width - src.x;
    }
    if (-src.height > rt.texture.height + src.y) {
        src.height = -(rt.texture.height - src.y);
    }

    return src;
}

static inline Rectangle centered_rectangle(Vector2 c, float w, float h) {
    return Rectangle{c.x - w * 0.5f, c.y - h * 0.5f, w, h};
}

static inline Vector2 calculate_camera_offset(const Context &ctx) {
    mailbox::SimulationConfigSnapshot scfg = ctx.sim.get_config();
    const float rt_w = (float)ctx.wcfg.screen_width;
    const float rt_h = (float)ctx.wcfg.screen_height;
    const float ox = std::floor((rt_w - scfg.bounds_width) * 0.5f);
    const float oy = std::floor((rt_h - scfg.bounds_height) * 0.5f);

    const float zoom = ctx.rcfg.camera.zoom();
    const float center_x = scfg.bounds_width * 0.5f;
    const float center_y = scfg.bounds_height * 0.5f;
    const float ox_cam =
        ox + center_x - center_x * zoom - ctx.rcfg.camera.x * zoom;
    const float oy_cam =
        oy + center_y - center_y * zoom - ctx.rcfg.camera.y * zoom;

    return Vector2{ox_cam, oy_cam};
}

static inline Vector2 interpolate_position(const Context &ctx,
                                           int particle_id) {
    const float a = std::clamp(ctx.interp_alpha, 0.0f, 1.0f);
    if (ctx.can_interpolate) {
        const auto &pos0 = *ctx.view.prev;
        const auto &pos1 = *ctx.view.curr;
        size_t b = (size_t)particle_id * 2;

        if (b + 1 >= pos1.size()) {
            return Vector2{0, 0};
        }

        float x = pos0[b] + (pos1[b] - pos0[b]) * a;
        float y = pos0[b + 1] + (pos1[b + 1] - pos0[b + 1]) * a;

        return Vector2{x, y};
    } else {
        const auto &pos = *ctx.view.curr;
        size_t b = (size_t)particle_id * 2;

        if (b + 1 >= pos.size()) {
            return Vector2{0, 0};
        }

        return Vector2{pos[b], pos[b + 1]};
    }
}

static inline Vector2 calculate_velocity(const Context &ctx, int particle_id) {
    const auto &pos0 = *ctx.view.prev;
    const auto &pos1 = *ctx.view.curr;
    size_t b = (size_t)particle_id * 2;

    if (b + 1 >= pos1.size() || b + 1 >= pos0.size()) {
        return Vector2{0, 0};
    }

    return Vector2{pos1[b] - pos0[b], pos1[b + 1] - pos0[b + 1]};
}

static inline Vector2 world_to_screen(const Vector2 &world_pos,
                                      const Context &ctx,
                                      const Vector2 &camera_offset) {
    const float zoom = ctx.rcfg.camera.zoom();

    return Vector2{world_pos.x * zoom + camera_offset.x,
                   world_pos.y * zoom + camera_offset.y};
}

InspectorUI::InspectorUI() {
    m_render_texture = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
}

InspectorUI::~InspectorUI() { UnloadRenderTexture(m_render_texture); }

void InspectorUI::resize() {
    UnloadRenderTexture(m_render_texture);
    m_render_texture = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
}

RenderTexture2D &InspectorUI::texture() { return m_render_texture; }

const RenderTexture2D &InspectorUI::texture() const { return m_render_texture; }

void InspectorUI::render(Context &ctx) {
    follow_tracked(ctx);
    BeginTextureMode(m_render_texture);
    ClearBackground({0, 0, 0, 0});
    draw_selection_overlay();
    EndTextureMode();
}

void InspectorUI::update_selection_from_mouse(Context &ctx) {
    if (!ctx.rcfg.show_ui) {
        return;
    }

    ImGuiIO &io = ImGui::GetIO();
    const bool ui_capturing = io.WantCaptureMouse;

    if (m_selection.track_enabled) {
        return;
    }

    if (!ui_capturing) {
        bool ctrl_cmd = IsKeyDown(KEY_LEFT_CONTROL) ||
                        IsKeyDown(KEY_RIGHT_CONTROL) ||
                        IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
        if (ctrl_cmd && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            m_selection.show_window = false;
            m_selection.dragging = true;
            m_selection.has = true;
            Vector2 mp = GetMousePosition();
            m_selection.rect = {mp.x, mp.y, 0, 0};
        }
    }

    if (m_selection.dragging) {
        Vector2 mp = GetMousePosition();
        m_selection.rect.width = mp.x - m_selection.rect.x;
        m_selection.rect.height = mp.y - m_selection.rect.y;
        Rectangle r = normalize_rectangle(m_selection.rect);
        ImGui::BeginTooltip();
        ImGui::Text("x=%.0f  y=%.0f\nw=%.0f  h=%.0f", r.x, r.y, r.width,
                    r.height);
        ImGui::EndTooltip();
    }

    if (m_selection.dragging && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        m_selection.dragging = false;
        m_selection.show_window = true;
    }
}

void InspectorUI::render_ui(Context &ctx, const RenderTexture2D &color_rt) {
    if (!ctx.rcfg.show_ui) {
        return;
    }
    if (!m_selection.show_window) {
        return;
    }

    Rectangle logical = normalize_rectangle(m_selection.rect);
    if (logical.width <= 0 || logical.height <= 0) {
        return;
    }

    Vector2 camera_offset = calculate_camera_offset(ctx);

    ImGui::Begin("Region Inspector", &m_selection.show_window);
    ImGui::Text("x=%.0f  y=%.0f  w=%.0f  h=%.0f", logical.x, logical.y,
                logical.width, logical.height);

    render_tracking_controls(ctx, logical);
    ImGui::Separator();
    render_particle_statistics(ctx, logical, camera_offset);
    render_region_preview(ctx, logical, camera_offset, color_rt);

    ImGui::End();
}

void InspectorUI::follow_tracked(Context &ctx) {
    if (!m_selection.track_enabled || m_selection.tracked_id < 0) {
        return;
    }

    const int total_particles = ctx.world_snapshot.get_particles_size();
    Vector2 camera_offset = calculate_camera_offset(ctx);

    if (m_selection.tracked_id < total_particles) {
        Vector2 tp = interpolate_position(ctx, m_selection.tracked_id);
        // Convert to screen space by applying camera transform
        Vector2 tps = world_to_screen(tp, ctx, camera_offset);
        Rectangle r =
            centered_rectangle(tps, m_selection.base_w, m_selection.base_h);
        float sw = (float)GetScreenWidth();
        float sh = (float)GetScreenHeight();

        if (r.x < 0) {
            r.x = 0;
        }
        if (r.y < 0) {
            r.y = 0;
        }
        if (r.x + r.width > sw) {
            r.x = sw - r.width;
        }
        if (r.y + r.height > sh) {
            r.y = sh - r.height;
        }

        m_selection.rect = r;
    } else {
        m_selection.tracked_id = -1;
        m_selection.tracked_group = -1;
        m_selection.has_last_vel = false;
        m_selection.last_vel = Vector2{0.f, 0.f};
    }
}

void InspectorUI::draw_selection_overlay() {
    auto &sel = m_selection;
    if (!sel.has || (!sel.dragging && !sel.show_window)) {
        return;
    }

    Rectangle r = normalize_rectangle(sel.rect);
    DrawRectangleLinesEx(r, 1.0f, RED);
    DrawRectangle(r.x, r.y, r.width, r.height, {255, 0, 0, 64});
}

void InspectorUI::render_tracking_controls(Context &ctx,
                                           const Rectangle &logical) {
    const auto &world = ctx.world_snapshot;
    const int G = world.get_groups_size();

    bool track_changed =
        ImGui::Checkbox("Track one particle", &m_selection.track_enabled);

    ImGui::SameLine();

    if (ImGui::Button("Clear##track")) {
        m_selection.tracked_id = -1;
        m_selection.tracked_group = -1;
        m_selection.has_last_vel = false;
        m_selection.last_vel = Vector2{0.f, 0.f};
    }

    if (track_changed) {
        if (m_selection.track_enabled) {
            m_selection.base_w = logical.width;
            m_selection.base_h = logical.height;
        }
    }

    if (m_selection.track_enabled && m_selection.tracked_id >= 0) {
        int gid = (m_selection.tracked_group >= 0)
                      ? m_selection.tracked_group
                      : world.group_of(m_selection.tracked_id);
        if (gid >= 0 && gid < G) {
            Color gc = world.get_group_color(gid);

            ImGui::SameLine();
            ImGui::TextUnformatted("Selected:");
            ImGui::SameLine();
            ImGui::ColorButton(
                "##selgroup",
                ImVec4(gc.r / 255.f, gc.g / 255.f, gc.b / 255.f, 1.0f),
                ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop |
                    ImGuiColorEditFlags_NoAlpha,
                ImVec2(18, 18));
            ImGui::Text("(id %d, group %d)", m_selection.tracked_id, gid);
            ImGui::Text("The following is not exactly per frame\n as the "
                        "tripple buffer is always behind simulation");

            Vector2 pos = interpolate_position(ctx, m_selection.tracked_id);
            ImGui::Text("px %d, py %d", (int)pos.x, (int)pos.y);
            Vector2 v_now = calculate_velocity(ctx, m_selection.tracked_id);
            float vnow_len = std::sqrt(v_now.x * v_now.x + v_now.y * v_now.y);

            if (vnow_len > 1e-6f) {
                m_selection.last_vel = v_now;
                m_selection.has_last_vel = true;
            }

            Vector2 v_disp = (vnow_len > 1e-6f) ? v_now
                                                : (m_selection.has_last_vel
                                                       ? m_selection.last_vel
                                                       : Vector2{0.f, 0.f});
            float speed = std::sqrt(v_disp.x * v_disp.x + v_disp.y * v_disp.y);

            ImGui::Text("v (vx %.2f, vy %.2f, |v| %.2f) px/tick", v_disp.x,
                        v_disp.y, speed);
        } else {
            ImGui::SameLine();
            ImGui::Text("(id %d)", m_selection.tracked_id);
        }
    }
}

void InspectorUI::render_particle_statistics(Context &ctx,
                                             const Rectangle &logical,
                                             const Vector2 &camera_offset) {
    const auto &world = ctx.world_snapshot;
    const int total_particles = world.get_particles_size();
    const int G = world.get_groups_size();

    std::vector<int> per_group(G, 0);
    int in_count = 0;
    for (int i = 0; i < total_particles; ++i) {
        Vector2 p = interpolate_position(ctx, i);
        Vector2 ps = world_to_screen(p, ctx, camera_offset);
        if (ps.x >= logical.x && ps.x < logical.x + logical.width &&
            ps.y >= logical.y && ps.y < logical.y + logical.height) {
            int g = world.group_of(i);
            // Skip disabled groups
            if (g >= 0 && g < G && world.is_group_enabled(g)) {
                ++in_count;
                ++per_group[g];
            }
        }
    }

    ImGui::Text("Particles in region: %d", in_count);
    if (G > 0) {
        ImGui::Spacing();
        ImGui::TextUnformatted("By group:");
        ImGui::Spacing();
        const ImVec2 chip_size(16.f, 16.f);
        for (int g = 0; g < G; ++g) {
            int cnt = per_group[g];
            if (cnt <= 0)
                continue;
            // Skip disabled groups
            if (!world.is_group_enabled(g))
                continue;
            ImGui::PushID(g);
            Color rc = world.get_group_color(g);
            ImGui::ColorButton(
                "##chip",
                ImVec4(rc.r / 255.f, rc.g / 255.f, rc.b / 255.f, 1.0f),
                ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop |
                    ImGuiColorEditFlags_NoAlpha,
                chip_size);
            ImGui::SameLine();
            ImGui::Text("particles: %d", cnt);
            ImGui::PopID();
        }
    }
}

void InspectorUI::render_region_preview(Context &ctx, const Rectangle &logical,
                                        const Vector2 &camera_offset,
                                        const RenderTexture2D &color_rt) {
    const float aspect = logical.height / logical.width;
    const int pw = 320;
    const int ph = (int)(pw * (aspect > 0 ? aspect : 1.0f));
    Rectangle src = selection_to_texture_src(m_selection.rect, color_rt);

    rlImGuiImageRect(&color_rt.texture, pw, ph, src);
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    ImVec2 size = ImVec2(max.x - min.x, max.y - min.y);
    ImGui::SetItemAllowOverlap();
    bool hovered = ImGui::IsItemHovered();
    bool clicked = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

    if (m_selection.track_enabled && clicked) {
        handle_particle_picking(ctx, logical, camera_offset, min, size);
    }
}

void InspectorUI::handle_particle_picking(Context &ctx,
                                          const Rectangle &logical,
                                          const Vector2 &camera_offset,
                                          const ImVec2 &min,
                                          const ImVec2 &size) {
    const auto &world = ctx.world_snapshot;
    const int total_particles = world.get_particles_size();

    ImVec2 m = ImGui::GetMousePos();
    float u = (size.x > 0) ? (m.x - min.x) / size.x : 0.f;
    float v = (size.y > 0) ? (m.y - min.y) / size.y : 0.f;
    float wx = logical.x + u * logical.width;
    float wy = logical.y + v * logical.height;
    const float pick_radius_px = 8.f;
    const float pick_r2 = pick_radius_px * pick_radius_px;
    int best_id = -1;
    float best_d2 = 1e30f;

    for (int i = 0; i < total_particles; ++i) {
        Vector2 p = interpolate_position(ctx, i);
        Vector2 ps = world_to_screen(p, ctx, camera_offset);
        if (ps.x < logical.x || ps.x > logical.x + logical.width ||
            ps.y < logical.y || ps.y > logical.y + logical.height) {
            continue;
        }

        float dx = ps.x - wx;
        float dy = ps.y - wy;
        float d2 = dx * dx + dy * dy;
        if (d2 < best_d2) {
            best_d2 = d2;
            best_id = i;
        }
    }

    if (best_id >= 0 && best_d2 <= pick_r2) {
        m_selection.tracked_id = best_id;
        m_selection.tracked_group = world.group_of(best_id);
        Vector2 seedv = calculate_velocity(ctx, best_id);
        m_selection.last_vel = seedv;
        m_selection.has_last_vel =
            (std::fabs(seedv.x) + std::fabs(seedv.y)) > 0.f;

        if (m_selection.base_w <= 0 || m_selection.base_h <= 0) {
            m_selection.base_w = logical.width;
            m_selection.base_h = logical.height;
        }
    }
}
