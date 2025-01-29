#pragma once

#include <algorithm>
#include <cmath>
#include <raylib.h>
#include <vector>

#include "../utility/exceptions.hpp"
#include "../utility/logger.hpp"

class World {
  public:
    struct RuleRowView {
        const float *row;
        int size;
        inline float get(int j) const noexcept {
            if (!row || j < 0 || j >= size)
                return 0.f;
            return row[j];
        }
    };
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
    float rule_val(int gsrc, int gdst) const;
    inline RuleRowView rules_of(int gsrc) const noexcept {
        const int G = get_groups_size();
        if ((size_t)gsrc >= m_g_radii2.size())
            return {nullptr, G};
        if (m_g_rules.size() < (size_t)G * (size_t)G)
            return {nullptr, G};
        return {&m_g_rules[gsrc * G], G};
    }
    void remove_group(int g);

  public:
    inline int get_groups_size() const noexcept {
        return (int)m_g_ranges.size() / 2;
    }
    inline int get_group_start(int g) const noexcept {
        return m_g_ranges[g * 2 + 0];
    }
    inline int get_group_end(int g) const noexcept {
        return m_g_ranges[g * 2 + 1];
    }

    inline int get_group_size(int g) const noexcept {
        return get_group_end(g) - get_group_start(g);
    }

    inline int get_particles_size() const noexcept {
        return (int)m_particles.size() / 4;
    }

    inline void set_group_color(int g, Color c) {
        if ((size_t)g < m_g_colors.size())
            m_g_colors[g] = c;
    }
    inline Color get_group_color(int g) const noexcept { return m_g_colors[g]; }

    inline float get_px(int idx) const noexcept {
        return m_particles[idx * 4 + 0];
    }
    inline float get_py(int idx) const noexcept {
        return m_particles[idx * 4 + 1];
    }
    inline float get_vx(int idx) const noexcept {
        return m_particles[idx * 4 + 2];
    }
    inline float get_vy(int idx) const noexcept {
        return m_particles[idx * 4 + 3];
    }
    inline void set_px(int idx, float v) noexcept {
        m_particles[idx * 4 + 0] = v;
    }
    inline void set_py(int idx, float v) noexcept {
        m_particles[idx * 4 + 1] = v;
    }
    inline void set_vx(int idx, float v) noexcept {
        m_particles[idx * 4 + 2] = v;
    }
    inline void set_vy(int idx, float v) noexcept {
        m_particles[idx * 4 + 3] = v;
    }

    inline void set_rule(int g_src, int g_dst, float v) {
        m_g_rules[g_src * get_groups_size() + g_dst] = v;
    }

    inline void set_r2(int g_src, float r2) { m_g_radii2[g_src] = r2; }

    inline int group_of(int i) const noexcept { return m_p_group[i]; }

    inline float r2_of(int gsrc) const noexcept {
        if ((size_t)gsrc >= m_g_radii2.size())
            return 0.f;
        return m_g_radii2[gsrc];
    }

    inline bool is_group_enabled(int g) const noexcept {
        if ((size_t)g >= m_g_enabled.size())
            return true; // default to enabled
        return m_g_enabled[g];
    }

    inline void set_group_enabled(int g, bool enabled) {
        if ((size_t)g < m_g_enabled.size())
            m_g_enabled[g] = enabled;
    }

  private:
    // each particle takes 4 items
    // px, py, vx, vy
    std::vector<float> m_particles;
    std::vector<int> m_p_group; // size N: group index per particle
    // each group takes 2 items
    // p_start, p_end
    std::vector<int> m_g_ranges;
    std::vector<float> m_g_rules; // size G*G: rules[src*G + dst]
    std::vector<float>
        m_g_radii2; // size G: interaction radius^2 for source group
    std::vector<Color> m_g_colors;
    std::vector<bool> m_g_enabled; // size G: enable/disable state per group
};
