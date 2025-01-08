#ifndef __MAILBOX_DRAWWBUFFER_HPP
#define __MAILBOX_DRAWWBUFFER_HPP

#include <atomic>
#include <cstdint>
#include <vector>

namespace mailbox {
struct DrawBuffer {
    // ---- Implementation constants & helpers (internal) ----
    static constexpr int kNumSlots = 3;

    // Packed (prev<<8)|curr published by the sim (writer).
    // Helpers keep the packing logic in one place.
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

    struct Slot {
        // [i*2+0] = px, [i*2+1] = py
        std::vector<float> pos;
        // Monotonic publish time of this slot
        std::atomic<long long> stamp_ns{0};
    };

    // ---- Storage ----
    Slot slots[kNumSlots];

    // Packed (prev<<8)|curr; writer stores with release; reader loads with
    // acquire
    std::atomic<uint32_t> pair{pack_pair(0u, 0u)};

    // Renderer-pinned slots (bitmask: bit i => slot i in use)
    mutable std::atomic<uint8_t> in_use{0};

    // Writer-only state
    int write_idx = 0; // set by begin_write

    // ---- Writer API (simulation thread) ----

    // Choose a free slot that is neither prev nor curr nor currently pinned
    int acquire_write_index() const {
        const uint32_t p = pair.load(std::memory_order_acquire);
        const int prev = unpack_prev(p);
        const int curr = unpack_curr(p);
        const uint8_t pinned = in_use.load(std::memory_order_acquire);

        // First try: any slot not prev/curr and not pinned
        for (int i = 0; i < kNumSlots; ++i) {
            if (i != prev && i != curr && (pinned & bit(i)) == 0)
                return i;
        }
        // Fallback (extremely unlikely): pick anything that's not curr.
        for (int i = 0; i < kNumSlots; ++i) {
            if (i != curr)
                return i;
        }
        // Should never happen, but keep a defined return
        return 0;
    }

    // Returns a writable vector for the sim step (size = 2*N floats)
    std::vector<float> &begin_write(size_t floats_needed) {
        write_idx = acquire_write_index();
        auto &v = slots[write_idx].pos;

        if (v.size() != floats_needed) {
            v.resize(floats_needed);
        }
        // zero-fill (renderer tolerates zeros; keeps semantics identical)
        std::fill(v.begin(), v.end(), 0.0f);

        return v;
    }

    // After filling the buffer, publish it as the new 'curr'. 'prev' becomes
    // old 'curr'.
    void publish(long long stamp_ns) {
        slots[write_idx].stamp_ns.store(stamp_ns, std::memory_order_relaxed);

        // Roll the pair forward: prev = old curr, curr = write_idx
        const uint32_t old = pair.load(std::memory_order_relaxed);
        const uint32_t old_curr = uint32_t(unpack_curr(old));
        const uint32_t new_pair = pack_pair(old_curr, uint32_t(write_idx));

        // Release: all data writes to the slot are visible before readers see
        // the new pair
        pair.store(new_pair, std::memory_order_release);
    }

    // Optional: prime an initial frame so sizes match on first draw
    void bootstrap_same_as_current(size_t floats_needed, long long stamp_ns) {
        begin_write(floats_needed); // contents can be zeros; renderer tolerates
        publish(stamp_ns);
    }

    // ---- Reader API (render thread) ----

    struct ReadView {
        const std::vector<float> *prev = nullptr;
        const std::vector<float> *curr = nullptr;
        long long t0 = 0, t1 = 0;
        uint8_t mask = 0; // which slots are pinned; pass to end_read
    };

    // Pin the current (prev,curr) so the writer won't reuse them in this frame.
    ReadView begin_read() const {
        for (;;) {
            const uint32_t p = pair.load(std::memory_order_acquire);
            const int prev = unpack_prev(p);
            const int curr = unpack_curr(p);

            const uint8_t want = uint8_t(bit(prev) | bit(curr));
            uint8_t old = in_use.load(std::memory_order_relaxed);

            // If anything we need is still pinned, spin (rare). A backoff could
            // be added here.
            if ((old & want) != 0) {
                continue;
            }

            // Try to pin both slots
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
        // Clear only the bits we set
        in_use.fetch_and(uint8_t(~v.mask), std::memory_order_release);
    }

    // Convenience: if you don’t interpolate this frame
    const std::vector<float> &read_current_only() const {
        const uint32_t p = pair.load(std::memory_order_acquire);
        const int curr = unpack_curr(p);
        return slots[curr].pos;
    }
};
} // namespace mailbox

#endif
