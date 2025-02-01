#ifndef __MAILBOX_SIMCONFIG_HPP
#define __MAILBOX_SIMCONFIG_HPP

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace mailbox {
// UI publishes, sim acquires once per tick.
class SimulationConfig {
  public:
    struct Snapshot {
        float bounds_width, bounds_height;
        float time_scale;
        float viscosity;
        float wall_repel;
        float wall_strength;
        float gravity_x, gravity_y;
        int target_tps;
        int sim_threads;

        struct DrawReport {
            bool grid_data;
        } draw_report;
    };

  public:
    SimulationConfig() {
        for (auto &b : m_buffer) {
            b.bounds_width = b.bounds_height = 0.f;
            b.time_scale = 1.f;
            b.viscosity = 0.1f;
            b.wall_repel = 0.f;
            b.wall_strength = 0.f;
            b.gravity_x = 0.f;
            b.gravity_y = 0.f;
            b.target_tps = 0;
            b.sim_threads = 1;
            b.draw_report = {false};
        }
    }

    ~SimulationConfig() = default;
    SimulationConfig(const SimulationConfig &) = delete;
    SimulationConfig(SimulationConfig &&) = delete;
    SimulationConfig &operator=(const SimulationConfig &) = delete;
    SimulationConfig &operator=(SimulationConfig &&) = delete;

    void publish(const Snapshot &s) {
        std::lock_guard lock(m_write_lock);
        int back = 1 - m_front.load(std::memory_order_relaxed);
        m_buffer[back] = s;
        m_front.store(back, std::memory_order_release);
    }

    Snapshot acquire() const {
        int f = m_front.load(std::memory_order_acquire);
        return m_buffer[f];
    }

  private:
    std::mutex m_write_lock;
    std::atomic<int> m_front{0};
    Snapshot m_buffer[2];
};
} // namespace mailbox
#endif // MAILBOXES_HPP
