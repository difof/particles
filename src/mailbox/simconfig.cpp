#include "simconfig.hpp"

namespace mailbox {

SimulationConfig::SimulationConfig() {
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

SimulationConfig::~SimulationConfig() = default;

void SimulationConfig::publish(const Snapshot &s) {
    std::lock_guard lock(m_write_lock);
    int back = 1 - m_front.load(std::memory_order_relaxed);
    m_buffer[back] = s;
    m_front.store(back, std::memory_order_release);
}

SimulationConfig::Snapshot SimulationConfig::acquire() const {
    int f = m_front.load(std::memory_order_acquire);
    return m_buffer[f];
}

} // namespace mailbox
