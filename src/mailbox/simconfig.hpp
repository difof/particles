#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace mailbox {

/**
 * @brief Thread-safe configuration mailbox for simulation parameters
 *
 * This class provides a lock-free double buffering system for simulation
 * configuration data that allows concurrent reading and writing between UI
 * and simulation threads. The UI thread publishes configuration updates
 * while the simulation thread acquires the latest configuration once per tick.
 */
class SimulationConfig {
  public:
    /**
     * @brief Configuration snapshot containing all simulation parameters
     *
     * This struct holds a complete set of simulation configuration parameters
     * that can be atomically published and acquired between threads.
     */
    struct Snapshot {
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

  public:
    /**
     * @brief Default constructor that initializes all buffer values
     */
    SimulationConfig();
    ~SimulationConfig();
    SimulationConfig(const SimulationConfig &) = delete;
    SimulationConfig(SimulationConfig &&) = delete;
    SimulationConfig &operator=(const SimulationConfig &) = delete;
    SimulationConfig &operator=(SimulationConfig &&) = delete;

    /**
     * @brief Publish a new configuration snapshot
     * @param s The configuration snapshot to publish
     */
    void publish(const Snapshot &s);

    /**
     * @brief Acquire the current configuration snapshot
     * @return The current configuration snapshot
     */
    Snapshot acquire() const;

  private:
    std::mutex m_write_lock;
    std::atomic<int> m_front{0};
    Snapshot m_buffer[2];
};

} // namespace mailbox
