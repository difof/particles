#ifndef __UNIFORM_GRID_HPP
#define __UNIFORM_GRID_HPP

#include <algorithm>
#include <cmath>
#include <concepts>
#include <vector>

template <typename F>
concept FloatGetter = requires(F f, int i) {
    { f(i) } -> std::convertible_to<float>;
};

class UniformGrid {
  public:
    UniformGrid() = default;
    ~UniformGrid() = default;
    UniformGrid(const UniformGrid &) = delete;
    UniformGrid(UniformGrid &&) = delete;
    UniformGrid &operator=(const UniformGrid &) = delete;
    UniformGrid &operator=(UniformGrid &&) = delete;

    inline float cell() const { return m_cell; }
    inline int cols() const { return m_cols; }
    inline int rows() const { return m_rows; }
    inline const std::vector<int> &head() const { return m_head; }
    inline int head_at(int i) const { return m_head[i]; }
    inline const std::vector<int> &next() const { return m_next; }
    inline int next_at(int i) const { return m_next[i]; }

    inline int cellIndex(int cx, int cy) const {
        if (cx < 0 || cy < 0 || cx >= m_cols || cy >= m_rows) {
            return -1;
        }
        return cy * m_cols + cx;
    }

    inline void resize(float width, float height, float cellSize, int count) {
        m_cell = std::max(1.0f, cellSize);
        m_cols = std::max(1, (int)std::floor(width / m_cell));
        m_rows = std::max(1, (int)std::floor(height / m_cell));

        m_head.assign(m_cols * m_rows, -1);
        m_next.assign(count, -1);
    }

    template <FloatGetter GetX, FloatGetter GetY>
    inline void build(int count, GetX getx, GetY gety, float width,
                      float height) {
        // assume resize already called for current N and bounds
        std::fill(m_head.begin(), m_head.end(), -1);
        if ((int)m_next.size() != count) {
            m_next.assign(count, -1);
        }

        for (int i = 0; i < count; ++i) {
            float x = getx(i);
            float y = gety(i);

            // Clamp to [0, W/H) so indices are valid (positions bounce into
            // range)
            x = std::min(std::max(0.0f, x), std::nextafter(width, 0.0f));
            y = std::min(std::max(0.0f, y), std::nextafter(height, 0.0f));

            int cx = (int)std::floor(x / m_cell);
            int cy = (int)std::floor(y / m_cell);
            int ci = cellIndex(cx, cy);

            // push-front into list
            m_next[i] = m_head[ci];
            m_head[ci] = i;
        }
    }

  private:
    // side length of a cell (auto-picked)
    float m_cell = 64.f;

    // grid size
    int m_cols = 1, m_rows = 1;

    // size cols*rows, index of first particle in cell (-1 if none)
    std::vector<int> m_head;

    // size N, next particle in same cell (-1 if none)
    std::vector<int> m_next;
};

#endif