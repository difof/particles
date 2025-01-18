#ifndef __INTERACTION_RENDERER_HPP
#define __INTERACTION_RENDERER_HPP

#include <imgui.h>
#include <raylib.h>
#include <rlImGui.h>

#include "renderer.hpp"

class InteractionRenderer : public IRenderer {
  public:
    InteractionRenderer() {
        m_rt = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    }

    ~InteractionRenderer() override { UnloadRenderTexture(m_rt); }

    RenderTexture2D &texture() { return m_rt; }

    void render(RenderContext &ctx) override {
        (void)ctx;
        BeginTextureMode(m_rt);
        ClearBackground({0, 0, 0, 0});
        draw_selection_overlay_internal();
        EndTextureMode();
    }

    static inline void update_selection_from_mouse() {
        rt_update_selection_from_mouse();
    }

    static inline void draw_inspector(RenderContext &ctx,
                                      const RenderTexture2D &colorRt) {
        rt_draw_region_inspector(colorRt, ctx.sim.get_world(), ctx.view,
                                 ctx.can_interpolate, ctx.interp_alpha);
    }

  private:
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

    static inline RegionSel &selection_state() {
        static RegionSel s;
        return s;
    }

    void draw_selection_overlay_internal() {
        auto &sel = selection_state();
        if (!sel.has || (!sel.dragging && !sel.show_window))
            return;
        Rectangle r = norm(sel.rect);
        DrawRectangleLinesEx(r, 1.0f, RED);
        DrawRectangle(r.x, r.y, r.width, r.height, {255, 0, 0, 64});
    }

    static void rt_update_selection_from_mouse() {
        auto &sel = selection_state();
        ImGuiIO &io = ImGui::GetIO();
        const bool uiCapturing = io.WantCaptureMouse;
        if (sel.track_enabled)
            return;
        if (!uiCapturing && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            sel.show_window = false;
            sel.dragging = true;
            sel.has = true;
            Vector2 mp = GetMousePosition();
            sel.rect = {mp.x, mp.y, 0, 0};
        }
        if (sel.dragging) {
            Vector2 mp = GetMousePosition();
            sel.rect.width = mp.x - sel.rect.x;
            sel.rect.height = mp.y - sel.rect.y;
            Rectangle r = norm(sel.rect);
            ImGui::BeginTooltip();
            ImGui::Text("x=%.0f  y=%.0f\nw=%.0f  h=%.0f", r.x, r.y, r.width,
                        r.height);
            ImGui::EndTooltip();
        }
        if (sel.dragging && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            sel.dragging = false;
            sel.show_window = true;
        }
    }

    static inline Rectangle norm(Rectangle r) {
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

    static inline Rectangle SelectionToTextureSrc(const Rectangle &screenSel,
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

    static void
    rt_draw_region_inspector(const RenderTexture2D &rt, const World &world,
                             const mailbox::DrawBuffer::ReadView &view,
                             bool doInterp, float interp_alpha) {
        if (ImGui::Begin("Dbg DPI")) {
            ImGui::Text("Screen %d x %d", GetScreenWidth(), GetScreenHeight());
            ImGui::Text("Render %d x %d", GetRenderWidth(), GetRenderHeight());
            ImGui::Text("Tex    %d x %d", rt.texture.width, rt.texture.height);
            ImGui::Text("Mouse  %.1f, %.1f", GetMousePosition().x,
                        GetMousePosition().y);
        }
        ImGui::End();
        auto &sel = selection_state();
        if (!sel.show_window)
            return;
        Rectangle logical = norm(sel.rect);
        if (logical.width <= 0 || logical.height <= 0)
            return;

        const float a = std::clamp(interp_alpha, 0.0f, 1.0f);
        auto posAt = [&](int i) -> Vector2 {
            if (doInterp) {
                const auto &pos0 = *view.prev;
                const auto &pos1 = *view.curr;
                size_t b = (size_t)i * 2;
                if (b + 1 >= pos1.size())
                    return Vector2{0, 0};
                float x = pos0[b] + (pos1[b] - pos0[b]) * a;
                float y = pos0[b + 1] + (pos1[b + 1] - pos0[b + 1]) * a;
                return Vector2{x, y};
            } else {
                const auto &pos = *view.curr;
                size_t b = (size_t)i * 2;
                if (b + 1 >= pos.size())
                    return Vector2{0, 0};
                return Vector2{pos[b], pos[b + 1]};
            }
        };
        auto velAt = [&](int i) -> Vector2 {
            const auto &pos0 = *view.prev;
            const auto &pos1 = *view.curr;
            size_t b = (size_t)i * 2;
            if (b + 1 >= pos1.size() || b + 1 >= pos0.size())
                return Vector2{0, 0};
            return Vector2{pos1[b] - pos0[b], pos1[b + 1] - pos0[b + 1]};
        };

        const int totalParticles = world.get_particles_count();
        const int G = world.get_groups_size();

        ImGui::Begin("Region Inspector", &sel.show_window);
        ImGui::Text("x=%.0f  y=%.0f  w=%.0f  h=%.0f", logical.x, logical.y,
                    logical.width, logical.height);

        bool trackChanged =
            ImGui::Checkbox("Track one particle", &sel.track_enabled);
        ImGui::SameLine();
        if (ImGui::Button("Clear##track")) {
            sel.tracked_id = -1;
            sel.tracked_group = -1;
            sel.has_last_vel = false;
            sel.last_vel = Vector2{0.f, 0.f};
        }
        if (trackChanged) {
            if (sel.track_enabled) {
                sel.base_w = logical.width;
                sel.base_h = logical.height;
            }
        }

        if (sel.track_enabled && sel.tracked_id >= 0) {
            int gid = (sel.tracked_group >= 0) ? sel.tracked_group
                                               : world.group_of(sel.tracked_id);
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
                ImGui::Text("(id %d, group %d)", sel.tracked_id, gid);
                ImGui::Text("The following is not exactly per frame\n as the "
                            "tripple buffer is always behind simulation");
                ImGui::Text("px %d, py %d", (int)posAt(sel.tracked_id).x,
                            (int)posAt(sel.tracked_id).y);
                Vector2 v_now = velAt(sel.tracked_id);
                float vnow_len =
                    std::sqrt(v_now.x * v_now.x + v_now.y * v_now.y);
                if (vnow_len > 1e-6f) {
                    sel.last_vel = v_now;
                    sel.has_last_vel = true;
                }
                Vector2 v_disp =
                    (vnow_len > 1e-6f)
                        ? v_now
                        : (sel.has_last_vel ? sel.last_vel : Vector2{0.f, 0.f});
                float speed =
                    std::sqrt(v_disp.x * v_disp.x + v_disp.y * v_disp.y);
                ImGui::Text("v (vx %.2f, vy %.2f, |v| %.2f) px/tick", v_disp.x,
                            v_disp.y, speed);
            } else {
                ImGui::SameLine();
                ImGui::Text("(id %d)", sel.tracked_id);
            }
        }

        ImGui::Separator();
        std::vector<int> perGroup(G, 0);
        int inCount = 0;
        for (int i = 0; i < totalParticles; ++i) {
            Vector2 p = posAt(i);
            if (p.x >= logical.x && p.x < logical.x + logical.width &&
                p.y >= logical.y && p.y < logical.y + logical.height) {
                ++inCount;
                int g = world.group_of(i);
                if (g >= 0 && g < G)
                    ++perGroup[g];
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
        Rectangle src = SelectionToTextureSrc(sel.rect, rt);
        rlImGuiImageRect(&rt.texture, pw, ph, src);
        ImVec2 min = ImGui::GetItemRectMin();
        ImVec2 max = ImGui::GetItemRectMax();
        ImVec2 size = ImVec2(max.x - min.x, max.y - min.y);
        ImGui::SetItemAllowOverlap();
        bool hovered = ImGui::IsItemHovered();
        bool clicked = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        if (sel.track_enabled && clicked) {
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
                if (p.x < logical.x || p.x > logical.x + logical.width ||
                    p.y < logical.y || p.y > logical.y + logical.height)
                    continue;
                float dx = p.x - wx;
                float dy = p.y - wy;
                float d2 = dx * dx + dy * dy;
                if (d2 < bestD2) {
                    bestD2 = d2;
                    bestId = i;
                }
            }
            if (bestId >= 0 && bestD2 <= pickR2) {
                sel.tracked_id = bestId;
                sel.tracked_group = world.group_of(bestId);
                Vector2 seedv = velAt(bestId);
                sel.last_vel = seedv;
                sel.has_last_vel =
                    (std::fabs(seedv.x) + std::fabs(seedv.y)) > 0.f;
                if (sel.base_w <= 0 || sel.base_h <= 0) {
                    sel.base_w = logical.width;
                    sel.base_h = logical.height;
                }
            }
        }

        if (sel.track_enabled && sel.tracked_id >= 0) {
            if (sel.tracked_id < totalParticles) {
                Vector2 tp = posAt(sel.tracked_id);
                Rectangle r = CenteredRect(tp, sel.base_w, sel.base_h);
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
                sel.rect = r;
                logical = norm(sel.rect);
            } else {
                sel.tracked_id = -1;
                sel.tracked_group = -1;
                sel.has_last_vel = false;
                sel.last_vel = Vector2{0.f, 0.f};
            }
        }

        ImGui::End();
    }

    static inline Rectangle CenteredRect(Vector2 c, float w, float h) {
        return Rectangle{c.x - w * 0.5f, c.y - h * 0.5f, w, h};
    }

  private:
    RenderTexture2D m_rt{};
};

#endif
