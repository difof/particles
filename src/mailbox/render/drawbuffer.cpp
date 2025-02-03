#include <algorithm>

#include "drawbuffer.hpp"

static inline uint32_t pack_pair(uint32_t prev, uint32_t curr) {
    return ((prev & 0xFFu) << 8) | (curr & 0xFFu);
}

static inline int unpack_prev(uint32_t pair) { return int((pair >> 8) & 0xFF); }

static inline int unpack_curr(uint32_t pair) { return int((pair >> 0) & 0xFF); }

static inline uint8_t bit(int i) { return uint8_t(1u << i); }

namespace mailbox::render {

DrawBuffer::DrawBuffer()
    : m_pair(pack_pair(0u, 0u)), m_in_use(0), m_write_idx(0) {}

int DrawBuffer::acquire_write_index() const {
    const uint32_t p = m_pair.load(std::memory_order_acquire);
    const int prev = unpack_prev(p);
    const int curr = unpack_curr(p);
    const uint8_t pinned = m_in_use.load(std::memory_order_acquire);

    for (int i = 0; i < N_BUFFERS; ++i) {
        if (i != prev && i != curr && (pinned & bit(i)) == 0) {
            return i;
        }
    }

    for (int i = 0; i < N_BUFFERS; ++i) {
        if (i != curr) {
            return i;
        }
    }

    return 0;
}

std::vector<float> &DrawBuffer::begin_write_pos(size_t floats_needed) {
    m_write_idx = acquire_write_index();
    auto &v = m_slots[m_write_idx].pos;

    if (v.size() != floats_needed) {
        v.resize(floats_needed);
    }

    return v;
}

std::vector<float> &DrawBuffer::begin_write_vel(size_t floats_needed) {
    auto &v = m_slots[m_write_idx].vel;

    if (v.size() != floats_needed) {
        v.resize(floats_needed);
    }

    return v;
}

GridFrame &DrawBuffer::begin_write_grid(int cols, int rows, int N,
                                        float cell_size, float width,
                                        float height) {
    auto &g = m_slots[m_write_idx].grid;
    g.cell = cell_size;
    g.width = width;
    g.height = height;

    g.resize(cols, rows, N);
    g.clear_accum();

    return g;
}

void DrawBuffer::publish(long long stamp_ns) {
    m_slots[m_write_idx].stamp_ns.store(stamp_ns, std::memory_order_relaxed);
    const uint32_t old = m_pair.load(std::memory_order_relaxed);
    const uint32_t old_curr = uint32_t(unpack_curr(old));
    const uint32_t new_pair = pack_pair(old_curr, uint32_t(m_write_idx));
    m_pair.store(new_pair, std::memory_order_release);
}

void DrawBuffer::bootstrap_same_as_current(size_t floats_needed,
                                           long long stamp_ns) {
    begin_write_pos(floats_needed);
    publish(stamp_ns);
}

const std::vector<float> &DrawBuffer::read_current_only() const {
    const uint32_t p = m_pair.load(std::memory_order_acquire);
    const int curr = unpack_curr(p);

    return m_slots[curr].pos;
}

ReadView DrawBuffer::begin_read() const {
    for (;;) {
        const uint32_t p = m_pair.load(std::memory_order_acquire);
        const int prev = unpack_prev(p);
        const int curr = unpack_curr(p);

        const uint8_t want = uint8_t(bit(prev) | bit(curr));
        uint8_t old = m_in_use.load(std::memory_order_relaxed);

        if ((old & want) != 0) {
            continue;
        }

        if (m_in_use.compare_exchange_weak(old, uint8_t(old | want),
                                           std::memory_order_acq_rel,
                                           std::memory_order_relaxed)) {
            ReadView v;
            v.prev = &m_slots[prev].pos;
            v.curr = &m_slots[curr].pos;
            v.curr_vel = &m_slots[curr].vel;
            v.grid = &m_slots[curr].grid;
            v.t0 = m_slots[prev].stamp_ns.load(std::memory_order_relaxed);
            v.t1 = m_slots[curr].stamp_ns.load(std::memory_order_relaxed);
            v.mask = want;

            return v;
        }
    }
}

void DrawBuffer::end_read(const ReadView &v) const {
    m_in_use.fetch_and(uint8_t(~v.mask), std::memory_order_release);
}

} // namespace mailbox::render
