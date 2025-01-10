#include "world.hpp"
#include <algorithm>
#include <cmath>

void World::finalize_groups() {
    const int G = get_groups_size();
    p_group.assign(get_particles_size(), 0);
    for (int g = 0; g < G; ++g) {
        for (int i = get_group_start(g); i < get_group_end(g); ++i) {
            p_group[i] = g;
        }
    }
}

void World::init_rule_tables(int G) {
    rules.assign(G * G, 0.f);
    radii2.assign(G, 0.f);
}

float World::max_interaction_radius() const {
    float maxr2 = 0.f;

    for (float v : radii2) {
        maxr2 = std::max(maxr2, v);
    }

    return (maxr2 > 0.f) ? std::sqrt(maxr2) : 0.f;
}

float World::rule_val(int gsrc, int gdst) const {
    const int G = get_groups_size();
    if ((size_t)gsrc >= radii2.size())
        return 0.f;
    if ((size_t)gdst >= (size_t)G)
        return 0.f;
    if (rules.size() < (size_t)G * (size_t)G)
        return 0.f;
    return rules[gsrc * G + gdst];
}

const float *World::rules_row(int gsrc) const {
    const int G = get_groups_size();
    if ((size_t)gsrc >= radii2.size())
        return nullptr;
    if (rules.size() < (size_t)G * (size_t)G)
        return nullptr;
    return &rules[gsrc * G];
}

int World::add_group(int count, Color color) {
    if (count <= 0) {
        return -1;
    }

    int start = get_particles_size();

    groups.push_back(start);
    particles.resize(particles.size() + count * 4, 0.);
    groups.push_back(get_particles_size());
    g_colors.push_back(color);

    return g_colors.size() - 1;
}

void World::reset(bool shrink) {
    particles.clear();
    groups.clear();
    g_colors.clear();
    p_group.clear();
    rules.clear();
    radii2.clear();

    if (shrink) {
        std::vector<float>().swap(particles);
        std::vector<int>().swap(groups);
        std::vector<Color>().swap(g_colors);
        std::vector<int>().swap(p_group);
        std::vector<float>().swap(rules);
        std::vector<float>().swap(radii2);
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
        particles.erase(particles.begin() + f0, particles.begin() + f1);
    }

    // 2) drop color
    g_colors.erase(g_colors.begin() + g);

    // 3) fix spans of subsequent groups and remove this group's span
    // groups layout: [g0_start,g0_end, g1_start,g1_end, ...]
    // shift -cnt for groups > g
    for (int gi = g + 1; gi < G; ++gi) {
        groups[gi * 2 + 0] -= cnt;
        groups[gi * 2 + 1] -= cnt;
    }
    // erase this group's [start,end]
    groups.erase(groups.begin() + g * 2, groups.begin() + g * 2 + 2);

    // 4) rebuild p_group for new layout
    finalize_groups();

    // 5) prune rule matrix row/col g and radii2[g]
    // rules is size G*G; radii2 size G
    if (!rules.empty()) {
        const int oldG = G;
        std::vector<float> newRules;
        newRules.reserve((oldG - 1) * (oldG - 1));
        for (int i = 0; i < oldG; ++i) {
            if (i == g)
                continue;
            for (int j = 0; j < oldG; ++j) {
                if (j == g)
                    continue;
                newRules.push_back(rules[i * oldG + j]);
            }
        }
        rules.swap(newRules);
    }
    if (!radii2.empty()) {
        radii2.erase(radii2.begin() + g);
    }
}