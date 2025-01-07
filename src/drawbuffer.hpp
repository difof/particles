#ifndef __DRAWWBUFFER_HPP
#define __DRAWWBUFFER_HPP

#include <atomic>
#include <cstdint>
#include <vector>

struct DrawBuffer {
    struct Slot {
        std::vector<float> pos;             // [i*2+0]=px, [i*2+1]=py
        std::atomic<long long> stamp_ns{0}; // monotonic time of this slot
    };

    // 3-slot mailbox
    Slot slots[3];

    // Packed (prev<<8)|curr published by the sim (writer) with release
    // semantics. Reader loads with acquire to get a consistent pair.
    std::atomic<uint32_t> pair{
        (0u << 8) | 0u}; // start degenerate; will be set after first publishes

    // Which slot the renderer has pinned for this frame (bitmask: bit i => slot
    // i in use)
    mutable std::atomic<uint8_t> in_use{0};

    // Writer-only state
    int write_idx = 0; // set by begin_write

    // --- Writer API (simulation thread) ---

    // Choose a free slot that is neither prev nor curr nor currently pinned by
    // reader.
    int acquire_write_index() const {
        const uint32_t p = pair.load(std::memory_order_acquire);
        const int prev = int((p >> 8) & 0xFF);
        const int curr = int((p >> 0) & 0xFF);
        const uint8_t mask = in_use.load(std::memory_order_acquire);

        for (int i = 0; i < 3; ++i) {
            if (i != prev && i != curr && ((mask & (1u << i)) == 0))
                return i;
        }
        // Fallback: if reader pinned both non-free (extremely unlikely), pick
        // the one that's not curr. We never overwrite 'curr'.
        for (int i = 0; i < 3; ++i) {
            if (i != curr)
                return i;
        }
        return 0; // should not happen
    }

    // Returns a writable vector for the sim step (size = 2*N floats)
    std::vector<float> &begin_write(size_t floats_needed) {
        write_idx = acquire_write_index();
        auto &v = slots[write_idx].pos;
        if (v.size() != floats_needed)
            v.assign(floats_needed, 0.f);
        return v;
    }

    // After filling the buffer, publish it as the new 'curr'. 'prev' becomes
    // old 'curr'.
    void publish(long long stamp_ns) {
        slots[write_idx].stamp_ns.store(stamp_ns, std::memory_order_relaxed);

        // load old pair, roll it forward: prev = old curr, curr = write_idx
        uint32_t old = pair.load(std::memory_order_relaxed);
        uint32_t old_curr = (old & 0xFFu);
        uint32_t new_pair =
            ((old_curr & 0xFFu) << 8) | (uint32_t(write_idx) & 0xFFu);

        // Publish with release so all data writes become visible before the
        // pair.
        pair.store(new_pair, std::memory_order_release);
    }

    // Optional: push an initial frame (e.g., right after seeding) to avoid size
    // mismatch during first draw.
    void bootstrap_same_as_current(size_t floats_needed, long long stamp_ns) {
        begin_write(floats_needed); // the contents can be left zeros; renderer
                                    // tolerates mismatch/zeros
        publish(stamp_ns);
    }

    // --- Reader API (render thread) ---

    struct ReadView {
        const std::vector<float> *prev = nullptr;
        const std::vector<float> *curr = nullptr;
        long long t0 = 0, t1 = 0;
        uint8_t mask = 0; // which slots are pinned; pass back to end_read
    };

    // Pin the current (prev,curr) so the writer won't reuse them in this frame.
    ReadView begin_read() const {
        for (;;) {
            const uint32_t p1 = pair.load(std::memory_order_acquire);
            const int prev = int((p1 >> 8) & 0xFF);
            const int curr = int((p1 >> 0) & 0xFF);
            const uint8_t want = uint8_t((1u << prev) | (1u << curr));

            uint8_t old = in_use.load(std::memory_order_relaxed);
            if ((old & want) != 0) {
                // Very rare: previous frame not released yet. Spin once; you
                // could Backoff here.
                continue;
            }
            if (in_use.compare_exchange_weak(old, uint8_t(old | want),
                                             std::memory_order_acq_rel,
                                             std::memory_order_relaxed)) {
                ReadView v;
                v.prev = &slots[prev].pos;
                v.curr = &slots[curr].pos;
                v.t0 = slots[prev].stamp_ns.load(std::memory_order_relaxed);
                v.t1 = slots[curr].stamp_ns.load(std::memory_order_relaxed);
                v.mask = want;
                return v;
            }
        }
    }

    // Unpin what we used this frame.
    void end_read(const ReadView &v) const {
        in_use.fetch_and(uint8_t(~v.mask), std::memory_order_release);
    }

    // Convenience: if you don’t interpolate this frame
    const std::vector<float> &read_current_only() const {
        const uint32_t p = pair.load(std::memory_order_acquire);
        const int curr = int(p & 0xFF);
        return slots[curr].pos;
    }
};

#endif