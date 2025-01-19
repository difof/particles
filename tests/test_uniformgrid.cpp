#include <catch_amalgamated.hpp>

#include "simulation/uniformgrid.hpp"

TEST_CASE("UniformGrid basic build and access", "[uniformgrid]") {
    UniformGrid grid;
    const int N = 4;
    const float W = 10.f, H = 10.f, C = 5.f;
    grid.resize(W, H, C, N);

    float xs[N] = {1.f, 2.f, 6.f, 7.f};
    float ys[N] = {1.f, 2.f, 6.f, 7.f};

    grid.build(
        N,
        [&](int i) {
            return xs[i];
        },
        [&](int i) {
            return ys[i];
        },
        W, H);

    REQUIRE(grid.cols() == 2);
    REQUIRE(grid.rows() == 2);

    int cx, cy;
    grid.cell_of(1.f, 1.f, cx, cy);
    REQUIRE(cx == 0);
    REQUIRE(cy == 0);

    const int ci00 = grid.cell_index(0, 0);
    REQUIRE(ci00 >= 0);

    // Items 0,1 should be somewhere in cell (0,0)
    int seen01 = 0;
    for (int i = grid.head_at(ci00); i != -1; i = grid.next_at(i)) {
        if (i == 0 || i == 1)
            seen01++;
    }
    REQUIRE(seen01 == 2);

    // Items 2,3 in (1,1)
    const int ci11 = grid.cell_index(1, 1);
    int seen23 = 0;
    for (int i = grid.head_at(ci11); i != -1; i = grid.next_at(i)) {
        if (i == 2 || i == 3)
            seen23++;
    }
    REQUIRE(seen23 == 2);
}

TEST_CASE("UniformGrid clamps out-of-bounds and non-finite", "[uniformgrid]") {
    UniformGrid grid;
    const int N = 3;
    grid.resize(10.f, 10.f, 4.f, N);

    float xs[N] = {-1000.f, std::numeric_limits<float>::infinity(), 9.f};
    float ys[N] = {-1000.f, 5.f, 9.f};

    grid.build(
        N,
        [&](int i) {
            return xs[i];
        },
        [&](int i) {
            return ys[i];
        },
        10.f, 10.f);

    int cx, cy;
    grid.cell_of(-1.f, -1.f, cx, cy);
    REQUIRE(cx == 0);
    REQUIRE(cy == 0);

    // Third item should map near max cell
    grid.cell_of(9.f, 9.f, cx, cy);
    REQUIRE(cx == grid.cols() - 1);
    REQUIRE(cy == grid.rows() - 1);
}
