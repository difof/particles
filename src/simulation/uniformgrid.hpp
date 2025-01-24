#ifndef __UNIFORM_GRID_HPP
#define __UNIFORM_GRID_HPP

#include <algorithm>
#include <cassert>
#include <cmath>
#include <concepts>
#include <vector>

/**
 * @brief Concept for a callable that returns an item's coordinate as float.
 * @details Must be invocable as f(int index) -> float.
 */
template <typename F>
concept FloatGetter = requires(F f, int i) {
    { f(i) } -> std::convertible_to<float>;
};

/**
 * @brief Fixed-cell-size 2D spatial hash for N items.
 *
 * @details
 * The space [0,width) × [0,height) is partitioned into a grid of cells of size
 * @p cellSize (clamped to at least 1). Each item @p i has an (x,y) position
 * provided by user callbacks.
 *
 * After calling resize(width,height,cellSize,count) and
 * build(count,getx,gety,...), the grid stores, per cell, a forward-linked list
 * of the item indices that fell into it:
 *
 * - @ref m_head has one entry per cell (size rows*cols). @c m_head[ci] is the
 * index of the first item in that cell, or -1 if empty.
 * - @ref m_next has one entry per item (size count). @c m_next[i] is the next
 * item index in the same cell as @c i, or -1 if the end of the list.
 *
 * This “struct-of-arrays linked list” is cache-friendly and avoids per-node
 * allocations.
 *
 * Typical usage pattern:
 * @code
 * grid.resize(worldW, worldH, cellSize, N);
 * grid.build(N, getX, getY, worldW, worldH);
 *
 * // iterate items in cell (cx,cy):
 * int ci = grid.cellIndex(cx, cy);
 * for (int i = grid.head_at(ci); i != -1; i = grid.next_at(i)) {
 *     // ... process item i ...
 * }
 * @endcode
 *
 * Neighbor queries: check the 8 neighbors around a cell plus the cell itself
 * and traverse their lists (see example at the bottom of this file-level
 * comment).
 *
 * Complexity:
 * - build: O(N) time, O(rows*cols + N) memory
 * - query by cell: O(k) where k is items in that cell
 *
 * Notes & caveats:
 * - Coordinates are clamped into the grid: out-of-bounds or non-finite
 * positions are placed at (0,0) and then clamped to [0..cols-1], [0..rows-1].
 * - You must call @ref resize before @ref build when N or bounds change.
 * - The structure is not thread-safe by itself; you can read concurrently after
 * build, but don’t mutate @ref m_head / @ref m_next while reading.
 *
 * Example: iterating a 3×3 neighborhood around the cell containing an item i0:
 * @code
 * auto cx = int(std::floor(getX(i0) / grid.cell()));
 * auto cy = int(std::floor(getY(i0) / grid.cell()));
 * for (int dy = -1; dy <= 1; ++dy) {
 *   for (int dx = -1; dx <= 1; ++dx) {
 *     int ci = grid.cellIndex(cx+dx, cy+dy);
 *     if (ci < 0) continue; // outside grid
 *     for (int j = grid.head_at(ci); j != -1; j = grid.next_at(j)) {
 *       if (j == i0) continue; // optional: skip self
 *       // ... neighbor candidate j ...
 *     }
 *   }
 * }
 * @endcode
 */
class UniformGrid {
  public:
    UniformGrid() = default;
    ~UniformGrid() = default;
    UniformGrid(const UniformGrid &) = delete;
    UniformGrid(UniformGrid &&) = delete;
    UniformGrid &operator=(const UniformGrid &) = delete;
    UniformGrid &operator=(UniformGrid &&) = delete;

    /**
     * @brief Reset to a minimal 1×1 grid and clear all links.
     */
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
    inline float cell_size() const { return m_cell; }
    inline int cols() const { return m_cols; }
    inline int rows() const { return m_rows; }
    inline float inv_cell() const { return 1.0f / m_cell; }

    /**
     * @brief Map a point (x,y) to clamped cell coordinates.
     * @note (Hot path: single floor + clamp, no branches beyond bounds checks.)
     */
    inline void cell_of(float x, float y, int &cx, int &cy) const {
        const float invc = 1.0f / m_cell;
        const int max_cx = m_cols - 1;
        const int max_cy = m_rows - 1;

        int ix = (int)std::floor(x * invc);
        int iy = (int)std::floor(y * invc);
        cx = std::clamp(ix, 0, max_cx);
        cy = std::clamp(iy, 0, max_cy);
    }

    /**
     * @brief Read-only view of the per-cell head array.
     * @details Size is rows*cols. @c head()[ci] is the first item in cell @c
     * ci, or -1.
     */
    inline const std::vector<int> &head() const { return m_head; }
    inline int head_at(int ci) const { return m_head[ci]; }

    /**
     * @brief Read-only view of the per-item next array.
     * @details Size is N (count passed to @ref resize/@ref build). @c next()[i]
     * is the next item index in the same cell as @c i, or -1.
     */
    inline const std::vector<int> &next() const { return m_next; }
    inline int next_at(int i) const { return m_next[i]; }

    // CSR-style contiguous storage accessors
    inline const std::vector<int> &cell_start() const { return m_cellStart; }
    inline const std::vector<int> &cell_count() const { return m_cellCount; }
    inline int cell_start_at(int ci) const { return m_cellStart[ci]; }
    inline int cell_count_at(int ci) const { return m_cellCount[ci]; }
    inline const std::vector<int> &indices() const { return m_indices; }

    /**
     * @brief Convert (cx,cy) cell coordinates to a flat cell index, or -1 if
     * out of range.
     */
    inline int cell_index(int cx, int cy) const {
        if (cx < 0 || cy < 0 || cx >= m_cols || cy >= m_rows) {
            return -1;
        }
        return cy * m_cols + cx;
    }

    /**
     * @brief Resize/reinitialize the grid for new bounds and item count.
     *
     * @param width     World width in the same units as your item positions (>=
     * 1).
     * @param height    World height (>= 1).
     * @param cellSize  Size of one cell (>= 1). Smaller → more cells, fewer
     * items per cell.
     * @param count     Number of items (N). Sets the size of @ref m_next.
     *
     * @details
     * Computes @ref m_cols and @ref m_rows as ceil(width/cellSize) and
     * ceil(height/cellSize), allocates @ref m_head with rows*cols initialized
     * to -1 and @ref m_next with N initialized to -1. Must be called before
     * @ref build whenever N or bounds change.
     */
    inline void resize(float width, float height, float cellSize, int count) {
        m_cell = std::max(1.0f, cellSize);
        m_width = std::max(1.0f, width);
        m_height = std::max(1.0f, height);

        // ceil so the grid covers the entire [0,width) × [0,height) box
        m_cols = std::max(1, (int)std::ceil(m_width / m_cell));
        m_rows = std::max(1, (int)std::ceil(m_height / m_cell));

        const int C = m_cols * m_rows;
        m_head.assign(C, -1);
        m_next.assign(count, -1);
        // CSR buffers
        m_cellStart.assign(C, 0);
        m_cellCount.assign(C, 0);
        m_indices.assign(count, -1);
    }

    /**
     * @brief Populate cell lists from item positions.
     *
     * @tparam GetX Callable int->float that returns x of item i
     * @tparam GetY Callable int->float that returns y of item i
     * @param count  Number of items (N). Must match the size passed to @ref
     * resize.
     * @param getx   X accessor
     * @param gety   Y accessor
     * @param width  Unused here (kept for API symmetry; bounds come from @ref
     * resize).
     * @param height Unused here.
     *
     * @details
     * For each item i:
     *  1) x = getx(i), y = gety(i); if non-finite → set to (0,0)
     *  2) Map to cell coords: cx=floor(x/cell), cy=floor(y/cell), then clamp to
     * the grid 3) Push-front into the cell’s list: m_next[i] = m_head[ci];
     *       m_head[ci] = i;
     *
     * After this, every cell’s items can be traversed by following @ref m_next
     * starting at @ref m_head.
     */
    template <FloatGetter GetX, FloatGetter GetY>
    inline void build(int count, GetX getx, GetY gety, float /*width*/,
                      float /*height*/) {
        // Clear/resize structures
        std::fill(m_head.begin(), m_head.end(), -1);
        if ((int)m_next.size() != count)
            m_next.assign(count, -1);
        if ((int)m_indices.size() != count)
            m_indices.assign(count, -1);
        std::fill(m_cellCount.begin(), m_cellCount.end(), 0);

#ifndef NDEBUG
        // Catch mismatched resize/build quickly in debug builds
        assert(m_cols > 0 && m_rows > 0);
        assert((int)m_head.size() == m_cols * m_rows);
        assert((int)m_next.size() == count);
#endif

        const int max_cx = m_cols - 1;
        const int max_cy = m_rows - 1;
        const float inv_cell = 1.0f / m_cell;

        // First pass: compute per-item cell, count items per cell, and build
        // head/next lists
        if ((int)m_itemCell.size() != count)
            m_itemCell.assign(count, 0);
        for (int i = 0; i < count; ++i) {
            float x = getx(i);
            float y = gety(i);

            if (!std::isfinite(x) || !std::isfinite(y)) {
                x = 0.0f;
                y = 0.0f;
            }

            int cx = (int)std::floor(x * inv_cell);
            int cy = (int)std::floor(y * inv_cell);
            cx = std::clamp(cx, 0, max_cx);
            cy = std::clamp(cy, 0, max_cy);
            const int ci = cy * m_cols + cx;
            m_itemCell[i] = ci;
            // linked list
            m_next[i] = m_head[ci];
            m_head[ci] = i;
            // CSR counts
            m_cellCount[ci] += 1;
        }

        // Exclusive scan over counts to produce starts
        int running = 0;
        for (int ci = 0; ci < (int)m_cellStart.size(); ++ci) {
            int cnt = m_cellCount[ci];
            m_cellStart[ci] = running;
            running += cnt;
        }

        // Fill indices using a scratch cursor per cell
        if ((int)m_cursor.size() != (int)m_cellStart.size())
            m_cursor.assign(m_cellStart.size(), 0);
        // initialize cursor to starts
        for (size_t ci = 0; ci < m_cellStart.size(); ++ci)
            m_cursor[ci] = m_cellStart[ci];
        for (int i = 0; i < count; ++i) {
            const int ci = m_itemCell[i];
            const int pos = m_cursor[ci]++;
            m_indices[pos] = i;
        }
    }

  private:
    // Grid configuration
    float m_cell = 64.f;   ///< Cell size (world units)
    float m_width = 64.f;  ///< World width (world units)
    float m_height = 64.f; ///< World height (world units)
    int m_cols = 1;        ///< Number of columns (ceil(width/cell))
    int m_rows = 1;        ///< Number of rows    (ceil(height/cell))

    /**
     * @brief Per-cell list heads (size rows*cols).
     * @details For cell ci, @c m_head[ci] is the index of the first item in
     * that cell, or -1.
     */
    std::vector<int> m_head;

    /**
     * @brief Per-item next pointers (size N).
     * @details For item i, @c m_next[i] is the next item index in the same
     * cell, or -1.
     */
    std::vector<int> m_next;

    // CSR-style contiguous storage per cell
    std::vector<int> m_cellStart; // size rows*cols
    std::vector<int> m_cellCount; // size rows*cols
    std::vector<int> m_indices;   // size N, contiguous ranges per cell

    // transient buffers reused across builds
    std::vector<int> m_itemCell; // size N
    std::vector<int> m_cursor;   // size rows*cols
};

#endif
