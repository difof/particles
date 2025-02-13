#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include <raylib.h>

#include "../utility/exceptions.hpp"
#include "../utility/logger.hpp"
#include "../world_base.hpp"

/**
 * @brief Manages particle groups, their properties, and interaction rules in
 * the simulation.
 *
 * The World class handles the organization of particles into groups, manages
 * group properties like colors and interaction rules, and provides access to
 * particle data. Each group has its own interaction radius and rules for
 * interacting with other groups.
 */
class World : public particles::WorldBase {

  public:
    World() = default;
    ~World() = default;
    World(const World &) = delete;
    World(World &&) = delete;
    World &operator=(const World &) = delete;
    World &operator=(World &&) = delete;

    /**
     * @brief Finalizes group assignments for all particles.
     *
     * Updates the particle-to-group mapping after group structure changes.
     */
    void finalize_groups();

    /**
     * @brief Initializes rule tables and group properties for the specified
     * number of groups.
     * @param group_count Number of groups to initialize
     * @throws SimulationError if group_count is negative
     */
    void init_rule_tables(int group_count);

    /**
     * @brief Gets the maximum interaction radius among all groups.
     * @return Maximum interaction radius, or 0.0f if no groups have interaction
     * radius set
     */
    float max_interaction_radius() const;

    /**
     * @brief Adds a new particle group to the world.
     * @param count Number of particles in the new group
     * @param color Color for the new group
     * @return Index of the newly created group
     * @throws SimulationError if count is invalid
     */
    int add_group(int count, Color color);

    /**
     * @brief Resets the world to empty state.
     * @param shrink Whether to shrink vectors to free memory
     */
    void reset(bool shrink = false);

    /**
     * @brief Removes a group and all its particles from the world.
     * @param group_index Index of the group to remove
     */
    void remove_group(int group_index);

    /**
     * @brief Resizes a group by adding or removing particles.
     * @param group_index Index of the group to resize
     * @param new_size New size for the group
     */
    void resize_group(int group_index, int new_size);

    /**
     * @brief Preserves existing rules when a new group is added.
     *
     * This method should be called after adding a group to maintain existing
     * interaction rules in the expanded rule matrix.
     */
    void preserve_rules_on_add_group();

  public:
    /**
     * @brief Gets the total number of groups in the world.
     * @return Number of groups
     */
    inline int get_groups_size() const noexcept override {
        return (int)m_group_ranges.size() / 2;
    }

    /**
     * @brief Gets the total number of particles in the world.
     * @return Total number of particles
     */
    inline int get_particles_size() const noexcept override {
        return (int)m_px.size();
    }

    /**
     * @brief Sets the color for a group.
     * @param group_index Group index
     * @param color New color for the group
     */
    inline void set_group_color(int group_index, Color color) {
        if ((size_t)group_index < m_group_colors.size())
            m_group_colors[group_index] = color;
    }

    /**
     * @brief Gets the x position of a particle.
     * @param particle_index Particle index
     * @return X position
     */
    inline float get_px(int particle_index) const noexcept {
        return m_px[particle_index];
    }

    /**
     * @brief Gets the y position of a particle.
     * @param particle_index Particle index
     * @return Y position
     */
    inline float get_py(int particle_index) const noexcept {
        return m_py[particle_index];
    }

    /**
     * @brief Gets the x velocity of a particle.
     * @param particle_index Particle index
     * @return X velocity
     */
    inline float get_vx(int particle_index) const noexcept {
        return m_vx[particle_index];
    }

    /**
     * @brief Gets the y velocity of a particle.
     * @param particle_index Particle index
     * @return Y velocity
     */
    inline float get_vy(int particle_index) const noexcept {
        return m_vy[particle_index];
    }

    /**
     * @brief Sets the x position of a particle.
     * @param particle_index Particle index
     * @param value New x position
     */
    inline void set_px(int particle_index, float value) noexcept {
        m_px[particle_index] = value;
    }

    /**
     * @brief Sets the y position of a particle.
     * @param particle_index Particle index
     * @param value New y position
     */
    inline void set_py(int particle_index, float value) noexcept {
        m_py[particle_index] = value;
    }

    /**
     * @brief Sets the x velocity of a particle.
     * @param particle_index Particle index
     * @param value New x velocity
     */
    inline void set_vx(int particle_index, float value) noexcept {
        m_vx[particle_index] = value;
    }

    /**
     * @brief Sets the y velocity of a particle.
     * @param particle_index Particle index
     * @param value New y velocity
     */
    inline void set_vy(int particle_index, float value) noexcept {
        m_vy[particle_index] = value;
    }

    /**
     * @brief Gets direct access to X positions array for SoA operations.
     * @return Pointer to X positions array
     */
    inline const float *get_px_array() const noexcept { return m_px.data(); }

    /**
     * @brief Gets direct access to Y positions array for SoA operations.
     * @return Pointer to Y positions array
     */
    inline const float *get_py_array() const noexcept { return m_py.data(); }

    /**
     * @brief Gets direct access to X velocities array for SoA operations.
     * @return Pointer to X velocities array
     */
    inline const float *get_vx_array() const noexcept { return m_vx.data(); }

    /**
     * @brief Gets direct access to Y velocities array for SoA operations.
     * @return Pointer to Y velocities array
     */
    inline const float *get_vy_array() const noexcept { return m_vy.data(); }

    /**
     * @brief Gets direct access to X positions array for SoA operations
     * (mutable).
     * @return Pointer to X positions array
     */
    inline float *get_px_array_mut() noexcept { return m_px.data(); }

    /**
     * @brief Gets direct access to Y positions array for SoA operations
     * (mutable).
     * @return Pointer to Y positions array
     */
    inline float *get_py_array_mut() noexcept { return m_py.data(); }

    /**
     * @brief Gets direct access to X velocities array for SoA operations
     * (mutable).
     * @return Pointer to X velocities array
     */
    inline float *get_vx_array_mut() noexcept { return m_vx.data(); }

    /**
     * @brief Gets direct access to Y velocities array for SoA operations
     * (mutable).
     * @return Pointer to Y velocities array
     */
    inline float *get_vy_array_mut() noexcept { return m_vy.data(); }

    /**
     * @brief Sets the interaction rule between two groups.
     * @param source_group Source group index
     * @param destination_group Destination group index
     * @param rule_value Rule value
     */
    inline void set_rule(int source_group, int destination_group,
                         float rule_value) {
        m_rules[source_group * get_groups_size() + destination_group] =
            rule_value;
    }

    /**
     * @brief Sets the interaction radius squared for a group.
     * @param group_index Group index
     * @param radius_squared Interaction radius squared
     */
    inline void set_r2(int group_index, float radius_squared) {
        m_group_radii2[group_index] = radius_squared;
    }

    /**
     * @brief Sets the enabled state for a group.
     * @param group_index Group index
     * @param enabled Whether the group should be enabled
     */
    inline void set_group_enabled(int group_index, bool enabled) {
        if ((size_t)group_index < m_group_enabled.size())
            m_group_enabled[group_index] = enabled;
    }

  private:
    std::vector<float> m_px; // Particle X positions
    std::vector<float> m_py; // Particle Y positions
    std::vector<float> m_vx; // Particle X velocities
    std::vector<float> m_vy; // Particle Y velocities
};
