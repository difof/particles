#ifndef __TYPES_HPP
#define __TYPES_HPP

#include <vector>
#include <mutex>
#include <atomic>

struct WindowConfig
{
    int screen_width, screen_height, panel_width, render_width;
};

struct DrawBuffers
{
    std::vector<float> pos[2];          // [i*2+0]=px, [i*2+1]=py
    std::atomic<int> front{0};          // index of readable buffer
    std::atomic<long long> stamp_ns[2]; // monotonic time for each buffer

    DrawBuffers()
    {
        stamp_ns[0].store(0, std::memory_order_relaxed);
        stamp_ns[1].store(0, std::memory_order_relaxed);
    }
};

struct SimConfig
{
    float bounds_width, bounds_height;
    std::atomic<float> time_scale{1.0f};
    std::atomic<float> viscosity{0.0f};
    std::atomic<float> gravity{0.0f};
    std::atomic<float> wallRepel{0.0f};
    std::atomic<float> wallStrength{0.1f};
    std::atomic<float> pulse{0.0f};
    std::atomic<float> pulse_x{0.0f};
    std::atomic<float> pulse_y{0.0f};

    std::atomic<bool> sim_running{true};
    std::atomic<int> target_tps{0};
    std::atomic<int> effective_tps{0};

    // interpolation controls
    std::atomic<bool> interpolate{false};
    std::atomic<float> interp_delay_ms{16.0f}; // render one small step behind

    std::atomic<bool> reset_requested{false};
    std::atomic<int> sim_threads{-1}; // -1 = auto (HW-2)
};

#endif