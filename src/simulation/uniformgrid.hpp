#ifndef __UNIFORM_GRID_HPP
#define __UNIFORM_GRID_HPP

#include <algorithm>
#include <cassert>
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

    void reset() {
        m_cell = 64.f;
        m_width = 64.f;
        m_height = 64.f;
        m_cols = 1;
        m_rows = 1;
        m_head.clear();
        m_next.clear();
    }

    inline float width() const { return m_width; }
    inline float height() const { return m_height; }
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
        m_width = std::max(1.0f, width);
        m_height = std::max(1.0f, height);

        // was: floor(...)
        m_cols = std::max(1, (int)std::ceil(m_width / m_cell));
        m_rows = std::max(1, (int)std::ceil(m_height / m_cell));

        m_head.assign(m_cols * m_rows, -1);
        m_next.assign(count, -1);
    }

    template <FloatGetter GetX, FloatGetter GetY>
    inline void build(int count, GetX getx, GetY gety, float width,
                      float height) {
        // assume resize already called for current N and bounds
        std::fill(m_head.begin(), m_head.end(), -1);
        if ((int)m_next.size() != count)
            m_next.assign(count, -1);

        // Debug sanity (helps catch mismatched resize/build)
#ifndef NDEBUG
        assert(m_cols > 0 && m_rows > 0);
        assert((int)m_head.size() == m_cols * m_rows);
        assert((int)m_next.size() == count);
#endif

        // Precompute safe upper bounds for integer clamp
        const int max_cx = m_cols - 1;
        const int max_cy = m_rows - 1;
        const float inv_cell = 1.0f / m_cell;

        for (int i = 0; i < count; ++i) {
            float x = getx(i);
            float y = gety(i);

            // Handle NaNs/infs robustly
            if (!std::isfinite(x) || !std::isfinite(y)) {
                // Either skip or clamp to something valid. Clamping to 0 is
                // safe:
                x = 0.0f;
                y = 0.0f;
            }

            // Compute integer cell indices and CLAMP THE INDICES
            int cx = (int)std::floor(x * inv_cell);
            int cy = (int)std::floor(y * inv_cell);
            cx = std::clamp(cx, 0, max_cx);
            cy = std::clamp(cy, 0, max_cy);
            const int ci = cy * m_cols + cx;

            // push-front into list
            m_next[i] = m_head[ci];
            m_head[ci] = i;
        }
    }

  private:
    float m_cell;
    float m_width;
    float m_height;
    int m_cols;
    int m_rows;

    // size cols*rows, index of first particle in cell (-1 if none)
    std::vector<int> m_head;

    // size N, next particle in same cell (-1 if none)
    std::vector<int> m_next;
};

#endif