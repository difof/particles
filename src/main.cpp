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

void ui()
{
    rlImGuiBegin();

    bool open = true;
    ImGui::ShowDemoWindow(&open);

    open = true;
    if (ImGui::Begin("Test Window", &open))
    {
    }
    ImGui::End();

    rlImGuiEnd();
}

int rand_n(int max)
{
    return rand() % (max + 1);
}

void render(int w, int h, RenderTexture2D texture)
{
    BeginTextureMode(texture);
    ClearBackground(BLACK);

    for (int i = 0; i < 1000; i++)
    {
        int x = rand_n(w);
        int y = rand_n(h);
        DrawCircle(x, y, 10, WHITE);
    }

    EndTextureMode();
}

int main(int argc, char *argv[])
{
    srand(time(0));

    int screenWidth = 1280;
    int screenHeight = 800;

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(screenWidth, screenHeight, "raylib-Extras [ImGui] example - simple ImGui Demo");
    SetTargetFPS(144);
    rlImGuiSetup(true);

    RenderTexture2D target = LoadRenderTexture(screenWidth, screenHeight);

    while (!WindowShouldClose())
    {
        render(screenWidth, screenHeight, target);

        BeginDrawing();
        ClearBackground(BLACK);
        DrawTextureRec(target.texture, (Rectangle){0, 0, (float)target.texture.width, (float)-target.texture.height}, (Vector2){0, 0}, WHITE);
        // ui();
        EndDrawing();
    }

    rlImGuiShutdown();
    CloseWindow();

    return 0;
}
