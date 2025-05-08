#include "render_config_ui.hpp"

void RenderConfigUI::render(Context &ctx) {
    if (!ctx.rcfg.show_ui || !ctx.rcfg.show_render_config)
        return;
    render_ui(ctx);
}

void RenderConfigUI::render_ui(Context &ctx) {
    auto &sim = ctx.sim;
    auto &rcfg = ctx.rcfg;
    mailbox::SimulationConfigSnapshot scfg = sim.get_config();
    bool scfg_updated = false;
    auto mark = [&scfg_updated](bool s) {
        if (s)
            scfg_updated = true;
    };

    ImGui::Begin("[3] Render Configuration", &ctx.rcfg.show_render_config);
    ImGui::SetWindowSize(ImVec2{500, 600}, ImGuiCond_FirstUseEver);

    render_interpolation_section(ctx);
    render_background_section(ctx);
    render_border_section(ctx);
    render_particle_rendering_section(ctx);
    render_overlays_section(ctx, mark);

    scfg.draw_report.grid_data = rcfg.show_grid_lines ||
                                 rcfg.show_density_heat ||
                                 rcfg.show_velocity_field;

    ImGui::End();

    if (scfg_updated) {
        sim.update_config(scfg);
    }
}

void RenderConfigUI::render_interpolation_section(Context &ctx) {
    auto &rcfg = ctx.rcfg;

    ImGui::SeparatorText("Interpolation");
    {
        bool before = rcfg.interpolate;
        if (ImGui::Checkbox("Interpolate", &rcfg.interpolate)) {
            push_rcfg(ctx, "render.interpolate", "Interpolate", before,
                      rcfg.interpolate, [&](const bool &v) {
                          rcfg.interpolate = v;
                      });
        }
    }
    if (rcfg.interpolate) {
        float before = rcfg.interp_delay_ms;
        if (ImGui::SliderFloat("Interp delay (ms)", &rcfg.interp_delay_ms, 0.0f,
                               50.0f, "%.1f")) {
            push_rcfg(ctx, "render.interp_delay_ms", "Interp delay (ms)",
                      before, rcfg.interp_delay_ms, [&](const float &v) {
                          rcfg.interp_delay_ms = v;
                      });
        }
    }
}

void RenderConfigUI::render_background_section(Context &ctx) {
    auto &rcfg = ctx.rcfg;

    ImGui::SeparatorText("Background");
    ImVec4 bg_color = ImVec4(
        rcfg.background_color.r / 255.0f, rcfg.background_color.g / 255.0f,
        rcfg.background_color.b / 255.0f, rcfg.background_color.a / 255.0f);
    if (ImGui::ColorEdit4("Background Color", (float *)&bg_color,
                          ImGuiColorEditFlags_NoAlpha)) {
        Color before = rcfg.background_color;
        rcfg.background_color = {(unsigned char)(bg_color.x * 255),
                                 (unsigned char)(bg_color.y * 255),
                                 (unsigned char)(bg_color.z * 255),
                                 (unsigned char)(bg_color.w * 255)};
        Color after = rcfg.background_color;
        push_rcfg(ctx, "render.background_color", "Background Color", before,
                  after, [&](const Color &c) {
                      rcfg.background_color = c;
                  });
    }
}

void RenderConfigUI::render_border_section(Context &ctx) {
    auto &rcfg = ctx.rcfg;

    ImGui::SeparatorText("Border");
    {
        bool before = rcfg.border_enabled;
        if (ImGui::Checkbox("Border enabled", &rcfg.border_enabled)) {
            push_rcfg(ctx, "render.border_enabled", "Border enabled", before,
                      rcfg.border_enabled, [&](const bool &v) {
                          rcfg.border_enabled = v;
                      });
        }
    }
    if (rcfg.border_enabled) {
        ImVec4 border_color =
            ImVec4(rcfg.border_color.r / 255.0f, rcfg.border_color.g / 255.0f,
                   rcfg.border_color.b / 255.0f, rcfg.border_color.a / 255.0f);
        if (ImGui::ColorEdit4("Border Color", (float *)&border_color,
                              ImGuiColorEditFlags_NoAlpha)) {
            Color before = rcfg.border_color;
            rcfg.border_color = {(unsigned char)(border_color.x * 255),
                                 (unsigned char)(border_color.y * 255),
                                 (unsigned char)(border_color.z * 255),
                                 (unsigned char)(border_color.w * 255)};
            Color after = rcfg.border_color;
            push_rcfg(ctx, "render.border_color", "Border Color", before, after,
                      [&](const Color &c) {
                          rcfg.border_color = c;
                      });
        }
        float before = rcfg.border_width;
        if (ImGui::SliderFloat("Border width (px)", &rcfg.border_width, 0.5f,
                               10.0f, "%.1f")) {
            push_rcfg(ctx, "render.border_width", "Border width (px)", before,
                      rcfg.border_width, [&](const float &v) {
                          rcfg.border_width = v;
                      });
        }
    }
}

void RenderConfigUI::render_particle_rendering_section(Context &ctx) {
    auto &rcfg = ctx.rcfg;

    ImGui::SeparatorText("Particle Rendering");
    {
        float before = rcfg.core_size;
        if (ImGui::SliderFloat("Core size (px)", &rcfg.core_size, 0.5f, 4.0f,
                               "%.2f")) {
            push_rcfg(ctx, "render.core_size", "Core size (px)", before,
                      rcfg.core_size, [&](const float &v) {
                          rcfg.core_size = v;
                      });
        }
    }
    {
        bool before = rcfg.glow_enabled;
        if (ImGui::Checkbox("Glow enabled", &rcfg.glow_enabled)) {
            push_rcfg(ctx, "render.glow_enabled", "Glow enabled", before,
                      rcfg.glow_enabled, [&](const bool &v) {
                          rcfg.glow_enabled = v;
                      });
        }
    }
    if (rcfg.glow_enabled) {
        render_glow_settings(ctx);
    }
}

void RenderConfigUI::render_glow_settings(Context &ctx) {
    auto &rcfg = ctx.rcfg;

    float before_outer = rcfg.outer_scale_mul;
    if (ImGui::SliderFloat("Outer scale (x core)", &rcfg.outer_scale_mul, 4.0f,
                           24.0f, "%.1f")) {
        push_rcfg(ctx, "render.outer_scale_mul", "Outer scale (x core)",
                  before_outer, rcfg.outer_scale_mul, [&](const float &v) {
                      rcfg.outer_scale_mul = v;
                  });
    }
    float before_org = rcfg.outer_rgb_gain;
    if (ImGui::SliderFloat("Outer RGB gain", &rcfg.outer_rgb_gain, 0.0f, 1.0f,
                           "%.2f")) {
        push_rcfg(ctx, "render.outer_rgb_gain", "Outer RGB gain", before_org,
                  rcfg.outer_rgb_gain, [&](const float &v) {
                      rcfg.outer_rgb_gain = v;
                  });
    }
    float before_inner = rcfg.inner_scale_mul;
    if (ImGui::SliderFloat("Inner scale (x core)", &rcfg.inner_scale_mul, 1.0f,
                           8.0f, "%.1f")) {
        push_rcfg(ctx, "render.inner_scale_mul", "Inner scale (x core)",
                  before_inner, rcfg.inner_scale_mul, [&](const float &v) {
                      rcfg.inner_scale_mul = v;
                  });
    }
    float before_irg = rcfg.inner_rgb_gain;
    if (ImGui::SliderFloat("Inner RGB gain", &rcfg.inner_rgb_gain, 0.0f, 1.0f,
                           "%.2f")) {
        push_rcfg(ctx, "render.inner_rgb_gain", "Inner RGB gain", before_irg,
                  rcfg.inner_rgb_gain, [&](const float &v) {
                      rcfg.inner_rgb_gain = v;
                  });
    }
    bool before_blit = rcfg.final_additive_blit;
    if (ImGui::Checkbox("Final additive blit", &rcfg.final_additive_blit)) {
        push_rcfg(ctx, "render.final_additive_blit", "Final additive blit",
                  before_blit, rcfg.final_additive_blit, [&](const bool &v) {
                      rcfg.final_additive_blit = v;
                  });
    }
}

void RenderConfigUI::render_overlays_section(Context &ctx,
                                             std::function<void(bool)> mark) {
    auto &rcfg = ctx.rcfg;

    ImGui::SeparatorText("Overlays");
    {
        bool before = rcfg.show_density_heat;
        if (ImGui::Checkbox("Density heatmap", &rcfg.show_density_heat)) {
            push_rcfg(ctx, "render.show_density_heat", "Density heatmap",
                      before, rcfg.show_density_heat, [&](const bool &v) {
                          rcfg.show_density_heat = v;
                      });
            mark(true);
        }
    }
    if (rcfg.show_density_heat) {
        float before = rcfg.heat_alpha;
        if (ImGui::SliderFloat("Heat alpha", &rcfg.heat_alpha, 0.0f, 1.0f,
                               "%.2f")) {
            push_rcfg(ctx, "render.heat_alpha", "Heat alpha", before,
                      rcfg.heat_alpha, [&](const float &v) {
                          rcfg.heat_alpha = v;
                      });
        }
    }
    {
        bool before = rcfg.show_grid_lines;
        if (ImGui::Checkbox("Show grid lines", &rcfg.show_grid_lines)) {
            push_rcfg(ctx, "render.show_grid_lines", "Show grid lines", before,
                      rcfg.show_grid_lines, [&](const bool &v) {
                          rcfg.show_grid_lines = v;
                      });
            mark(true);
        }
    }
    {
        bool before = rcfg.show_velocity_field;
        if (ImGui::Checkbox("Velocity field", &rcfg.show_velocity_field)) {
            push_rcfg(ctx, "render.show_velocity_field", "Velocity field",
                      before, rcfg.show_velocity_field, [&](const bool &v) {
                          rcfg.show_velocity_field = v;
                      });
            mark(true);
        }
    }
    if (rcfg.show_velocity_field) {
        render_velocity_field_settings(ctx);
    }
}

void RenderConfigUI::render_velocity_field_settings(Context &ctx) {
    auto &rcfg = ctx.rcfg;

    float before_vs = rcfg.vel_scale;
    if (ImGui::SliderFloat("Vel scale", &rcfg.vel_scale, 0.1f, 5.0f, "%.2f")) {
        push_rcfg(ctx, "render.vel_scale", "Vel scale", before_vs,
                  rcfg.vel_scale, [&](const float &v) {
                      rcfg.vel_scale = v;
                  });
    }
    float before_vt = rcfg.vel_thickness;
    if (ImGui::SliderFloat("Vel thickness", &rcfg.vel_thickness, 0.5f, 4.0f,
                           "%.1f")) {
        push_rcfg(ctx, "render.vel_thickness", "Vel thickness", before_vt,
                  rcfg.vel_thickness, [&](const float &v) {
                      rcfg.vel_thickness = v;
                  });
    }
}
