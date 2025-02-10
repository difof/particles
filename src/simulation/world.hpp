#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include <raylib.h>

#include "../utility/exceptions.hpp"
#include "../utility/logger.hpp"

/**
 * @brief Manages particle groups, their properties, and interaction rules in
 * the simulation.
 *
 * The World class handles the organization of particles into groups, manages
 * group properties like colors and interaction rules, and provides access to
 * particle data. Each group has its own interaction radius and rules for
 * interacting with other groups.
 */
class World {
  public:
    /**
     * @brief Provides read-only access to interaction rules for a specific
     * source group.
     *
     * This struct provides a safe way to access rule values without direct
     * array access, with bounds checking to prevent out-of-range access.
     */
    struct RuleRowView {
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
     * @brief Gets the interaction rule value between two groups.
     * @param source_group Source group index
     * @param destination_group Destination group index
     * @return Rule value, or 0.0f if indices are invalid
     */
    float rule_val(int source_group, int destination_group) const;

    /**
     * @brief Gets a view of interaction rules for a specific source group.
     * @param source_group Source group index
     * @return RuleRowView providing safe access to rule values
     */
    inline RuleRowView rules_of(int source_group) const noexcept {
        const int G = get_groups_size();
        if ((size_t)source_group >= m_g_radii2.size())
            return {nullptr, G};
        if (m_g_rules.size() < (size_t)G * (size_t)G)
            return {nullptr, G};
        return {&m_g_rules[source_group * G], G};
    }

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
    inline int get_groups_size() const noexcept {
        return (int)m_g_ranges.size() / 2;
    }

    /**
     * @brief Gets the starting particle index for a group.
     * @param group_index Group index
     * @return Starting particle index
     */
    inline int get_group_start(int group_index) const noexcept {
        return m_g_ranges[group_index * 2 + 0];
    }

    /**
     * @brief Gets the ending particle index for a group.
     * @param group_index Group index
     * @return Ending particle index
     */
    inline int get_group_end(int group_index) const noexcept {
        return m_g_ranges[group_index * 2 + 1];
    }

    /**
     * @brief Gets the number of particles in a group.
     * @param group_index Group index
     * @return Number of particles in the group
     */
    inline int get_group_size(int group_index) const noexcept {
        return get_group_end(group_index) - get_group_start(group_index);
    }

    /**
     * @brief Gets the total number of particles in the world.
     * @return Total number of particles
     */
    inline int get_particles_size() const noexcept {
        return (int)m_particles.size() / 4;
    }

    /**
     * @brief Sets the color for a group.
     * @param group_index Group index
     * @param color New color for the group
     */
    inline void set_group_color(int group_index, Color color) {
        if ((size_t)group_index < m_g_colors.size())
            m_g_colors[group_index] = color;
    }

    /**
     * @brief Gets the color of a group.
     * @param group_index Group index
     * @return Color of the group
     */
    inline Color get_group_color(int group_index) const noexcept {
        return m_g_colors[group_index];
    }

    /**
     * @brief Gets the x position of a particle.
     * @param particle_index Particle index
     * @return X position
     */
    inline float get_px(int particle_index) const noexcept {
        return m_particles[particle_index * 4 + 0];
    }

    /**
     * @brief Gets the y position of a particle.
     * @param particle_index Particle index
     * @return Y position
     */
    inline float get_py(int particle_index) const noexcept {
        return m_particles[particle_index * 4 + 1];
    }

    /**
     * @brief Gets the x velocity of a particle.
     * @param particle_index Particle index
     * @return X velocity
     */
    inline float get_vx(int particle_index) const noexcept {
        return m_particles[particle_index * 4 + 2];
    }

    /**
     * @brief Gets the y velocity of a particle.
     * @param particle_index Particle index
     * @return Y velocity
     */
    inline float get_vy(int particle_index) const noexcept {
        return m_particles[particle_index * 4 + 3];
    }

    /**
     * @brief Sets the x position of a particle.
     * @param particle_index Particle index
     * @param value New x position
     */
    inline void set_px(int particle_index, float value) noexcept {
        m_particles[particle_index * 4 + 0] = value;
    }

    /**
     * @brief Sets the y position of a particle.
     * @param particle_index Particle index
     * @param value New y position
     */
    inline void set_py(int particle_index, float value) noexcept {
        m_particles[particle_index * 4 + 1] = value;
    }

    /**
     * @brief Sets the x velocity of a particle.
     * @param particle_index Particle index
     * @param value New x velocity
     */
    inline void set_vx(int particle_index, float value) noexcept {
        m_particles[particle_index * 4 + 2] = value;
    }

    /**
     * @brief Sets the y velocity of a particle.
     * @param particle_index Particle index
     * @param value New y velocity
     */
    inline void set_vy(int particle_index, float value) noexcept {
        m_particles[particle_index * 4 + 3] = value;
    }

    /**
     * @brief Sets the interaction rule between two groups.
     * @param source_group Source group index
     * @param destination_group Destination group index
     * @param rule_value Rule value
     */
    inline void set_rule(int source_group, int destination_group,
                         float rule_value) {
        m_g_rules[source_group * get_groups_size() + destination_group] =
            rule_value;
    }

    /**
     * @brief Sets the interaction radius squared for a group.
     * @param group_index Group index
     * @param radius_squared Interaction radius squared
     */
    inline void set_r2(int group_index, float radius_squared) {
        m_g_radii2[group_index] = radius_squared;
    }

    /**
     * @brief Gets the group index for a particle.
     * @param particle_index Particle index
     * @return Group index
     */
    inline int group_of(int particle_index) const noexcept {
        return m_p_group[particle_index];
    }

    /**
     * @brief Gets the interaction radius squared for a group.
     * @param group_index Group index
     * @return Interaction radius squared, or 0.0f if invalid
     */
    inline float r2_of(int group_index) const noexcept {
        if ((size_t)group_index >= m_g_radii2.size())
            return 0.f;
        return m_g_radii2[group_index];
    }

    /**
     * @brief Checks if a group is enabled.
     * @param group_index Group index
     * @return True if group is enabled, false otherwise
     */
    inline bool is_group_enabled(int group_index) const noexcept {
        if ((size_t)group_index >= m_g_enabled.size())
            return true; // default to enabled
        return m_g_enabled[group_index];
    }

    /**
     * @brief Sets the enabled state for a group.
     * @param group_index Group index
     * @param enabled Whether the group should be enabled
     */
    inline void set_group_enabled(int group_index, bool enabled) {
        if ((size_t)group_index < m_g_enabled.size())
            m_g_enabled[group_index] = enabled;
    }

  private:
    std::vector<float> m_particles; // Particle data: each particle has 4
                                    // floats (px, py, vx, vy)
    std::vector<int> m_p_group;     // Group index for each particle (size N)
    std::vector<int>
        m_g_ranges; // Group ranges: each group has 2 items (start, end)
    std::vector<float>
        m_g_rules; // Interaction rules matrix (size G*G: rules[src*G + dst])
    std::vector<float>
        m_g_radii2; // Interaction radius squared for each group (size G)
    std::vector<Color> m_g_colors; // Color for each group
    std::vector<bool>
        m_g_enabled; // Enable/disable state for each group (size G)
};
