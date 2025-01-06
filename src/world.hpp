#ifndef __WORLD_HPP
#define __WORLD_HPP

#include <raylib.h>
#include <vector>
#include <cmath>
#include <algorithm>

class World
{
    // each particle takes 4 items
    // px, py, vx, vy
    std::vector<float> particles;

    // each group takes 2 items
    // p_start, p_end
    std::vector<int> groups;

    std::vector<Color> g_colors;
    std::vector<int> p_group;  // size N: group index per particle
    std::vector<float> rules;  // size G*G: rules[src*G + dst]
    std::vector<float> radii2; // size G: interaction radius^2 for source group
public:
    void finalize_groups()
    {
        const int G = get_groups_size();
        p_group.assign(get_particles_size(), 0);
        for (int g = 0; g < G; ++g)
        {
            for (int i = get_group_start(g); i < get_group_end(g); ++i)
                p_group[i] = g;
        }
    }
    void init_rule_tables(int G)
    {
        rules.assign(G * G, 0.f);
        radii2.assign(G, 0.f);
    }
    void set_rule(int g_src, int g_dst, float v) { rules[g_src * get_groups_size() + g_dst] = v; }
    void set_r2(int g_src, float r2) { radii2[g_src] = r2; }

    int group_of(int i) const { return p_group[i]; }
    float rule_val(int gsrc, int gdst) const { return rules[gsrc * get_groups_size() + gdst]; }
    float r2_of(int gsrc) const { return radii2[gsrc]; }

    const float *rules_row(int gsrc) const
    {
        return &rules[gsrc * get_groups_size()];
    }

    float max_interaction_radius() const
    {
        float maxr2 = 0.f;
        for (float v : radii2)
            maxr2 = std::max(maxr2, v);
        return (maxr2 > 0.f) ? std::sqrt(maxr2) : 0.f;
    }

    int add_group(int count, Color color)
    {
        if (count <= 0)
        {
            return -1;
        }

        int start = get_particles_size();
        groups.push_back(start);
        particles.resize(particles.size() + count * 4, 0.);
        groups.push_back(get_particles_size());
        g_colors.push_back(color);
        return g_colors.size() - 1;
    }

    void reset(bool shrink = false)
    {
        particles.clear();
        groups.clear();
        g_colors.clear();
        p_group.clear();
        rules.clear();
        radii2.clear();

        if (shrink)
        {
            std::vector<float>().swap(particles);
            std::vector<int>().swap(groups);
            std::vector<Color>().swap(g_colors);
            std::vector<int>().swap(p_group);
            std::vector<float>().swap(rules);
            std::vector<float>().swap(radii2);
        }
    }

    Color *get_group_color(int g) { return &g_colors[g]; }

    int get_groups_size() const { return (int)groups.size() / 2; }
    int get_group_start(int g) const { return groups[g * 2 + 0]; }
    int get_group_end(int g) const { return groups[g * 2 + 1]; }
    int get_group_size(int g) const { return get_group_end(g) - get_group_start(g); }

    int get_particles_size() { return particles.size() / 4; }

    inline float get_px(int idx) const { return particles[idx * 4 + 0]; }
    inline float get_py(int idx) const { return particles[idx * 4 + 1]; }
    inline float get_vx(int idx) const { return particles[idx * 4 + 2]; }
    inline float get_vy(int idx) const { return particles[idx * 4 + 3]; }
    inline void set_px(int idx, float v) { particles[idx * 4 + 0] = v; }
    inline void set_py(int idx, float v) { particles[idx * 4 + 1] = v; }
    inline void set_vx(int idx, float v) { particles[idx * 4 + 2] = v; }
    inline void set_vy(int idx, float v) { particles[idx * 4 + 3] = v; }

    const std::vector<Color> &colors() const { return g_colors; }
    const std::vector<int> &group_spans() const { return groups; }
    const std::vector<float> &raw() const { return particles; } // for rendering if needed
};

#endif