#include <stdio.h>
#include <raylib.h>
#include <imgui.h>
#include <rlImGui.h>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <cstddef>
#include <cmath>
#include <random>
#include <algorithm>
#include <thread>
#include <mutex>
#include <atomic>

struct WindowConfig
{
    int screen_width, screen_height, panel_width, render_width;
};

struct SimTimer
{
    double sim_accum;
    double last_time;
    double tps_timer;
    int sim_steps_this_second;
};

struct SimConfig
{
    float bounds_width, bounds_height;
    float bounds_force, bounds_radius;
    int target_tps, effective_tps;
    bool interpolate;
    SimTimer timer;
};

void render_ui(const WindowConfig &wcfg, SimConfig &scfg)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.);
    ImGui::Begin("main", NULL, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);
    ImGui::SetWindowPos(ImVec2{0., 0.}, ImGuiCond_Always);
    ImGui::SetWindowSize(ImVec2{static_cast<float>(wcfg.panel_width), static_cast<float>(wcfg.screen_height)}, ImGuiCond_Always);
    {
        ImGui::SeparatorText("Stats");
        ImGui::Text("FPS: %d", GetFPS());
        ImGui::SameLine();
        ImGui::Text("TPS: %d", scfg.effective_tps);
        ImGui::Text("Sim Bounds: %.0f x %.0f", scfg.bounds_width, scfg.bounds_height);

        ImGui::SeparatorText("Sim Config");
        ImGui::SliderInt("Target TPS", &scfg.target_tps, 0, 60, "%d", ImGuiSliderFlags_AlwaysClamp);
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

std::mutex sim_mutex;
std::atomic<bool> sim_running{true};

void simulate(SimConfig &scfg)
{
    // Dummy heavy compute: sum sqrt of 10000000 numbers
    volatile double sum = 0.0;
    for (int i = 1; i <= 10000000; ++i)
    {
        sum += std::sqrt(static_cast<double>(i));
    }
    (void)sum;
}

void simulation_thread_func(SimConfig &scfg)
{
    while (sim_running.load())
    {
        double now = GetTime();
        double frame_time = now - scfg.timer.last_time;
        scfg.timer.last_time = now;
        scfg.timer.sim_accum += frame_time;
        scfg.timer.tps_timer += frame_time;

        double sim_dt = (scfg.target_tps > 0) ? (1.0 / scfg.target_tps) : 0.0;
        int max_steps = 5;
        int steps = 0;
        if (scfg.target_tps > 0)
        {
            while (scfg.timer.sim_accum >= sim_dt && steps < max_steps)
            {
                simulate(scfg);
                scfg.timer.sim_steps_this_second++;
                scfg.timer.sim_accum -= sim_dt;
                steps++;
            }
            if (steps == max_steps)
                scfg.timer.sim_accum = 0.0;
        }

        while (scfg.timer.tps_timer >= 1.0)
        {
            std::lock_guard<std::mutex> lock(sim_mutex);
            scfg.effective_tps = scfg.timer.sim_steps_this_second;
            scfg.timer.sim_steps_this_second = 0;
            scfg.timer.tps_timer -= 1.0;
        }

        // sleep a bit to avoid busy-waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void render_tex()
{
    ClearBackground(BLACK);
}

void run()
{
    int screenW = 1280;
    int screenH = 800;
    int panelW = screenW * .23;
    int texW = screenW - panelW;
    WindowConfig wcfg = {screenW, screenH, panelW, texW};

    SimConfig scfg = {};
    SimTimer st = {};
    {
        scfg.bounds_width = static_cast<float>(wcfg.render_width);
        scfg.bounds_height = static_cast<float>(wcfg.screen_height);
        scfg.bounds_force = 1.;
        scfg.bounds_radius = 40.;
        scfg.target_tps = 20;
        scfg.interpolate = false;

        st.last_time = GetTime();
        scfg.timer = st;
    }

    InitWindow(wcfg.screen_width, wcfg.screen_height, "Particles");
    SetTargetFPS(60);
    rlImGuiSetup(true);

    RenderTexture2D tex = LoadRenderTexture(wcfg.render_width, wcfg.screen_height);

    sim_running = true;
    std::thread sim_thread(simulation_thread_func, std::ref(scfg));

    while (!WindowShouldClose())
    {
        // Only lock when reading effective_tps for UI
        int effective_tps_copy;
        {
            std::lock_guard<std::mutex> lock(sim_mutex);
            effective_tps_copy = scfg.effective_tps;
        }
        scfg.effective_tps = effective_tps_copy;

        BeginTextureMode(tex);
        render_tex();
        EndTextureMode();

        BeginDrawing();
        ClearBackground(BLACK);

        // render texture must be y-flipped due to default OpenGL coordinates (left-bottom)
        DrawTextureRec(tex.texture, (Rectangle){0, 0, (float)tex.texture.width, (float)-tex.texture.height}, (Vector2){wcfg.panel_width, 0}, WHITE);

        rlImGuiBegin();
        render_ui(wcfg, scfg);
        rlImGuiEnd();

        EndDrawing();
    }

    // Signal simulation thread to stop and join
    sim_running = false;
    sim_thread.join();

    rlImGuiShutdown();
    UnloadRenderTexture(tex);
    CloseWindow();
}

int main()
{
    run();
    return 0;
}