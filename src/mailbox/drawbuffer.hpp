#ifndef __MAILBOX_DRAWWBUFFER_HPP
#define __MAILBOX_DRAWWBUFFER_HPP

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <vector>

namespace mailbox {

// FIXME: begin_read() spins until it can pin two slots. That’s OK here, but
// consider a bounded spin + backoff or yielding to avoid potential UI hitching
// on contention.

struct DrawBuffer {
    static constexpr int kNumSlots = 3;

    static inline uint32_t pack_pair(uint32_t prev, uint32_t curr) {
        return ((prev & 0xFFu) << 8) | (curr & 0xFFu);
    }
    static inline int unpack_prev(uint32_t pair) {
        return int((pair >> 8) & 0xFF);
    }
    static inline int unpack_curr(uint32_t pair) {
        return int((pair >> 0) & 0xFF);
    }
    static inline uint8_t bit(int i) { return uint8_t(1u << i); }

    struct GridFrame {
        float cell = 64.f;
        int cols = 1, rows = 1;
        float width = 0.f, height = 0.f;

        std::vector<int> head;
        std::vector<int> next;
        std::vector<int> count;

        // per-cell velocity accumulation for the frame (sum, not avg)
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

    struct Slot {
        std::vector<float> pos;
        std::vector<float> vel;

        GridFrame grid;

        std::atomic<long long> stamp_ns{0};
    };

    Slot slots[kNumSlots];

    std::atomic<uint32_t> pair{pack_pair(0u, 0u)};
    mutable std::atomic<uint8_t> in_use{0};

    int write_idx = 0;

    int acquire_write_index() const {
        const uint32_t p = pair.load(std::memory_order_acquire);
        const int prev = unpack_prev(p);
        const int curr = unpack_curr(p);
        const uint8_t pinned = in_use.load(std::memory_order_acquire);
        for (int i = 0; i < kNumSlots; ++i)
            if (i != prev && i != curr && (pinned & bit(i)) == 0)
                return i;
        for (int i = 0; i < kNumSlots; ++i)
            if (i != curr)
                return i;
        return 0;
    }

    std::vector<float> &begin_write_pos(size_t floats_needed) {
        write_idx = acquire_write_index();
        auto &v = slots[write_idx].pos;
        if (v.size() != floats_needed)
            v.resize(floats_needed);
        // std::fill(v.begin(), v.end(), 0.0f);
        return v;
    }

    std::vector<float> &begin_write_vel(size_t floats_needed) {
        auto &v = slots[write_idx].vel;
        if (v.size() != floats_needed)
            v.resize(floats_needed);
        // std::fill(v.begin(), v.end(), 0.0f);
        return v;
    }

    GridFrame &begin_write_grid(int cols, int rows, int N, float cell_size,
                                float width, float height) {
        auto &g = slots[write_idx].grid;
        g.cell = cell_size;
        g.width = width;
        g.height = height;
        g.resize(cols, rows, N);
        g.clear_accum();
        return g;
    }

    void publish(long long stamp_ns) {
        slots[write_idx].stamp_ns.store(stamp_ns, std::memory_order_relaxed);
        const uint32_t old = pair.load(std::memory_order_relaxed);
        const uint32_t old_curr = uint32_t(unpack_curr(old));
        const uint32_t new_pair = pack_pair(old_curr, uint32_t(write_idx));
        pair.store(new_pair, std::memory_order_release);
    }

    void bootstrap_same_as_current(size_t floats_needed, long long stamp_ns) {
        begin_write_pos(floats_needed);
        publish(stamp_ns);
    }

    // ---- reader view ----
    struct ReadView {
        const std::vector<float> *prev = nullptr;
        const std::vector<float> *curr = nullptr;
        const std::vector<float> *curr_vel = nullptr;

        const GridFrame *grid = nullptr;

        long long t0 = 0, t1 = 0;
        uint8_t mask = 0;
    };

    ReadView begin_read() const {
        for (;;) {
            const uint32_t p = pair.load(std::memory_order_acquire);
            const int prev = unpack_prev(p);
            const int curr = unpack_curr(p);

            const uint8_t want = uint8_t(bit(prev) | bit(curr));
            uint8_t old = in_use.load(std::memory_order_relaxed);
            if ((old & want) != 0)
                continue;

            if (in_use.compare_exchange_weak(old, uint8_t(old | want),
                                             std::memory_order_acq_rel,
                                             std::memory_order_relaxed)) {
                ReadView v;
                v.prev = &slots[prev].pos;
                v.curr = &slots[curr].pos;
                v.curr_vel = &slots[curr].vel;
                v.grid = &slots[curr].grid;
                v.t0 = slots[prev].stamp_ns.load(std::memory_order_relaxed);
                v.t1 = slots[curr].stamp_ns.load(std::memory_order_relaxed);
                v.mask = want;
                return v;
            }
        }
    }

    void end_read(const ReadView &v) const {
        in_use.fetch_and(uint8_t(~v.mask), std::memory_order_release);
    }

    const std::vector<float> &read_current_only() const {
        const uint32_t p = pair.load(std::memory_order_acquire);
        const int curr = unpack_curr(p);
        return slots[curr].pos;
    }
};

} // namespace mailbox
#endif
