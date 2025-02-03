#pragma once

#include <atomic>
#include <cstdint>
#include <vector>

#include "types.hpp"

namespace mailbox::render {

/**
 * @brief Number of buffer slots for triple buffering
 */
static constexpr int N_BUFFERS = 3;

/**
 * @brief Thread-safe triple-buffered draw buffer for particle rendering
 *
 * This class provides a lock-free triple buffering system for particle data
 * that allows concurrent reading and writing between simulation and rendering
 * threads. It manages position data, velocity data, and grid frame information
 * with atomic operations to ensure thread safety.
 */
class DrawBuffer {
  public:
    /**
     * @brief Construct a new DrawBuffer with default initialization
     *
     * Initializes all buffers and atomic variables to their default state.
     * The buffer starts with no active slots and is ready for use.
     */
    DrawBuffer();

    /**
     * @brief Destructor (default)
     */
    ~DrawBuffer() = default;

    // Delete copy and move operations for thread safety
    DrawBuffer(const DrawBuffer &) = delete;
    DrawBuffer &operator=(const DrawBuffer &) = delete;
    DrawBuffer(DrawBuffer &&) = delete;
    DrawBuffer &operator=(DrawBuffer &&) = delete;

    /**
     * @brief Begin writing position data to the buffer
     * @param floats_needed Number of float values needed for position data
     * @return Reference to the position vector for writing
     *
     * This method acquires a write slot and prepares the position vector
     * for writing. The vector will be resized if necessary to accommodate
     * the required number of floats.
     */
    std::vector<float> &begin_write_pos(size_t floats_needed);

    /**
     * @brief Begin writing velocity data to the buffer
     * @param floats_needed Number of float values needed for velocity data
     * @return Reference to the velocity vector for writing
     *
     * This method prepares the velocity vector for writing in the currently
     * acquired write slot. The vector will be resized if necessary.
     */
    std::vector<float> &begin_write_vel(size_t floats_needed);

    /**
     * @brief Begin writing grid frame data to the buffer
     * @param cols Number of columns in the grid
     * @param rows Number of rows in the grid
     * @param N Total number of particles
     * @param cell_size Size of each grid cell
     * @param width Total width of the grid
     * @param height Total height of the grid
     * @return Reference to the GridFrame for writing
     *
     * This method initializes and prepares the grid frame data structure
     * for writing. The grid will be resized and cleared for new data.
     */
    GridFrame &begin_write_grid(int cols, int rows, int N, float cell_size,
                                float width, float height);

    /**
     * @brief Publish the current write buffer and make it available for reading
     * @param stamp_ns Timestamp in nanoseconds when the data was written
     *
     * This method atomically updates the buffer pair to make the current
     * write buffer available for reading by other threads. The timestamp
     * is stored for interpolation purposes.
     */
    void publish(long long stamp_ns);

    /**
     * @brief Bootstrap the buffer with the same data as current
     * @param floats_needed Number of float values needed for position data
     * @param stamp_ns Timestamp in nanoseconds
     *
     * This method initializes the buffer with the same data as the current
     * read buffer, useful for initialization or reset scenarios.
     */
    void bootstrap_same_as_current(size_t floats_needed, long long stamp_ns);

    /**
     * @brief Read the current position data without acquiring a read lock
     * @return Const reference to the current position vector
     *
     * This method provides quick access to the current position data
     * without the overhead of acquiring a full read view. Use with caution
     * as the data may change during reading.
     */
    const std::vector<float> &read_current_only() const;

    /**
     * @brief Begin reading from the buffer with thread safety
     * @return ReadView containing references to current and previous data
     *
     * This method acquires a read lock and returns a view containing
     * both current and previous frame data for interpolation. The caller
     * must call end_read() when finished to release the lock.
     */
    ReadView begin_read() const;

    /**
     * @brief End reading and release the read lock
     * @param v The ReadView returned by begin_read()
     *
     * This method releases the read lock acquired by begin_read().
     * Must be called for each successful begin_read() call to prevent
     * deadlocks and resource leaks.
     */
    void end_read(const ReadView &v) const;

  private:
    /**
     * @brief Acquire an available write index
     * @return Index of the slot to use for writing
     *
     * This method finds an available buffer slot that is not currently
     * being read from or used as the current/previous frame. It uses
     * atomic operations to ensure thread safety.
     */
    int acquire_write_index() const;

  private:
    /**
     * @brief Array of buffer slots for triple buffering
     *
     * Contains N_BUFFERS slots, each holding position data, velocity data,
     * grid frame data, and a timestamp.
     */
    Slot m_slots[N_BUFFERS];

    /**
     * @brief Atomic pair tracking current and previous buffer indices
     *
     * Packed representation of (previous_index, current_index) using
     * the lower 8 bits for current and upper 8 bits for previous.
     */
    std::atomic<uint32_t> m_pair;

    /**
     * @brief Atomic bitmask tracking which buffers are currently in use
     *
     * Each bit represents whether the corresponding buffer slot is
     * currently being read from. Used to prevent writing to buffers
     * that are being read.
     */
    mutable std::atomic<uint8_t> m_in_use;

    /**
     * @brief Index of the currently acquired write slot
     *
     * Tracks which buffer slot is currently being written to.
     * Set by begin_write_pos() and used by other write methods.
     */
    int m_write_idx;
};

} // namespace mailbox::render
