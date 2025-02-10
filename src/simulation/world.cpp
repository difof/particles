#include <algorithm>
#include <cmath>

#include "world.hpp"

void World::finalize_groups() {
    const int group_count = get_groups_size();
    m_p_group.assign(get_particles_size(), 0);
    for (int group_index = 0; group_index < group_count; ++group_index) {
        for (int particle_index = get_group_start(group_index);
             particle_index < get_group_end(group_index); ++particle_index) {
            m_p_group[particle_index] = group_index;
        }
    }
}

void World::init_rule_tables(int group_count) {
    if (group_count < 0) {
        throw particles::SimulationError("Invalid group count: " +
                                         std::to_string(group_count));
    }

    LOG_DEBUG("Initializing rule tables for " + std::to_string(group_count) +
              " groups");
    m_g_rules.assign(group_count * group_count, 0.f);
    m_g_radii2.assign(group_count, 0.f);
    m_g_enabled.assign(group_count, true); // default to enabled
}

float World::max_interaction_radius() const {
    float max_radius_squared = 0.f;

    for (float radius_squared : m_g_radii2) {
        max_radius_squared = std::max(max_radius_squared, radius_squared);
    }

    return (max_radius_squared > 0.f) ? std::sqrt(max_radius_squared) : 0.f;
}

float World::rule_val(int source_group, int destination_group) const {
    const int group_count = get_groups_size();
    if ((size_t)source_group >= m_g_radii2.size())
        return 0.f;
    if ((size_t)destination_group >= (size_t)group_count)
        return 0.f;
    if (m_g_rules.size() < (size_t)group_count * (size_t)group_count)
        return 0.f;
    return m_g_rules[source_group * group_count + destination_group];
}

int World::add_group(int particle_count, Color color) {
    if (particle_count <= 0) {
        throw particles::SimulationError("Invalid particle count: " +
                                         std::to_string(particle_count));
    }

    LOG_DEBUG("Adding group with " + std::to_string(particle_count) +
              " particles");
    int start_index = get_particles_size();

    m_g_ranges.push_back(start_index);
    m_particles.resize(m_particles.size() + particle_count * 4, 0.);
    m_g_ranges.push_back(get_particles_size());
    m_g_colors.push_back(color);

    return m_g_colors.size() - 1;
}

void World::reset(bool shrink) {
    m_particles.clear();
    m_g_ranges.clear();
    m_g_colors.clear();
    m_p_group.clear();
    m_g_rules.clear();
    m_g_radii2.clear();
    m_g_enabled.clear();

    if (shrink) {
        std::vector<float>().swap(m_particles);
        std::vector<int>().swap(m_g_ranges);
        std::vector<Color>().swap(m_g_colors);
        std::vector<int>().swap(m_p_group);
        std::vector<float>().swap(m_g_rules);
        std::vector<float>().swap(m_g_radii2);
        std::vector<bool>().swap(m_g_enabled);
    }
}

void World::remove_group(int group_index) {
    const int group_count = get_groups_size();
    if (group_index < 0 || group_index >= group_count)
        return;

    // particle span for this group
    const int start_index = get_group_start(group_index);
    const int end_index = get_group_end(group_index);
    const int particle_count = end_index - start_index;

    // 1) erase particles of the group (4 floats per particle)
    if (particle_count > 0) {
        const size_t float_start = size_t(start_index) * 4;
        const size_t float_end = size_t(end_index) * 4;
        m_particles.erase(m_particles.begin() + float_start,
                          m_particles.begin() + float_end);
    }

    // 2) drop color
    m_g_colors.erase(m_g_colors.begin() + group_index);

    // 3) fix spans of subsequent groups and remove this group's span
    // groups layout: [g0_start,g0_end, g1_start,g1_end, ...]
    // shift -particle_count for groups > group_index
    for (int group_i = group_index + 1; group_i < group_count; ++group_i) {
        m_g_ranges[group_i * 2 + 0] -= particle_count;
        m_g_ranges[group_i * 2 + 1] -= particle_count;
    }
    // erase this group's [start,end]
    m_g_ranges.erase(m_g_ranges.begin() + group_index * 2,
                     m_g_ranges.begin() + group_index * 2 + 2);

    // 4) rebuild p_group for new layout
    finalize_groups();

    // 5) prune rule matrix row/col group_index and radii2[group_index]
    // rules is size group_count*group_count; radii2 size group_count
    if (!m_g_rules.empty()) {
        const int old_group_count = group_count;
        std::vector<float> new_rules;
        new_rules.reserve((old_group_count - 1) * (old_group_count - 1));
        for (int i = 0; i < old_group_count; ++i) {
            if (i == group_index)
                continue;
            for (int j = 0; j < old_group_count; ++j) {
                if (j == group_index)
                    continue;
                new_rules.push_back(m_g_rules[i * old_group_count + j]);
            }
        }
        m_g_rules.swap(new_rules);
    }
    if (!m_g_radii2.empty()) {
        m_g_radii2.erase(m_g_radii2.begin() + group_index);
    }
    if (!m_g_enabled.empty()) {
        m_g_enabled.erase(m_g_enabled.begin() + group_index);
    }
}

void World::resize_group(int group_index, int new_size) {
    const int group_count = get_groups_size();
    if (group_index < 0 || group_index >= group_count || new_size < 0)
        return;

    const int current_size = get_group_size(group_index);
    const int start_index = get_group_start(group_index);

    if (new_size == current_size) {
        // Still need to finalize groups to ensure consistency
        finalize_groups();
        return;
    }

    if (new_size > current_size) {
        // Add particles
        const int add_count = new_size - current_size;
        m_particles.resize(m_particles.size() + add_count * 4, 0.);

        // Update group ranges for subsequent groups
        for (int group_i = group_index + 1; group_i < group_count; ++group_i) {
            m_g_ranges[group_i * 2 + 0] += add_count;
            m_g_ranges[group_i * 2 + 1] += add_count;
        }
        m_g_ranges[group_index * 2 + 1] += add_count;

    } else if (new_size < current_size) {
        // Remove particles
        const int remove_count = current_size - new_size;
        const int end_index = get_group_end(group_index);

        // Remove particles from the end of the group
        const size_t float_start = size_t(end_index - remove_count) * 4;
        const size_t float_end = size_t(end_index) * 4;
        m_particles.erase(m_particles.begin() + float_start,
                          m_particles.begin() + float_end);

        // Update group ranges for subsequent groups
        for (int group_i = group_index + 1; group_i < group_count; ++group_i) {
            m_g_ranges[group_i * 2 + 0] -= remove_count;
            m_g_ranges[group_i * 2 + 1] -= remove_count;
        }
        m_g_ranges[group_index * 2 + 1] -= remove_count;
    }

    finalize_groups();
}

void World::preserve_rules_on_add_group() {
    const int old_group_count =
        get_groups_size() - 1; // -1 because we just added a group
    if (old_group_count <= 0)
        return;

    const int new_group_count = get_groups_size();

    // Backup old data
    std::vector<float> old_rules = m_g_rules;
    std::vector<float> old_radii2 = m_g_radii2;
    std::vector<bool> old_enabled = m_g_enabled;

    // Reinitialize rule tables
    init_rule_tables(new_group_count);

    // Restore old rules
    for (int i = 0; i < old_group_count; ++i) {
        for (int j = 0; j < old_group_count; ++j) {
            set_rule(i, j, old_rules[i * old_group_count + j]);
        }
        set_r2(i, old_radii2[i]);
        set_group_enabled(i, old_enabled[i]);
    }
}
