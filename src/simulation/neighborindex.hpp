#pragma once

#include "uniformgrid.hpp"
#include "world.hpp"

/**
 * @brief Spatial acceleration structure for efficient particle neighbor finding
 * @details Caches a UniformGrid to avoid expensive rebuilds when simulation
 * parameters haven't changed. Provides optimized spatial queries for particle
 * physics calculations.
 */
struct NeighborIndex {
    /** @brief The underlying spatial hash grid */
    UniformGrid grid;

    /** @brief Cached particle count from last build */
    int lastN = -1;

    /** @brief Cached world width from last build */
    float lastW = -1.f;

    /** @brief Cached world height from last build */
    float lastH = -1.f;

    /** @brief Cached cell size from last build */
    float lastCell = -1.f;

    /**
     * @brief Ensures the spatial grid is up-to-date and returns inverse cell
     * size
     * @param w World containing particles to index
     * @param W World width
     * @param H World height
     * @param cell Cell size for spatial partitioning
     * @return Inverse cell size (1.0f / cell) for efficient distance
     * calculations
     * @details Rebuilds the grid only if parameters have changed since last
     * call
     */
    inline float ensure(const World &w, float W, float H, float cell) {
        const int N = w.get_particles_size();
        const bool needResize =
            (N != lastN) || (W != lastW) || (H != lastH) || (cell != lastCell);
        if (needResize) {
            grid.resize(W, H, cell, N);
            lastN = N;
            lastW = W;
            lastH = H;
            lastCell = cell;
        }
        grid.build(
            N,
            [&w](int i) {
                return w.get_px(i);
            },
            [&w](int i) {
                return w.get_py(i);
            },
            W, H);
        return grid.inv_cell();
    }
};
