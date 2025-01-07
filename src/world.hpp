#ifndef __WORLD_HPP
#define __WORLD_HPP

#include <algorithm>
#include <cmath>
#include <raylib.h>
#include <vector>

class World {
  public:
    World() = default;
    ~World() = default;
    World(const World &) = delete;
    World(World &&) = delete;
    World &operator=(const World &) = delete;
    World &operator=(World &&) = delete;

    void finalize_groups();
    void init_rule_tables(int G);
    float max_interaction_radius() const;
    int add_group(int count, Color color);
    void reset(bool shrink = false);

  public:
    inline int get_groups_size() const { return (int)groups.size() / 2; }
    inline int get_group_start(int g) const { return groups[g * 2 + 0]; }
    inline int get_group_end(int g) const { return groups[g * 2 + 1]; }

    inline int get_group_size(int g) const {
        return get_group_end(g) - get_group_start(g);
    }

    inline int get_particles_size() const { return (int)particles.size() / 4; }

    inline Color *get_group_color(int g) { return &g_colors[g]; }
    inline const std::vector<Color> &colors() const { return g_colors; }
    inline const std::vector<int> &group_spans() const { return groups; }

    inline float get_px(int idx) const { return particles[idx * 4 + 0]; }
    inline float get_py(int idx) const { return particles[idx * 4 + 1]; }
    inline float get_vx(int idx) const { return particles[idx * 4 + 2]; }
    inline float get_vy(int idx) const { return particles[idx * 4 + 3]; }
    inline void set_px(int idx, float v) { particles[idx * 4 + 0] = v; }
    inline void set_py(int idx, float v) { particles[idx * 4 + 1] = v; }
    inline void set_vx(int idx, float v) { particles[idx * 4 + 2] = v; }
    inline void set_vy(int idx, float v) { particles[idx * 4 + 3] = v; }

    inline void set_rule(int g_src, int g_dst, float v) {
        rules[g_src * get_groups_size() + g_dst] = v;
    }

    inline void set_r2(int g_src, float r2) { radii2[g_src] = r2; }

    inline int group_of(int i) const { return p_group[i]; }

    inline float rule_val(int gsrc, int gdst) const {
        return rules[gsrc * get_groups_size() + gdst];
    }

    inline float r2_of(int gsrc) const { return radii2[gsrc]; }

    inline const float *rules_row(int gsrc) const {
        return &rules[gsrc * get_groups_size()];
    }

  private:
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
};

#endif