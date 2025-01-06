#ifndef __RENDERER_HPP
#define __RENDERER_HPP

#include "types.hpp"
#include "world.hpp"

void render_tex(World &world, const DrawBuffers &dbuf, const SimConfig &scfg)
{
    ClearBackground(BLACK);

    const bool doInterp = scfg.interpolate.load(std::memory_order_relaxed);

    // Take a stable read of 'front'
    int f1 = dbuf.front.load(std::memory_order_acquire);
    int p0 = 1 - f1; // previous
    int p1 = f1;     // current

    // Grab timestamps
    long long t0 = dbuf.stamp_ns[p0].load(std::memory_order_relaxed);
    long long t1 = dbuf.stamp_ns[p1].load(std::memory_order_relaxed);

    // references to buffers
    const std::vector<float> &pos0 = dbuf.pos[p0];
    const std::vector<float> &pos1 = dbuf.pos[p1];

    const int G = world.get_groups_size();

    // Stable check: if front flipped mid-read, just skip interpolation this frame
    if (doInterp)
    {
        int f2 = dbuf.front.load(std::memory_order_relaxed);
        if (f2 != f1)
        {
            // fall through and draw current only
            goto DRAW_CURRENT_ONLY;
        }
    }

    if (doInterp && t0 > 0 && t1 > 0 && t1 > t0 &&
        pos0.size() == pos1.size() && !pos1.empty())
    {
        // compute alpha using a small "render delay" so target time lies in [t0, t1]
        const float delay_ms = scfg.interp_delay_ms.load(std::memory_order_relaxed);
        const long long now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                     std::chrono::steady_clock::now().time_since_epoch())
                                     .count();
        const long long target_ns = now_ns - (long long)(delay_ms * 1'000'000.0f);

        // Clamp target into [t0, t1] to avoid extrapolation
        float alpha = 0.0f;
        if (target_ns <= t0)
        {
            alpha = 0.0f; // draw previous
        }
        else if (target_ns >= t1)
        {
            alpha = 1.0f; // draw current
        }
        else
        {
            alpha = float(target_ns - t0) / float(t1 - t0);
        }

        // draw each group with its color, interpolating positions
        for (int g = 0; g < G; ++g)
        {
            const int start = world.get_group_start(g);
            const int end = world.get_group_end(g);
            const Color col = *world.get_group_color(g);

            for (int i = start; i < end; ++i)
            {
                const size_t base = (size_t)i * 2;
                if (base + 1 >= pos1.size())
                    break;

                const float x0 = pos0[base + 0];
                const float y0 = pos0[base + 1];
                const float x1 = pos1[base + 0];
                const float y1 = pos1[base + 1];

                const float x = x0 + (x1 - x0) * alpha;
                const float y = y0 + (y1 - y0) * alpha;

                DrawCircleV(Vector2{x, y}, 1.5f, col);
            }
        }
        return;
    }

DRAW_CURRENT_ONLY:
{
    // No interpolation path (unchanged behavior)
    const std::vector<float> &pos = dbuf.pos[dbuf.front.load(std::memory_order_acquire)];
    for (int g = 0; g < G; ++g)
    {
        const int start = world.get_group_start(g);
        const int end = world.get_group_end(g);
        const Color col = *world.get_group_color(g);

        for (int i = start; i < end; ++i)
        {
            const size_t base = (size_t)i * 2;
            if (base + 1 >= pos.size())
                break;
            const float x = pos[base + 0];
            const float y = pos[base + 1];
            DrawCircleV(Vector2{x, y}, 1.5f, col);
        }
    }
}
}

#endif