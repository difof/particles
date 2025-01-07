// file: mailboxes.hpp
#ifndef MAILBOXES_HPP
#define MAILBOXES_HPP

#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

//
// UI -> Simulation commands
//
struct SimCommand {
    enum class Kind { ResetWorld, Quit /* add more as needed */ } kind;
    // Optional payload (simple & generic)
    float a = 0.f, b = 0.f, c = 0.f;
};

class CommandQueue {
  public:
    void push(const SimCommand &cmd) {
        std::lock_guard<std::mutex> lk(m_);
        q_.push_back(cmd);
    }

    // Called only by the simulation thread; returns and clears current batch
    std::vector<SimCommand> drain() {
        std::vector<SimCommand> out;
        std::lock_guard<std::mutex> lk(m_);
        out.swap(q_);
        return out;
    }

  private:
    std::mutex m_;
    std::vector<SimCommand> q_;
};

//
// Simulation -> UI stats
//
struct SimStatsSnapshot {
    int effective_tps = 0; // averaged once per second
    int particles = 0;
    int groups = 0;
    int sim_threads = 0;        // current worker count
    long long last_step_ns = 0; // duration of last simulate_once
    long long published_ns = 0; // when this snapshot was published
};

class StatsBuffer {
  public:
    void publish(const SimStatsSnapshot &s) {
        std::lock_guard<std::mutex> lk(w_);
        int back = 1 - front_.load(std::memory_order_relaxed);
        buf_[back] = s;
        front_.store(back, std::memory_order_release);
    }

    SimStatsSnapshot acquire() const {
        int f = front_.load(std::memory_order_acquire);
        return buf_[f];
    }

  private:
    mutable std::mutex w_;
    std::atomic<int> front_{0};
    SimStatsSnapshot buf_[2];
};

struct SimulationConfigSnapshot {
    float bounds_width, bounds_height;
    float time_scale;
    float viscosity;
    float wallRepel;
    float wallStrength;
    int target_tps;
    bool interpolate; // TODO: move to render config
    float interp_delay_ms;
    int sim_threads;
};

// UI publishes, sim acquires once per tick.
class SimulationConfigBuffer {
  public:
    SimulationConfigBuffer() = default;
    ~SimulationConfigBuffer() = default;
    SimulationConfigBuffer(const SimulationConfigBuffer &) = delete;
    SimulationConfigBuffer(SimulationConfigBuffer &&) = delete;
    SimulationConfigBuffer &operator=(const SimulationConfigBuffer &) = delete;
    SimulationConfigBuffer &operator=(SimulationConfigBuffer &&) = delete;

    void publish(const SimulationConfigSnapshot &s) {
        std::lock_guard lock(m_write_lock);
        int back = 1 - m_front.load(std::memory_order_relaxed);
        m_buffer[back] = s;
        m_front.store(back, std::memory_order_release);
    }

    SimulationConfigSnapshot acquire() const {
        int f = m_front.load(std::memory_order_acquire);
        return m_buffer[f];
    }

  private:
    std::mutex m_write_lock;
    std::atomic<int> m_front{0};
    SimulationConfigSnapshot m_buffer[2];
};

#endif // MAILBOXES_HPP
