#pragma once

#include <atomic>
#include <cstdint>
#include <vector>

#include "types.hpp"

namespace mailbox::render {

static constexpr int N_BUFFERS = 3;

class DrawBuffer {
  public:
    DrawBuffer();
    ~DrawBuffer() = default;

    DrawBuffer(const DrawBuffer &) = delete;
    DrawBuffer &operator=(const DrawBuffer &) = delete;
    DrawBuffer(DrawBuffer &&) = delete;
    DrawBuffer &operator=(DrawBuffer &&) = delete;

    std::vector<float> &begin_write_pos(size_t floats_needed);
    std::vector<float> &begin_write_vel(size_t floats_needed);
    GridFrame &begin_write_grid(int cols, int rows, int N, float cell_size,
                                float width, float height);
    void publish(long long stamp_ns);
    void bootstrap_same_as_current(size_t floats_needed, long long stamp_ns);

    const std::vector<float> &read_current_only() const;

    ReadView begin_read() const;
    void end_read(const ReadView &v) const;

  private:
    int acquire_write_index() const;

  private:
    Slot m_slots[N_BUFFERS];
    std::atomic<uint32_t> m_pair;
    mutable std::atomic<uint8_t> m_in_use;
    int m_write_idx;
};

} // namespace mailbox::render
