#ifndef __INSPECTOR_UI_HPP
#define __INSPECTOR_UI_HPP

#include <imgui.h>
#include <raylib.h>
#include <rlImGui.h>

#include "../irenderer.hpp"

// Internal helpers local to this translation unit.
namespace {
inline Rectangle norm(Rectangle r) {
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

inline Rectangle SelectionToTextureSrc(const Rectangle &screenSel,
                                       const RenderTexture2D &rt) {
    Rectangle r = norm(screenSel);
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
    if (src.x + src.width > rt.texture.width)
        src.width = rt.texture.width - src.x;
    if (-src.height > rt.texture.height + src.y)
        src.height = -(rt.texture.height - src.y);
    return src;
}

inline Rectangle CenteredRect(Vector2 c, float w, float h) {
    return Rectangle{c.x - w * 0.5f, c.y - h * 0.5f, w, h};
}
} // namespace

class InspectorUI : public IRenderer {
  public:
    InspectorUI() {
        m_rt = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    }
    ~InspectorUI() override { UnloadRenderTexture(m_rt); }

    void resize() {
        UnloadRenderTexture(m_rt);
        m_rt = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    }

    RenderTexture2D &texture() { return m_rt; }
    const RenderTexture2D &texture() const { return m_rt; }

    void render(Context &ctx) override {
        follow_tracked(ctx);
        BeginTextureMode(m_rt);
        ClearBackground({0, 0, 0, 0});
        draw_selection_overlay_internal();
        EndTextureMode();
    }

    void update_selection_from_mouse(Context &ctx) {
        if (!ctx.rcfg.show_ui)
            return;
        ImGuiIO &io = ImGui::GetIO();
        const bool uiCapturing = io.WantCaptureMouse;
        if (m_sel.track_enabled) {
            return;
        }
        if (!uiCapturing) {
            bool ctrl_cmd =
                IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) ||
                IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
            if (ctrl_cmd && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                m_sel.show_window = false;
                m_sel.dragging = true;
                m_sel.has = true;
                Vector2 mp = GetMousePosition();
                m_sel.rect = {mp.x, mp.y, 0, 0};
            }
        }
        if (m_sel.dragging) {
            Vector2 mp = GetMousePosition();
            m_sel.rect.width = mp.x - m_sel.rect.x;
            m_sel.rect.height = mp.y - m_sel.rect.y;
            Rectangle r = norm(m_sel.rect);
            ImGui::BeginTooltip();
            ImGui::Text("x=%.0f  y=%.0f\nw=%.0f  h=%.0f", r.x, r.y, r.width,
                        r.height);
            ImGui::EndTooltip();
        }
        if (m_sel.dragging && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            m_sel.dragging = false;
            m_sel.show_window = true;
        }
    }

    void render_ui(Context &ctx, const RenderTexture2D &colorRt) {
        if (!ctx.rcfg.show_ui)
            return;
        if (!m_sel.show_window)
            return;

        const auto &world = ctx.world_snapshot;
        mailbox::SimulationConfigSnapshot scfg = ctx.sim.get_config();
        const float rt_w = (float)ctx.wcfg.screen_width;
        const float rt_h = (float)ctx.wcfg.screen_height;
        const float ox = std::floor((rt_w - scfg.bounds_width) * 0.5f);
        const float oy = std::floor((rt_h - scfg.bounds_height) * 0.5f);

        // Apply camera transform with proper center-based zooming
        const float zoom = ctx.rcfg.camera.zoom();
        const float center_x = scfg.bounds_width * 0.5f;
        const float center_y = scfg.bounds_height * 0.5f;
        const float ox_cam =
            ox + center_x - center_x * zoom - ctx.rcfg.camera.x * zoom;
        const float oy_cam =
            oy + center_y - center_y * zoom - ctx.rcfg.camera.y * zoom;
        Rectangle logical = norm(m_sel.rect);
        if (logical.width <= 0 || logical.height <= 0)
            return;

        const float a = std::clamp(ctx.interp_alpha, 0.0f, 1.0f);
        auto posAt = [&](int i) -> Vector2 {
            if (ctx.can_interpolate) {
                const auto &pos0 = *ctx.view.prev;
                const auto &pos1 = *ctx.view.curr;
                size_t b = (size_t)i * 2;
                if (b + 1 >= pos1.size())
                    return Vector2{0, 0};
                float x = pos0[b] + (pos1[b] - pos0[b]) * a;
                float y = pos0[b + 1] + (pos1[b + 1] - pos0[b + 1]) * a;
                return Vector2{x, y};
            } else {
                const auto &pos = *ctx.view.curr;
                size_t b = (size_t)i * 2;
                if (b + 1 >= pos.size())
                    return Vector2{0, 0};
                return Vector2{pos[b], pos[b + 1]};
            }
        };
        auto velAt = [&](int i) -> Vector2 {
            const auto &pos0 = *ctx.view.prev;
            const auto &pos1 = *ctx.view.curr;
            size_t b = (size_t)i * 2;
            if (b + 1 >= pos1.size() || b + 1 >= pos0.size())
                return Vector2{0, 0};
            return Vector2{pos1[b] - pos0[b], pos1[b + 1] - pos0[b + 1]};
        };

        const int totalParticles = world.get_particles_size();
        const int G = world.get_groups_size();

        ImGui::Begin("Region Inspector", &m_sel.show_window);
        ImGui::Text("x=%.0f  y=%.0f  w=%.0f  h=%.0f", logical.x, logical.y,
                    logical.width, logical.height);

        bool trackChanged =
            ImGui::Checkbox("Track one particle", &m_sel.track_enabled);
        ImGui::SameLine();
        if (ImGui::Button("Clear##track")) {
            m_sel.tracked_id = -1;
            m_sel.tracked_group = -1;
            m_sel.has_last_vel = false;
            m_sel.last_vel = Vector2{0.f, 0.f};
        }
        if (trackChanged) {
            if (m_sel.track_enabled) {
                m_sel.base_w = logical.width;
                m_sel.base_h = logical.height;
            }
        }

        if (m_sel.track_enabled && m_sel.tracked_id >= 0) {
            int gid = (m_sel.tracked_group >= 0)
                          ? m_sel.tracked_group
                          : world.group_of(m_sel.tracked_id);
            if (gid >= 0 && gid < G) {
                Color gc = world.get_group_color(gid);
                ImGui::SameLine();
                ImGui::TextUnformatted("Selected:");
                ImGui::SameLine();
                ImGui::ColorButton(
                    "##selgroup",
                    ImVec4(gc.r / 255.f, gc.g / 255.f, gc.b / 255.f, 1.0f),
                    ImGuiColorEditFlags_NoTooltip |
                        ImGuiColorEditFlags_NoDragDrop |
                        ImGuiColorEditFlags_NoAlpha,
                    ImVec2(18, 18));
                ImGui::Text("(id %d, group %d)", m_sel.tracked_id, gid);
                ImGui::Text("The following is not exactly per frame\n as the "
                            "tripple buffer is always behind simulation");
                ImGui::Text("px %d, py %d", (int)posAt(m_sel.tracked_id).x,
                            (int)posAt(m_sel.tracked_id).y);
                Vector2 v_now = velAt(m_sel.tracked_id);
                float vnow_len =
                    std::sqrt(v_now.x * v_now.x + v_now.y * v_now.y);
                if (vnow_len > 1e-6f) {
                    m_sel.last_vel = v_now;
                    m_sel.has_last_vel = true;
                }
                Vector2 v_disp = (vnow_len > 1e-6f)
                                     ? v_now
                                     : (m_sel.has_last_vel ? m_sel.last_vel
                                                           : Vector2{0.f, 0.f});
                float speed =
                    std::sqrt(v_disp.x * v_disp.x + v_disp.y * v_disp.y);
                ImGui::Text("v (vx %.2f, vy %.2f, |v| %.2f) px/tick", v_disp.x,
                            v_disp.y, speed);
            } else {
                ImGui::SameLine();
                ImGui::Text("(id %d)", m_sel.tracked_id);
            }
        }

        ImGui::Separator();
        std::vector<int> perGroup(G, 0);
        int inCount = 0;
        for (int i = 0; i < totalParticles; ++i) {
            Vector2 p = posAt(i);
            Vector2 ps = {p.x * zoom + ox_cam, p.y * zoom + oy_cam};
            if (ps.x >= logical.x && ps.x < logical.x + logical.width &&
                ps.y >= logical.y && ps.y < logical.y + logical.height) {
                int g = world.group_of(i);
                // Skip disabled groups
                if (g >= 0 && g < G && world.is_group_enabled(g)) {
                    ++inCount;
                    ++perGroup[g];
                }
            }
        }
        ImGui::Text("Particles in region: %d", inCount);
        if (G > 0) {
            ImGui::Spacing();
            ImGui::TextUnformatted("By group:");
            ImGui::Spacing();
            const ImVec2 chipSize(16.f, 16.f);
            for (int g = 0; g < G; ++g) {
                int cnt = perGroup[g];
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
                    ImGuiColorEditFlags_NoTooltip |
                        ImGuiColorEditFlags_NoDragDrop |
                        ImGuiColorEditFlags_NoAlpha,
                    chipSize);
                ImGui::SameLine();
                ImGui::Text("particles: %d", cnt);
                ImGui::PopID();
            }
        }

        const float aspect = logical.height / logical.width;
        const int pw = 320;
        const int ph = (int)(pw * (aspect > 0 ? aspect : 1.0f));
        Rectangle src = SelectionToTextureSrc(m_sel.rect, colorRt);
        rlImGuiImageRect(&colorRt.texture, pw, ph, src);
        ImVec2 min = ImGui::GetItemRectMin();
        ImVec2 max = ImGui::GetItemRectMax();
        ImVec2 size = ImVec2(max.x - min.x, max.y - min.y);
        ImGui::SetItemAllowOverlap();
        bool hovered = ImGui::IsItemHovered();
        bool clicked = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        if (m_sel.track_enabled && clicked) {
            ImVec2 m = ImGui::GetMousePos();
            float u = (size.x > 0) ? (m.x - min.x) / size.x : 0.f;
            float v = (size.y > 0) ? (m.y - min.y) / size.y : 0.f;
            float wx = logical.x + u * logical.width;
            float wy = logical.y + v * logical.height;
            const float pickRadiusPx = 8.f;
            const float pickR2 = pickRadiusPx * pickRadiusPx;
            int bestId = -1;
            float bestD2 = 1e30f;
            for (int i = 0; i < totalParticles; ++i) {
                Vector2 p = posAt(i);
                Vector2 ps = {p.x * zoom + ox_cam, p.y * zoom + oy_cam};
                if (ps.x < logical.x || ps.x > logical.x + logical.width ||
                    ps.y < logical.y || ps.y > logical.y + logical.height)
                    continue;
                float dx = ps.x - wx;
                float dy = ps.y - wy;
                float d2 = dx * dx + dy * dy;
                if (d2 < bestD2) {
                    bestD2 = d2;
                    bestId = i;
                }
            }
            if (bestId >= 0 && bestD2 <= pickR2) {
                m_sel.tracked_id = bestId;
                m_sel.tracked_group = world.group_of(bestId);
                Vector2 seedv = velAt(bestId);
                m_sel.last_vel = seedv;
                m_sel.has_last_vel =
                    (std::fabs(seedv.x) + std::fabs(seedv.y)) > 0.f;
                if (m_sel.base_w <= 0 || m_sel.base_h <= 0) {
                    m_sel.base_w = logical.width;
                    m_sel.base_h = logical.height;
                }
            }
        }

        // tracking follow moved to follow_tracked()

        ImGui::End();
    }

  private:
    void follow_tracked(Context &ctx) {
        if (!m_sel.track_enabled || m_sel.tracked_id < 0)
            return;
        const int totalParticles = ctx.world_snapshot.get_particles_size();
        mailbox::SimulationConfigSnapshot scfg = ctx.sim.get_config();
        const float rt_w = (float)ctx.wcfg.screen_width;
        const float rt_h = (float)ctx.wcfg.screen_height;
        const float ox = std::floor((rt_w - scfg.bounds_width) * 0.5f);
        const float oy = std::floor((rt_h - scfg.bounds_height) * 0.5f);

        // Apply camera transform with proper center-based zooming
        const float zoom = ctx.rcfg.camera.zoom();
        const float center_x = scfg.bounds_width * 0.5f;
        const float center_y = scfg.bounds_height * 0.5f;
        const float ox_cam =
            ox + center_x - center_x * zoom - ctx.rcfg.camera.x * zoom;
        const float oy_cam =
            oy + center_y - center_y * zoom - ctx.rcfg.camera.y * zoom;
        auto posAt = [&](int i) -> Vector2 {
            const float a = std::clamp(ctx.interp_alpha, 0.0f, 1.0f);
            if (ctx.can_interpolate) {
                const auto &pos0 = *ctx.view.prev;
                const auto &pos1 = *ctx.view.curr;
                size_t b = (size_t)i * 2;
                if (b + 1 >= pos1.size())
                    return Vector2{0, 0};
                float x = pos0[b] + (pos1[b] - pos0[b]) * a;
                float y = pos0[b + 1] + (pos1[b + 1] - pos0[b + 1]) * a;
                return Vector2{x, y};
            } else {
                const auto &pos = *ctx.view.curr;
                size_t b = (size_t)i * 2;
                if (b + 1 >= pos.size())
                    return Vector2{0, 0};
                return Vector2{pos[b], pos[b + 1]};
            }
        };
        if (m_sel.tracked_id < totalParticles) {
            Vector2 tp = posAt(m_sel.tracked_id);
            // Convert to screen space by applying camera transform
            Vector2 tps = {tp.x * zoom + ox_cam, tp.y * zoom + oy_cam};
            Rectangle r = CenteredRect(tps, m_sel.base_w, m_sel.base_h);
            float sw = (float)GetScreenWidth();
            float sh = (float)GetScreenHeight();
            if (r.x < 0)
                r.x = 0;
            if (r.y < 0)
                r.y = 0;
            if (r.x + r.width > sw)
                r.x = sw - r.width;
            if (r.y + r.height > sh)
                r.y = sh - r.height;
            m_sel.rect = r;
        } else {
            m_sel.tracked_id = -1;
            m_sel.tracked_group = -1;
            m_sel.has_last_vel = false;
            m_sel.last_vel = Vector2{0.f, 0.f};
        }
    }

    struct RegionSel {
        bool show_window = false;
        bool has = false;
        bool dragging = false;
        Rectangle rect{0, 0, 0, 0};
        bool track_enabled = false;
        int tracked_id = -1;
        int tracked_group = -1;
        float base_w = 0.f, base_h = 0.f;
        bool want_pick_from_preview = false;
        Vector2 last_vel{0.f, 0.f};
        bool has_last_vel = false;
    };

    void draw_selection_overlay_internal() {
        auto &sel = m_sel;
        if (!sel.has || (!sel.dragging && !sel.show_window))
            return;
        Rectangle r = norm(sel.rect);
        DrawRectangleLinesEx(r, 1.0f, RED);
        DrawRectangle(r.x, r.y, r.width, r.height, {255, 0, 0, 64});
    }

  private:
    RenderTexture2D m_rt{};
    RegionSel m_sel;
};

#endif
