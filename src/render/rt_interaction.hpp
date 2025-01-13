// file: rt_interaction.hpp
#ifndef __RT_INTERACTION_HPP
#define __RT_INTERACTION_HPP

#include "../simulation/simulation.hpp"
#include "renderconfig.hpp"
#include <imgui.h>
#include <raylib.h>
#include <rlImGui.h>

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

inline RegionSel &selection_state() {
    static RegionSel s;
    return s;
}

static inline ImVec4 ToImVec4(Color c) {
    return ImVec4(c.r / 255.f, c.g / 255.f, c.b / 255.f, 1.0f);
}

static inline Rectangle CenteredRect(Vector2 c, float w, float h) {
    return Rectangle{c.x - w * 0.5f, c.y - h * 0.5f, w, h};
}

static inline Color Invert(Color c) {
    return Color{(unsigned char)(255 - c.r), (unsigned char)(255 - c.g),
                 (unsigned char)(255 - c.b), 255};
}

static inline Color HighContrastBW(Color c) {
    // Perceived luminance (Rec. 709)
    float L = 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b; // 0..255
    return (L > 140.f) ? Color{0, 0, 0, 255} : Color{255, 255, 255, 255};
}

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

inline void update_selection_from_mouse() {
    auto &sel = selection_state();
    ImGuiIO &io = ImGui::GetIO();
    const bool uiCapturing = io.WantCaptureMouse;

    // NEW: if tracking is on, don't start a new drag selection
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
        sel.show_window = true; // pop the preview window for this selection
    }
}

inline void draw_selection_overlay() {
    auto &sel = selection_state();
    ClearBackground({0, 0, 0, 0});

    // Existing selection drawing:
    if (!sel.has || (!sel.dragging && !sel.show_window))
        return;
    Rectangle r = norm(sel.rect);
    DrawRectangleLinesEx(r, 1.0f, RED);
    DrawRectangle(r.x, r.y, r.width, r.height, {255, 0, 0, 64});
}

inline Rectangle SelectionToTextureSrc(const Rectangle &screenSel,
                                       const RenderTexture2D &rt) {
    Rectangle r = norm(screenSel);

    // Window (logical px) -> texture (pixel) scale
    const float sx = (float)rt.texture.width / (float)GetScreenWidth();
    const float sy = (float)rt.texture.height / (float)GetScreenHeight();

    Rectangle src;
    src.x = r.x * sx;
    src.y = r.y * sy;
    src.width = r.width * sx;
    src.height = -(r.height * sy); // negative height = flip vertically

    // (Optional) clamp if you ever drag outside the window:
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

void DrawRegionInspector(const RenderTexture2D &rt, const World &world,
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

    // position accessor (unchanged)
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

    // --- UI header ----------------------------------------------------------
    ImGui::Begin("Region Inspector", &sel.show_window);
    ImGui::Text("x=%.0f  y=%.0f  w=%.0f  h=%.0f", logical.x, logical.y,
                logical.width, logical.height);

    // NEW: tracking UI
    bool trackChanged =
        ImGui::Checkbox("Track one particle", &sel.track_enabled);
    ImGui::SameLine();
    if (ImGui::Button("Clear##track")) {
        sel.tracked_id = -1;
        sel.tracked_group = -1;
        sel.has_last_vel = false;         // NEW
        sel.last_vel = Vector2{0.f, 0.f}; // NEW
    }

    if (trackChanged) {
        if (sel.track_enabled) {
            // lock base size at enable time
            sel.base_w = logical.width;
            sel.base_h = logical.height;
        } else {
            // turning off: nothing special
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
            ImGui::ColorButton("##selgroup", ToImVec4(gc),
                               ImGuiColorEditFlags_NoTooltip |
                                   ImGuiColorEditFlags_NoDragDrop |
                                   ImGuiColorEditFlags_NoAlpha,
                               ImVec2(18, 18));
            ImGui::Text("(id %d, group %d)", sel.tracked_id, gid);
            ImGui::Text(
                "The following is not exactly per frame\n as the tripple "
                "buffer is always behind simulation");
            ImGui::Text("px %d, py %d", (int)posAt(sel.tracked_id).x,
                        (int)posAt(sel.tracked_id).y);

            Vector2 v_now = velAt(sel.tracked_id);
            float vnow_len = std::sqrt(v_now.x * v_now.x + v_now.y * v_now.y);

            // NEW: update cache only when we detect motion
            if (vnow_len > 1e-6f) {
                sel.last_vel = v_now;
                sel.has_last_vel = true;
            }

            // NEW: display velocity prefers live, falls back to cached
            Vector2 v_disp =
                (vnow_len > 1e-6f)
                    ? v_now
                    : (sel.has_last_vel ? sel.last_vel : Vector2{0.f, 0.f});
            float speed = std::sqrt(v_disp.x * v_disp.x + v_disp.y * v_disp.y);
            ImGui::Text("v (vx %.2f, vy %.2f, |v| %.2f) px/tick", v_disp.x,
                        v_disp.y, speed);
        } else {
            ImGui::SameLine();
            ImGui::Text("(id %d)", sel.tracked_id);
        }
    }

    ImGui::Separator();

    // --- Count totals + per-group in current logical rect -------------------
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
            ImGui::ColorButton("##chip", ToImVec4(rc),
                               ImGuiColorEditFlags_NoTooltip |
                                   ImGuiColorEditFlags_NoDragDrop |
                                   ImGuiColorEditFlags_NoAlpha,
                               chipSize);
            ImGui::SameLine();
            ImGui::Text("particles: %d", cnt);
            ImGui::PopID();
        }
    }

    // --- Preview crop --------------------------------------------------------
    const float aspect = logical.height / logical.width;
    const int pw = 320;
    const int ph = (int)(pw * (aspect > 0 ? aspect : 1.0f));
    Rectangle src = SelectionToTextureSrc(sel.rect, rt);

    // Draw the texture
    // NOTE: rlImGuiImageRect pushes an item; we can get its rect to capture
    // clicks.
    rlImGuiImageRect(&rt.texture, pw, ph, src);

    // Get item rect to map mouse to preview UV
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    ImVec2 size = ImVec2(max.x - min.x, max.y - min.y);

    // Make it clickable
    ImGui::SetItemAllowOverlap(); // allow overlay drawing after
    bool hovered = ImGui::IsItemHovered();
    bool clicked = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

    // If tracking is enabled and user clicks preview, pick nearest particle
    if (sel.track_enabled && clicked) {
        ImVec2 m = ImGui::GetMousePos();
        float u = (size.x > 0) ? (m.x - min.x) / size.x : 0.f; // 0..1
        float v = (size.y > 0) ? (m.y - min.y) / size.y : 0.f; // 0..1

        // Because src.height is negative (texture flip), the preview is already
        // upright. So mapping is straightforward:
        float wx = logical.x + u * logical.width;
        float wy = logical.y + v * logical.height;

        // Find nearest particle within the displayed region (small radius
        // threshold)
        const float pickRadiusPx =
            8.f; // screen/preview-ish pixels mapped to world scale
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

        // Only accept if something close enough (or just accept best regardless
        // if you prefer)
        if (bestId >= 0 && bestD2 <= pickR2) {
            sel.tracked_id = bestId;

            sel.tracked_id = bestId;
            sel.tracked_group = world.group_of(bestId);

            // seed last velocity from current buffers
            {
                Vector2 seedv = velAt(bestId);
                sel.last_vel = seedv;
                sel.has_last_vel =
                    (std::fabs(seedv.x) + std::fabs(seedv.y)) > 0.f;
            }

            sel.tracked_group = world.group_of(bestId);
            // lock base size if not already
            if (sel.base_w <= 0 || sel.base_h <= 0) {
                sel.base_w = logical.width;
                sel.base_h = logical.height;
            }
        }
    }

    // If tracking is enabled & we have a valid particle, re-center the rect
    if (sel.track_enabled && sel.tracked_id >= 0) {
        if (sel.tracked_id < totalParticles) {
            Vector2 tp = posAt(sel.tracked_id);
            Rectangle r = CenteredRect(tp, sel.base_w, sel.base_h);

            // Clamp to screen (optional, feels nicer)
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

            sel.rect = r; // keep *selection* moving with the particle
            logical = norm(sel.rect);
        } else {
            // particle id invalid now (e.g., despawned) -> drop tracking
            sel.tracked_id = -1;
            sel.tracked_group = -1;
            sel.has_last_vel = false;         // NEW
            sel.last_vel = Vector2{0.f, 0.f}; // NEW
        }
    }

    // --- Draw a marker for the tracked particle INSIDE the preview -----------
    if (sel.track_enabled && sel.tracked_id >= 0 &&
        sel.tracked_id < totalParticles) {
        Vector2 tp = posAt(sel.tracked_id);
        // map world -> preview pixel
        float u =
            (tp.x - logical.x) / (logical.width > 0 ? logical.width : 1.f);
        float v =
            (tp.y - logical.y) / (logical.height > 0 ? logical.height : 1.f);
        float px = min.x + u * size.x;
        float py = min.y + v * size.y;

        // Colored ring by group color
        int gid = sel.tracked_group >= 0 ? sel.tracked_group
                                         : world.group_of(sel.tracked_id);

        Color ring = (gid >= 0 && gid < G) ? world.get_group_color(gid) : RED;
        Color arrowC = Invert(ring);
        ImU32 col = ImGui::GetColorU32(ToImVec4(ring));
        ImU32 colArrow = ImGui::GetColorU32(ToImVec4(HighContrastBW(ring)));
        ImU32 colOutline =
            ImGui::GetColorU32(ToImVec4(Invert(HighContrastBW(ring))));

        // Simple circle outline
        ImDrawList *dl = ImGui::GetWindowDrawList();
        const float R = 8.f;
        dl->AddCircle(ImVec2(px, py), R, col, 0, 2.0f);
        dl->AddCircle(ImVec2(px, py), R + 2.f, IM_COL32(0, 0, 0, 200), 0,
                      1.0f); // outer halo

        // draw velocity arrow in preview
        if (false) {
            Vector2 vv_now = velAt(sel.tracked_id);
            float vvlen_now =
                std::sqrt(vv_now.x * vv_now.x + vv_now.y * vv_now.y);
            if (vvlen_now > 1e-6f) {
                sel.last_vel = vv_now;
                sel.has_last_vel = true;
            }
            Vector2 vv =
                (vvlen_now > 1e-6f)
                    ? vv_now
                    : (sel.has_last_vel ? sel.last_vel : Vector2{0.f, 0.f});

            // scale velocity to a nice on-screen length
            // base on preview size so it’s visible regardless of
            // zoom
            float baseLen = 0.12f * std::min(size.x, size.y); // tweak to taste
            float vlen = std::sqrt(vv.x * vv.x + vv.y * vv.y);
            float scale = (vlen > 1e-6f) ? (baseLen / vlen) : 0.f;

            // endpoint in world
            Vector2 tp2 =
                Vector2{tp.x + vv.x * scale * (logical.width / size.x),
                        tp.y + vv.y * scale * (logical.height / size.y)};

            // map endpoint to preview pixels
            float u2 =
                (tp2.x - logical.x) / (logical.width > 0 ? logical.width : 1.f);
            float v2 = (tp2.y - logical.y) /
                       (logical.height > 0 ? logical.height : 1.f);
            float px2 = min.x + u2 * size.x;
            float py2 = min.y + v2 * size.y;

            // main vector
            dl->AddLine(ImVec2(px, py), ImVec2(px2, py2), colArrow, 2.0f);

            // arrow head
            ImVec2 dir = ImVec2(px2 - px, py2 - py);
            float dlen = std::sqrt(dir.x * dir.x + dir.y * dir.y);
            if (dlen > 1e-3f) {
                ImVec2 ndir = ImVec2(dir.x / dlen, dir.y / dlen);
                ImVec2 left = ImVec2(-ndir.y, ndir.x);
                float head = 8.f; // size of arrow head
                ImVec2 tip = ImVec2(px2, py2);
                ImVec2 a = ImVec2(tip.x - ndir.x * head - left.x * head * 0.6f,
                                  tip.y - ndir.y * head - left.y * head * 0.6f);
                ImVec2 b = ImVec2(tip.x - ndir.x * head + left.x * head * 0.6f,
                                  tip.y - ndir.y * head + left.y * head * 0.6f);
                dl->AddTriangleFilled(tip, a, b, colOutline);
            }
        }
    }

    ImGui::End();
}

#endif // __RT_INTERACTION_HPP
