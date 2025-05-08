#include "sim_config_ui.hpp"

#include <cmath>
#include <functional>
#include <memory>
#include <thread>

#include "../../simulation/multicore.hpp"

void SimConfigUI::render(Context &ctx) {
    if (!ctx.rcfg.show_ui || !ctx.rcfg.show_sim_config)
        return;
    render_ui(ctx);
}

void SimConfigUI::render_ui(Context &ctx) {
    auto &sim = ctx.sim;
    mailbox::SimulationConfigSnapshot scfg = sim.get_config();
    bool scfg_updated = false;

    ImGui::Begin("[4] Simulation Configuration", &ctx.rcfg.show_sim_config);
    ImGui::SetWindowSize(ImVec2{450, 500}, ImGuiCond_FirstUseEver);

    render_bounds_section(ctx, scfg, scfg_updated);
    render_simulation_params(ctx, scfg, scfg_updated);
    render_gravity_section(ctx, scfg, scfg_updated);
    render_parallelism_section(ctx, scfg, scfg_updated);

    ImGui::End();

    if (scfg_updated) {
        sim.update_config(scfg);
    }
}

void SimConfigUI::render_bounds_section(Context &ctx,
                                        mailbox::SimulationConfigSnapshot &scfg,
                                        bool &scfg_updated) {
    ImGui::SeparatorText("Simulation Parameters");
    ImGui::SeparatorText("Bounds");

    struct BoundsUIState {
        bool inited = false;
        int bw = 0;
        int bh = 0;
        int applied_w = 0; // last committed to sim config
        int applied_h = 0;
    };
    static BoundsUIState bs;

    if (!bs.inited || ImGui::IsWindowAppearing()) {
        bs.bw = (int)std::lrint(scfg.bounds_width);
        bs.bh = (int)std::lrint(scfg.bounds_height);
        bs.applied_w = bs.bw;
        bs.applied_h = bs.bh;
        bs.inited = true;
    }

    ImGui::SliderInt("Bounds Width", &bs.bw, 64, 5000);
    bool widthActive = ImGui::IsItemActive();
    ImGui::SliderInt("Bounds Height", &bs.bh, 64, 5000);
    bool heightActive = ImGui::IsItemActive();

    // Only reflect external changes (e.g., undo) when the committed
    // config changed since last apply; do NOT clobber local edits.
    if (!widthActive && !heightActive) {
        int cfgW = (int)std::lrint(scfg.bounds_width);
        int cfgH = (int)std::lrint(scfg.bounds_height);
        if (cfgW != bs.applied_w || cfgH != bs.applied_h) {
            bs.bw = cfgW;
            bs.bh = cfgH;
            bs.applied_w = cfgW;
            bs.applied_h = cfgH;
        }
    }

    if (ImGui::Button("Apply Bounds")) {
        auto &sim = ctx.sim;
        auto before_w = (int)std::lrint(scfg.bounds_width);
        auto before_h = (int)std::lrint(scfg.bounds_height);
        ImGuiID id = ImGui::GetItemID();
        ctx.undo.beginInteraction(id);

        ctx.undo.push(std::unique_ptr<IAction>(new ValueAction<int>(
            "sim.bounds_width", "Bounds Width",
            []() {
                return 0;
            },
            [&](const int &v) {
                auto cfg = sim.get_config();
                cfg.bounds_width = (float)v;
                sim.update_config(cfg);
            },
            before_w, bs.bw)));

        ctx.undo.push(std::unique_ptr<IAction>(new ValueAction<int>(
            "sim.bounds_height", "Bounds Height",
            []() {
                return 0;
            },
            [&](const int &v) {
                auto cfg = sim.get_config();
                cfg.bounds_height = (float)v;
                sim.update_config(cfg);
            },
            before_h, bs.bh)));

        ctx.undo.endInteraction(id);
        scfg.bounds_width = (float)bs.bw;
        scfg.bounds_height = (float)bs.bh;
        bs.applied_w = bs.bw;
        bs.applied_h = bs.bh;
        scfg_updated = true;
    }
}

void SimConfigUI::render_simulation_params(
    Context &ctx, mailbox::SimulationConfigSnapshot &scfg, bool &scfg_updated) {
    ImGui::Separator();

    auto &sim = ctx.sim;

    // Target TPS
    int before_tps = scfg.target_tps;
    if (ImGui::SliderInt("Target TPS", &scfg.target_tps, 0, 240, "%d",
                         ImGuiSliderFlags_AlwaysClamp)) {
        push_scfg_action(ctx, "sim.target_tps", "Target TPS", before_tps,
                         scfg.target_tps, [&](const int &v) {
                             auto cfg = sim.get_config();
                             cfg.target_tps = v;
                             sim.update_config(cfg);
                         });
        scfg_updated = true;
    }

    // Time Scale
    float before_time_scale = scfg.time_scale;
    if (ImGui::SliderFloat("Time Scale", &scfg.time_scale, 0.01f, 2.0f, "%.3f",
                           ImGuiSliderFlags_Logarithmic)) {
        push_scfg_action(ctx, "sim.time_scale", "Time Scale", before_time_scale,
                         scfg.time_scale, [&](const float &v) {
                             auto cfg = sim.get_config();
                             cfg.time_scale = v;
                             sim.update_config(cfg);
                         });
        scfg_updated = true;
    }

    // Viscosity
    float before_viscosity = scfg.viscosity;
    if (ImGui::SliderFloat("Viscosity", &scfg.viscosity, 0.0f, 1.0f, "%.3f")) {
        push_scfg_action(ctx, "sim.viscosity", "Viscosity", before_viscosity,
                         scfg.viscosity, [&](const float &v) {
                             auto cfg = sim.get_config();
                             cfg.viscosity = v;
                             sim.update_config(cfg);
                         });
        scfg_updated = true;
    }

    // Wall Repel
    float before_wall_repel = scfg.wall_repel;
    if (ImGui::SliderFloat("Wall Repel (px)", &scfg.wall_repel, 0.0f, 200.0f,
                           "%.1f")) {
        push_scfg_action(ctx, "sim.wall_repel", "Wall Repel (px)",
                         before_wall_repel, scfg.wall_repel,
                         [&](const float &v) {
                             auto cfg = sim.get_config();
                             cfg.wall_repel = v;
                             sim.update_config(cfg);
                         });
        scfg_updated = true;
    }

    // Wall Strength
    float before_wall_strength = scfg.wall_strength;
    if (ImGui::SliderFloat("Wall Strength", &scfg.wall_strength, 0.0f, 1.0f,
                           "%.3f")) {
        push_scfg_action(ctx, "sim.wall_strength", "Wall Strength",
                         before_wall_strength, scfg.wall_strength,
                         [&](const float &v) {
                             auto cfg = sim.get_config();
                             cfg.wall_strength = v;
                             sim.update_config(cfg);
                         });
        scfg_updated = true;
    }
}

void SimConfigUI::render_gravity_section(
    Context &ctx, mailbox::SimulationConfigSnapshot &scfg, bool &scfg_updated) {
    ImGui::SeparatorText("Gravity");

    auto &sim = ctx.sim;

    // Gravity X
    float before_gravity_x = scfg.gravity_x;
    if (ImGui::SliderFloat("Gravity X", &scfg.gravity_x, -1.0f, 1.0f, "%.3f")) {
        push_scfg_action(ctx, "sim.gravity_x", "Gravity X", before_gravity_x,
                         scfg.gravity_x, [&](const float &v) {
                             auto cfg = sim.get_config();
                             cfg.gravity_x = v;
                             sim.update_config(cfg);
                         });
        scfg_updated = true;
    }

    // Gravity Y
    float before_gravity_y = scfg.gravity_y;
    if (ImGui::SliderFloat("Gravity Y", &scfg.gravity_y, -1.0f, 1.0f, "%.3f")) {
        push_scfg_action(ctx, "sim.gravity_y", "Gravity Y", before_gravity_y,
                         scfg.gravity_y, [&](const float &v) {
                             auto cfg = sim.get_config();
                             cfg.gravity_y = v;
                             sim.update_config(cfg);
                         });
        scfg_updated = true;
    }

    // Reset Gravity button
    if (ImGui::Button("Reset Gravity")) {
        float before_x = scfg.gravity_x;
        float before_y = scfg.gravity_y;
        ImGuiID id = ImGui::GetItemID();
        ctx.undo.beginInteraction(id);

        ctx.undo.push(std::unique_ptr<IAction>(new ValueAction<float>(
            "sim.gravity_x", "Gravity X",
            []() {
                return 0.0f;
            },
            [&](const float &v) {
                auto cfg = sim.get_config();
                cfg.gravity_x = v;
                sim.update_config(cfg);
            },
            before_x, 0.0f)));

        ctx.undo.push(std::unique_ptr<IAction>(new ValueAction<float>(
            "sim.gravity_y", "Gravity Y",
            []() {
                return 0.0f;
            },
            [&](const float &v) {
                auto cfg = sim.get_config();
                cfg.gravity_y = v;
                sim.update_config(cfg);
            },
            before_y, 0.0f)));

        ctx.undo.endInteraction(id);
        scfg.gravity_x = 0.0f;
        scfg.gravity_y = 0.0f;
        scfg_updated = true;
    }
}

void SimConfigUI::render_parallelism_section(
    Context &ctx, mailbox::SimulationConfigSnapshot &scfg, bool &scfg_updated) {
    ImGui::SeparatorText("Parallelism");

    auto &sim = ctx.sim;
    unsigned hc = std::thread::hardware_concurrency();
    int max_threads = std::max(1, (int)hc - 2);
    ImGui::Text("HW threads: %u", hc ? hc : 1);

    bool auto_mode = (scfg.sim_threads <= 0);

    // Auto mode checkbox
    bool before_auto = auto_mode;
    if (ImGui::Checkbox("Auto (HW-2)", &auto_mode)) {
        int new_threads = auto_mode ? -1 : std::min(1, max_threads);
        push_scfg_action(ctx, "sim.sim_threads_auto", "Sim threads auto",
                         before_auto, auto_mode, [&](const bool &v) {
                             auto cfg = sim.get_config();
                             cfg.sim_threads =
                                 v ? -1 : std::min(1, max_threads);
                             sim.update_config(cfg);
                         });
        scfg.sim_threads = new_threads;
        scfg_updated = true;
    }

    // Manual thread count slider
    if (!auto_mode) {
        int before_threads = scfg.sim_threads;
        if (ImGui::SliderInt("Sim threads", &scfg.sim_threads, 1, max_threads,
                             "%d", ImGuiSliderFlags_AlwaysClamp)) {
            push_scfg_action(ctx, "sim.sim_threads", "Sim threads",
                             before_threads, scfg.sim_threads,
                             [&](const int &v) {
                                 auto cfg = sim.get_config();
                                 cfg.sim_threads = v;
                                 sim.update_config(cfg);
                             });
            scfg_updated = true;
        }
    } else {
        ImGui::BeginDisabled();
        int auto_val = std::max(1, (int)compute_sim_threads());
        ImGui::SliderInt("Sim threads", &auto_val, 1, max_threads, "%d");
        ImGui::EndDisabled();
    }
}
