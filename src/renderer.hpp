#ifndef __RENDERER_HPP
#define __RENDERER_HPP

#include "mailbox/mailbox.hpp"
#include "simulation/simulation.hpp"
#include "simulation/world.hpp"
#include "types.hpp"

const float particle_size = 3.f;

void render_tex(World &world, const mailbox::DrawBuffer &dbuf,
                const mailbox::SimulationConfig &scfgb) {
    ClearBackground(Color{0, 0, 0, 0});

    mailbox::SimulationConfig::Snapshot scfg = scfgb.acquire();

    const bool doInterp = scfg.interpolate;

    mailbox::DrawBuffer::ReadView view = dbuf.begin_read();
    const int G = world.get_groups_size();

    if (doInterp && view.t0 > 0 && view.t1 > 0 && view.t1 > view.t0 &&
        view.prev && view.curr && view.prev->size() == view.curr->size() &&
        !view.curr->empty()) {
        const std::vector<float> &pos0 = *view.prev;
        const std::vector<float> &pos1 = *view.curr;

        const float delay_ms = scfg.interp_delay_ms;
        const long long now_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count();
        const long long target_ns =
            now_ns - (long long)(delay_ms * 1'000'000.0f);

        float alpha = 0.0f;
        if (target_ns <= view.t0)
            alpha = 0.0f;
        else if (target_ns >= view.t1)
            alpha = 1.0f;
        else
            alpha = float(target_ns - view.t0) / float(view.t1 - view.t0);

        for (int g = 0; g < G; ++g) {
            const int start = world.get_group_start(g);
            const int end = world.get_group_end(g);
            const Color col = *world.get_group_color(g);

            for (int i = start; i < end; ++i) {
                const size_t base = (size_t)i * 2;
                if (base + 1 >= pos1.size())
                    break;

                const float x0 = pos0[base + 0], y0 = pos0[base + 1];
                const float x1 = pos1[base + 0], y1 = pos1[base + 1];
                DrawCircleV(
                    Vector2{x0 + (x1 - x0) * alpha, y0 + (y1 - y0) * alpha},
                    particle_size, col);
            }
        }

        dbuf.end_read(view);

        return;
    }

    // No interpolation (or mismatched sizes)
    {
        const std::vector<float> &pos = dbuf.read_current_only();
        for (int g = 0; g < G; ++g) {
            const int start = world.get_group_start(g);
            const int end = world.get_group_end(g);
            const Color col = *world.get_group_color(g);

            for (int i = start; i < end; ++i) {
                const size_t base = (size_t)i * 2;
                if (base + 1 >= pos.size())
                    break;
                DrawCircleV(Vector2{pos[base + 0], pos[base + 1]},
                            particle_size, col);
            }
        }
    }

    dbuf.end_read(view);
}

#endif