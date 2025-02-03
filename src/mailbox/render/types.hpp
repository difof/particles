#pragma once

#include <atomic>
#include <vector>

namespace mailbox::render {

struct GridFrame {
    float cell = 64.f;
    int cols = 1, rows = 1;
    float width = 0.f, height = 0.f;

    std::vector<int> head;
    std::vector<int> next;
    std::vector<int> count;

    std::vector<float> sumVx;
    std::vector<float> sumVy;

    void resize(int c, int r, int N) {
        cols = std::max(1, c);
        rows = std::max(1, r);
        const int C = cols * rows;
        head.assign(C, -1);
        count.assign(C, 0);
        sumVx.assign(C, 0.f);
        sumVy.assign(C, 0.f);
        next.assign(N, -1);
    }

    void clear_accum() {
        std::fill(head.begin(), head.end(), -1);
        std::fill(count.begin(), count.end(), 0);
        std::fill(sumVx.begin(), sumVx.end(), 0.f);
        std::fill(sumVy.begin(), sumVy.end(), 0.f);
        std::fill(next.begin(), next.end(), -1);
    }
};

struct ReadView {
    const std::vector<float> *prev = nullptr;
    const std::vector<float> *curr = nullptr;
    const std::vector<float> *curr_vel = nullptr;
    const GridFrame *grid = nullptr;
    long long t0 = 0, t1 = 0;
    uint8_t mask = 0;
};

struct Slot {
    std::vector<float> pos;
    std::vector<float> vel;
    GridFrame grid;
    std::atomic<long long> stamp_ns{0};
};

} // namespace mailbox::render
