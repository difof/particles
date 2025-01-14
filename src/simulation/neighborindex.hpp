#ifndef __NEIGHBOR_INDEX_HPP
#define __NEIGHBOR_INDEX_HPP

#include "uniformgrid.hpp"
#include "world.hpp"

struct NeighborIndex {
    UniformGrid grid;
    int lastN = -1;
    float lastW = -1.f;
    float lastH = -1.f;
    float lastCell = -1.f;

    // returns inverse_cell for kernels
    inline float ensure(const World &w, float W, float H, float cell) {
        const int N = w.get_particles_count();
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

#endif
