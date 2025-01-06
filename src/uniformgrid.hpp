#ifndef __UNIFORM_GRID_HPP
#define __UNIFORM_GRID_HPP

#include <vector>
#include <cmath>
#include <algorithm>

struct UniformGrid
{
    float cell = 64.f;      // side length of a cell (auto-picked)
    int cols = 1, rows = 1; // grid size
    std::vector<int> head;  // size cols*rows, index of first particle in cell (-1 if none)
    std::vector<int> next;  // size N, next particle in same cell (-1 if none)

    inline int cellIndex(int cx, int cy) const
    {
        if (cx < 0 || cy < 0 || cx >= cols || cy >= rows)
            return -1;
        return cy * cols + cx;
    }

    inline void resize(float W, float H, float cellSize, int N)
    {
        cell = std::max(1.0f, cellSize);
        cols = std::max(1, (int)std::floor(W / cell));
        rows = std::max(1, (int)std::floor(H / cell));
        head.assign(cols * rows, -1);
        next.assign(N, -1);
    }

    template <typename GetX, typename GetY>
    inline void build(int N, GetX getx, GetY gety, float W, float H)
    {
        // assume resize already called for current N and bounds
        std::fill(head.begin(), head.end(), -1);
        if ((int)next.size() != N)
            next.assign(N, -1);

        for (int i = 0; i < N; ++i)
        {
            float x = getx(i);
            float y = gety(i);
            // Clamp to [0, W/H) so indices are valid (positions bounce into range)
            x = std::min(std::max(0.0f, x), std::nextafter(W, 0.0f));
            y = std::min(std::max(0.0f, y), std::nextafter(H, 0.0f));
            int cx = (int)std::floor(x / cell);
            int cy = (int)std::floor(y / cell);
            int ci = cellIndex(cx, cy);
            // push-front into list
            next[i] = head[ci];
            head[ci] = i;
        }
    }
};

#endif