#pragma once

#include <atomic>
#include <mutex>
#include <type_traits>
#include <vector>

#include <raylib.h>

#include "../world_base.hpp"

namespace mailbox {

/**
 * @brief Configuration snapshot containing all simulation parameters
 */
struct SimulationConfigSnapshot {
    float bounds_width;
    float bounds_height;
    float time_scale;
    float viscosity;
    float wall_repel;
    float wall_strength;
    float gravity_x;
    float gravity_y;
    int target_tps;
    int sim_threads;

    /**
     * @brief Drawing and visualization report settings
     */
    struct DrawReport {
        bool grid_data;
    } draw_report;
};

/**
 * @brief Statistics snapshot containing all simulation performance data
 */
struct SimulationStatsSnapshot {
    int effective_tps;      // Effective ticks per second (averaged once per
                            // second)
    int particles;          // Current number of particles in simulation
    int groups;             // Current number of particle groups
    int sim_threads;        // Current worker thread count
    long long last_step_ns; // Duration of last simulate_once call in
                            // nanoseconds
    long long published_ns; // Timestamp when this snapshot was published
    long long num_steps;    // Total number of simulation steps completed
};

/**
 * @brief World snapshot containing all read-only world data
 */
struct WorldSnapshot : public particles::WorldBase {
  public:
    int group_count;
    int particles_count; // Total number of particles

    /**
     * @brief Gets the total number of groups
     * @return Number of groups
     */
    inline int get_groups_size() const noexcept override { return group_count; }

    /**
     * @brief Gets the total number of particles
     * @return Total number of particles
     */
    inline int get_particles_size() const noexcept override {
        return particles_count;
    }
};

/**
 * @brief Concept to constrain DataSnapshot to only accept valid snapshot types
 */
template <typename T>
concept ValidSnapshotType = std::is_same_v<T, SimulationConfigSnapshot> ||
                            std::is_same_v<T, SimulationStatsSnapshot> ||
                            std::is_same_v<T, WorldSnapshot>;

/**
 * @brief Thread-safe double buffering template for snapshot data
 *
 * This template class provides a lock-free double buffering system for any
 * snapshot data type that allows concurrent reading and writing between
 * threads. One thread can publish new snapshots while another thread acquires
 * the latest snapshot without blocking.
 *
 * @tparam T The snapshot data type to buffer
 */
template <typename T>
    requires ValidSnapshotType<T>
class DataSnapshot {
  public:
    /**
     * @brief Default constructor
     */
    DataSnapshot() = default;
    ~DataSnapshot() = default;
    DataSnapshot(const DataSnapshot &) = delete;
    DataSnapshot(DataSnapshot &&) = delete;
    DataSnapshot &operator=(const DataSnapshot &) = delete;
    DataSnapshot &operator=(DataSnapshot &&) = delete;

    /**
     * @brief Publish a new snapshot
     * @param snapshot The snapshot data to publish
     */
    void publish(const T &snapshot) {
        std::lock_guard<std::mutex> lock(m_write_lock);
        int back = 1 - m_front.load(std::memory_order_relaxed);
        m_buffer[back] = snapshot;
        m_front.store(back, std::memory_order_release);
    }

    /**
     * @brief Acquire the current snapshot
     * @return The current snapshot data
     */
    T acquire() const {
        int f = m_front.load(std::memory_order_acquire);
        return m_buffer[f];
    }

  private:
    std::mutex m_write_lock;
    std::atomic<int> m_front{0};
    T m_buffer[2];
};

} // namespace mailbox
