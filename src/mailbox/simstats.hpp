#ifndef __MAILBOX_STATS_HPP
#define __MAILBOX_STATS_HPP

#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

namespace mailbox {
class SimulationStats {
  public:
    struct Snapshot {
        int effective_tps = 0; // averaged once per second
        int particles = 0;
        int groups = 0;
        int sim_threads = 0;        // current worker count
        long long last_step_ns = 0; // duration of last simulate_once
        long long published_ns = 0; // when this snapshot was published
        long long num_steps = 0;
    };

  public:
    void publish(const Snapshot &s) {
        std::lock_guard<std::mutex> lk(w_);
        int back = 1 - front_.load(std::memory_order_relaxed);
        buf_[back] = s;
        front_.store(back, std::memory_order_release);
    }

    Snapshot acquire() const {
        int f = front_.load(std::memory_order_acquire);
        return buf_[f];
    }

  private:
    mutable std::mutex w_;
    std::atomic<int> front_{0};
    Snapshot buf_[2];
};
} // namespace mailbox

#endif