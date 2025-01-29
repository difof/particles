#include "world.hpp"
#include <algorithm>
#include <cmath>

void World::finalize_groups() {
    const int G = get_groups_size();
    m_p_group.assign(get_particles_size(), 0);
    for (int g = 0; g < G; ++g) {
        for (int i = get_group_start(g); i < get_group_end(g); ++i) {
            m_p_group[i] = g;
        }
    }
}

void World::init_rule_tables(int G) {
    if (G < 0) {
        throw particles::SimulationError("Invalid group count: " +
                                         std::to_string(G));
    }

    LOG_DEBUG("Initializing rule tables for " + std::to_string(G) + " groups");
    m_g_rules.assign(G * G, 0.f);
    m_g_radii2.assign(G, 0.f);
    m_g_enabled.assign(G, true); // default to enabled
}

float World::max_interaction_radius() const {
    float maxr2 = 0.f;

    for (float v : m_g_radii2) {
        maxr2 = std::max(maxr2, v);
    }

    return (maxr2 > 0.f) ? std::sqrt(maxr2) : 0.f;
}

float World::rule_val(int gsrc, int gdst) const {
    const int G = get_groups_size();
    if ((size_t)gsrc >= m_g_radii2.size())
        return 0.f;
    if ((size_t)gdst >= (size_t)G)
        return 0.f;
    if (m_g_rules.size() < (size_t)G * (size_t)G)
        return 0.f;
    return m_g_rules[gsrc * G + gdst];
}

int World::add_group(int count, Color color) {
    if (count <= 0) {
        throw particles::SimulationError("Invalid particle count: " +
                                         std::to_string(count));
    }

    LOG_DEBUG("Adding group with " + std::to_string(count) + " particles");
    int start = get_particles_size();

    m_g_ranges.push_back(start);
    m_particles.resize(m_particles.size() + count * 4, 0.);
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

void World::remove_group(int g) {
    const int G = get_groups_size();
    if (g < 0 || g >= G)
        return;

    // particle span for this group
    const int start = get_group_start(g);
    const int end = get_group_end(g);
    const int cnt = end - start;

    // 1) erase particles of the group (4 floats per particle)
    if (cnt > 0) {
        const size_t f0 = size_t(start) * 4;
        const size_t f1 = size_t(end) * 4;
        m_particles.erase(m_particles.begin() + f0, m_particles.begin() + f1);
    }

    // 2) drop color
    m_g_colors.erase(m_g_colors.begin() + g);

    // 3) fix spans of subsequent groups and remove this group's span
    // groups layout: [g0_start,g0_end, g1_start,g1_end, ...]
    // shift -cnt for groups > g
    for (int gi = g + 1; gi < G; ++gi) {
        m_g_ranges[gi * 2 + 0] -= cnt;
        m_g_ranges[gi * 2 + 1] -= cnt;
    }
    // erase this group's [start,end]
    m_g_ranges.erase(m_g_ranges.begin() + g * 2,
                     m_g_ranges.begin() + g * 2 + 2);

    // 4) rebuild p_group for new layout
    finalize_groups();

    // 5) prune rule matrix row/col g and radii2[g]
    // rules is size G*G; radii2 size G
    if (!m_g_rules.empty()) {
        const int oldG = G;
        std::vector<float> newRules;
        newRules.reserve((oldG - 1) * (oldG - 1));
        for (int i = 0; i < oldG; ++i) {
            if (i == g)
                continue;
            for (int j = 0; j < oldG; ++j) {
                if (j == g)
                    continue;
                newRules.push_back(m_g_rules[i * oldG + j]);
            }
        }
        m_g_rules.swap(newRules);
    }
    if (!m_g_radii2.empty()) {
        m_g_radii2.erase(m_g_radii2.begin() + g);
    }
    if (!m_g_enabled.empty()) {
        m_g_enabled.erase(m_g_enabled.begin() + g);
    }
}