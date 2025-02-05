#pragma once

#include <algorithm>
#include <concepts>
#include <condition_variable>
#include <functional>
#include <latch>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "../utility/exceptions.hpp"
#include "../utility/logger.hpp"

using Job = std::function<void()>;

/**
 * @brief Concept for kernel functions that can be parallelized
 * @details A kernel function must accept two integer parameters (start and end)
 * and return void
 */
template <typename F>
concept Kernel = requires(F f, int a, int b) {
    { f(a, b) } -> std::same_as<void>;
};

/**
 * @brief Computes the optimal number of simulation threads
 * @return Number of threads to use for simulation (leaves 1 core for render
 * thread and 1 for OS)
 */
inline int compute_sim_threads() {
    unsigned int num_threads = std::thread::hardware_concurrency();
    if (num_threads <= 2) {
        return 1;
    }

    return int(num_threads) - 2;
}

/**
 * @brief Thread pool for parallel simulation computations
 * @details Manages a pool of worker threads to execute parallel tasks
 * efficiently
 */
class SimulationThreadPool {
  public:
    /**
     * @brief Constructs a new thread pool
     * @param threads Number of threads to create (-1 for automatic detection)
     */
    explicit SimulationThreadPool(int threads = -1);

    /**
     * @brief Destructs the thread pool and stops all workers
     */
    ~SimulationThreadPool();

    SimulationThreadPool(const SimulationThreadPool &) = delete;
    SimulationThreadPool(SimulationThreadPool &&) = delete;
    SimulationThreadPool &operator=(const SimulationThreadPool &) = delete;
    SimulationThreadPool &operator=(SimulationThreadPool &&) = delete;

    /**
     * @brief Resizes the thread pool to use a different number of threads
     * @param threads New number of threads (-1 for automatic detection)
     */
    void resize(int threads);

    /**
     * @brief Executes a kernel function in parallel across multiple threads
     * @param fn Kernel function to execute (must accept start and end
     * parameters)
     * @param n_items Total number of items to process
     */
    template <Kernel F>
    void parallel_for_n(F fn, int n_items) {
        if (n_items <= 0) {
            return;
        }

        int num_threads = std::max(1, static_cast<int>(m_workers.size()));
        if (num_threads == 1 || n_items < 1024) {
            fn(0, n_items);
            return;
        }

        int block = (n_items + num_threads - 1) / num_threads;
        int jobs = (n_items + block - 1) / block;
        std::latch job_latch(jobs);

        for (int thread = 0; thread < jobs; ++thread) {
            int start = thread * block;
            int end_exclusive = std::min(n_items, start + block);

            enqueue([start, end_exclusive, &fn, &job_latch] {
                fn(start, end_exclusive);
                job_latch.count_down();
            });
        }

        job_latch.wait();
    }

  private:
    /**
     * @brief Starts the thread pool with the specified number of threads
     * @param threads Number of threads to start
     * @throws particles::SimulationError if called while already started
     */
    void start(int threads);

    /**
     * @brief Stops the thread pool and joins all worker threads
     * @throws particles::SimulationError if called when not started
     */
    void stop();

    /**
     * @brief Enqueues a job for execution by a worker thread
     * @param f Job function to execute
     */
    void enqueue(Job f);

    /**
     * @brief Worker thread main loop
     */
    void worker_thread();

  private:
    /** @brief Vector of worker threads */
    std::vector<std::thread> m_workers;

    /** @brief Mutex for protecting the task queue */
    std::mutex m_tasks_mutex;

    /** @brief Condition variable for signaling new tasks */
    std::condition_variable m_tasks_signal;

    /** @brief Queue of pending jobs */
    std::queue<Job> m_tasks;

    /** @brief Flag indicating if the thread pool is stopping */
    bool m_stopping = false;
};