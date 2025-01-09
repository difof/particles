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
        float wallRepel;
        float wallStrength;
        int target_tps;
        int sim_threads;
    };

  public:
    SimulationConfig() = default;
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
