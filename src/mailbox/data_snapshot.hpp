#pragma once

#include <atomic>
#include <mutex>
#include <type_traits>
#include <vector>

#include <raylib.h>

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
 * @brief Provides read-only access to interaction rules for a specific
 * source group in WorldSnapshot.
 */
struct WorldSnapshotRuleRowView {
    const float *row; // Pointer to the rule row data
    int size;         // Size of the rule row

    /**
     * @brief Get the interaction rule value for a specific destination
     * group.
     * @param j Destination group index
     * @return Rule value, or 0.0f if out of bounds
     */
    inline float get(int j) const noexcept {
        if (!row || j < 0 || j >= size)
            return 0.f;
        return row[j];
    }
};

/**
 * @brief World snapshot containing all read-only world data
 */
struct WorldSnapshot {
    int group_count;
    int particles_count;              // Total number of particles
    std::vector<int> group_ranges;    // 2*G: [start, end] for each group
    std::vector<Color> group_colors;  // G colors
    std::vector<float> group_radii2;  // G interaction radii squared
    std::vector<bool> group_enabled;  // G enabled states
    std::vector<float> rules;         // G×G interaction rules matrix
    std::vector<int> particle_groups; // N: group index for each particle

    // Safe accessors matching World interface
    int get_groups_size() const noexcept;
    int get_particles_size() const noexcept;
    int get_group_start(int group_index) const noexcept;
    int get_group_end(int group_index) const noexcept;
    int get_group_size(int group_index) const noexcept;
    Color get_group_color(int group_index) const noexcept;
    float r2_of(int group_index) const noexcept;
    bool is_group_enabled(int group_index) const noexcept;
    float rule_val(int source_group, int destination_group) const;
    WorldSnapshotRuleRowView rules_of(int source_group) const noexcept;
    int group_of(int particle_index) const noexcept;
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
