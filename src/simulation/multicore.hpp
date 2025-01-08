#ifndef __MULTICORE_HPP
#define __MULTICORE_HPP

#include <algorithm>
#include <concepts>
#include <condition_variable>
#include <functional>
#include <latch>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

using Job = std::function<void()>;

template <typename F>
concept Kernel = requires(F f, int a, int b) {
    { f(a, b) } -> std::same_as<void>;
};

// Leave 1 core for render thread and 1 for OS
inline int compute_sim_threads() {
    unsigned int num_threads = std::thread::hardware_concurrency();
    if (num_threads <= 2) {
        return 1;
    }

    return int(num_threads) - 2;
}

class SimulationThreadPool {
  public:
    explicit SimulationThreadPool(int threads = -1);
    ~SimulationThreadPool();

    SimulationThreadPool(const SimulationThreadPool &) = delete;
    SimulationThreadPool(SimulationThreadPool &&) = delete;
    SimulationThreadPool &operator=(const SimulationThreadPool &) = delete;
    SimulationThreadPool &operator=(SimulationThreadPool &&) = delete;

    void resize(int threads);

    template <Kernel F>
    void parallel_for_n(int n_items, F fn) {
        if (n_items <= 0)
            return;

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
    void start(
        int threads); // Throws std::logic_error if called while already started
    void stop();      // Throws std::logic_error if called when not started

    void enqueue(Job f);
    void worker_thread();

  private:
    std::vector<std::thread> m_workers;
    std::mutex m_tasks_mutex;
    std::condition_variable m_tasks_signal;
    std::queue<Job> m_tasks;
    bool m_stopping = false;
};

#endif