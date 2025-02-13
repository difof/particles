#pragma once

#include <vector>

#include <raylib.h>

namespace particles {

/**
 * @brief Provides read-only access to interaction rules for a specific source
 * group
 *
 * This struct provides a safe way to access rule values without direct
 * array access, with bounds checking to prevent out-of-range access.
 */
struct RuleRowView {
    const float *row; // Pointer to the rule row data
    int size;         // Size of the rule row

    /**
     * @brief Get the interaction rule value for a specific destination group
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
 * @brief Base class containing shared group-related data and read-only accessor
 * methods
 *
 * This class provides all shared functionality between World and WorldSnapshot.
 */
class WorldBase {
  public:
    WorldBase() = default;
    virtual ~WorldBase() = default;
    WorldBase(const WorldBase &) = default;
    WorldBase(WorldBase &&) = default;
    WorldBase &operator=(const WorldBase &) = default;
    WorldBase &operator=(WorldBase &&) = default;

    /**
     * @brief Gets the total number of groups (pure virtual)
     * @return Number of groups
     */
    virtual int get_groups_size() const noexcept = 0;

    /**
     * @brief Gets the total number of particles (pure virtual)
     * @return Total number of particles
     */
    virtual int get_particles_size() const noexcept = 0;

    /**
     * @brief Gets the starting particle index for a group
     * @param group_index Group index
     * @return Starting particle index
     */
    inline int get_group_start(int group_index) const noexcept {
        if (group_index < 0 || (size_t)group_index >= m_group_ranges.size() / 2)
            return 0;
        return m_group_ranges[group_index * 2 + 0];
    }

    /**
     * @brief Gets the ending particle index for a group
     * @param group_index Group index
     * @return Ending particle index
     */
    inline int get_group_end(int group_index) const noexcept {
        if (group_index < 0 || (size_t)group_index >= m_group_ranges.size() / 2)
            return 0;
        return m_group_ranges[group_index * 2 + 1];
    }

    /**
     * @brief Gets the number of particles in a group
     * @param group_index Group index
     * @return Number of particles in the group
     */
    inline int get_group_size(int group_index) const noexcept {
        return get_group_end(group_index) - get_group_start(group_index);
    }

    /**
     * @brief Gets the color of a group
     * @param group_index Group index
     * @return Color of the group
     */
    inline Color get_group_color(int group_index) const noexcept {
        if (group_index < 0 || (size_t)group_index >= m_group_colors.size())
            return WHITE;
        return m_group_colors[group_index];
    }

    /**
     * @brief Gets the interaction radius squared for a group
     * @param group_index Group index
     * @return Interaction radius squared, or 0.0f if invalid
     */
    inline float r2_of(int group_index) const noexcept {
        if (group_index < 0 || (size_t)group_index >= m_group_radii2.size())
            return 0.f;
        return m_group_radii2[group_index];
    }

    /**
     * @brief Checks if a group is enabled
     * @param group_index Group index
     * @return True if group is enabled, false otherwise
     */
    inline bool is_group_enabled(int group_index) const noexcept {
        if (group_index < 0 || (size_t)group_index >= m_group_enabled.size())
            return true; // default to enabled
        return m_group_enabled[group_index];
    }

    /**
     * @brief Gets the group index for a particle
     * @param particle_index Particle index
     * @return Group index
     */
    inline int group_of(int particle_index) const noexcept {
        if (particle_index < 0 ||
            (size_t)particle_index >= m_particle_groups.size())
            return 0;
        return m_particle_groups[particle_index];
    }

    /**
     * @brief Gets the interaction rule value between two groups
     * @param source_group Source group index
     * @param destination_group Destination group index
     * @return Rule value, or 0.0f if indices are invalid
     */
    float rule_val(int source_group, int destination_group) const {
        const int group_count = get_groups_size();
        if (source_group < 0 || destination_group < 0 ||
            source_group >= group_count || destination_group >= group_count)
            return 0.f;

        size_t index = (size_t)source_group * (size_t)group_count +
                       (size_t)destination_group;
        if (index >= m_rules.size())
            return 0.f;

        return m_rules[index];
    }

    /**
     * @brief Gets a view of interaction rules for a specific source group
     * @param source_group Source group index
     * @return RuleRowView providing safe access to rule values
     */
    inline RuleRowView rules_of(int source_group) const noexcept {
        const int G = get_groups_size();
        if ((size_t)source_group >= m_group_radii2.size())
            return {nullptr, G};
        if (m_rules.size() < (size_t)G * (size_t)G)
            return {nullptr, G};
        return {&m_rules[source_group * G], G};
    }

    /**
     * @brief Gets the group ranges vector for snapshot creation
     * @return Const reference to group ranges
     */
    inline const std::vector<int> &get_group_ranges() const noexcept {
        return m_group_ranges;
    }

    /**
     * @brief Gets the group colors vector for snapshot creation
     * @return Const reference to group colors
     */
    inline const std::vector<Color> &get_group_colors() const noexcept {
        return m_group_colors;
    }

    /**
     * @brief Gets the group radii squared vector for snapshot creation
     * @return Const reference to group radii squared
     */
    inline const std::vector<float> &get_group_radii2() const noexcept {
        return m_group_radii2;
    }

    /**
     * @brief Gets the group enabled states vector for snapshot creation
     * @return Const reference to group enabled states
     */
    inline const std::vector<bool> &get_group_enabled() const noexcept {
        return m_group_enabled;
    }

    /**
     * @brief Gets the rules matrix for snapshot creation
     * @return Const reference to rules matrix
     */
    inline const std::vector<float> &get_rules() const noexcept {
        return m_rules;
    }

    /**
     * @brief Gets the particle groups vector for snapshot creation
     * @return Const reference to particle groups
     */
    inline const std::vector<int> &get_particle_groups() const noexcept {
        return m_particle_groups;
    }

    /**
     * @brief Sets the group ranges vector for snapshot creation
     * @param ranges The group ranges to set
     */
    inline void set_group_ranges(const std::vector<int> &ranges) noexcept {
        m_group_ranges = ranges;
    }

    /**
     * @brief Sets the group colors vector for snapshot creation
     * @param colors The group colors to set
     */
    inline void set_group_colors(const std::vector<Color> &colors) noexcept {
        m_group_colors = colors;
    }

    /**
     * @brief Sets the group radii squared vector for snapshot creation
     * @param radii2 The group radii squared to set
     */
    inline void set_group_radii2(const std::vector<float> &radii2) noexcept {
        m_group_radii2 = radii2;
    }

    /**
     * @brief Sets the group enabled states vector for snapshot creation
     * @param enabled The group enabled states to set
     */
    inline void set_group_enabled(const std::vector<bool> &enabled) noexcept {
        m_group_enabled = enabled;
    }

    /**
     * @brief Sets the rules matrix for snapshot creation
     * @param rules The rules matrix to set
     */
    inline void set_rules(const std::vector<float> &rules) noexcept {
        m_rules = rules;
    }

    /**
     * @brief Sets the particle groups vector for snapshot creation
     * @param groups The particle groups to set
     */
    inline void set_particle_groups(const std::vector<int> &groups) noexcept {
        m_particle_groups = groups;
    }

  protected:
    std::vector<int>
        m_group_ranges; // Group ranges: each group has 2 items (start, end)
    std::vector<Color> m_group_colors; // Color for each group
    std::vector<float>
        m_group_radii2; // Interaction radius squared for each group
    std::vector<bool> m_group_enabled; // Enable/disable state for each group
    std::vector<float>
        m_rules; // Interaction rules matrix (size G*G: rules[src*G + dst])
    std::vector<int> m_particle_groups; // Group index for each particle
};

} // namespace particles
