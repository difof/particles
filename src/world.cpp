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