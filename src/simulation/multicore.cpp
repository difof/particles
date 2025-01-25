#include "multicore.hpp"
#include <stdexcept>

SimulationThreadPool::SimulationThreadPool(int threads) {
    LOG_DEBUG("Creating thread pool with " + std::to_string(threads) +
              " threads");
    start(threads);
}
SimulationThreadPool::~SimulationThreadPool() {
    LOG_DEBUG("Destroying thread pool");
    stop();
}

void SimulationThreadPool::resize(int threads) {
    stop();
    start(threads);
}

void SimulationThreadPool::enqueue(Job f) {
    {
        std::lock_guard<std::mutex> lock(m_tasks_mutex);
        m_tasks.push(std::move(f));
    }

    m_tasks_signal.notify_one();
}

void SimulationThreadPool::start(int threads) {
    if (!m_workers.empty()) {
        throw particles::SimulationError("Thread pool already started");
    }

    int num_threads = threads <= 0 ? compute_sim_threads() : threads;
    num_threads = std::max(1, num_threads);

    if (num_threads < 1) {
        throw particles::SimulationError("Invalid thread count: " +
                                         std::to_string(num_threads));
    }

    LOG_DEBUG("Starting thread pool with " + std::to_string(num_threads) +
              " threads");
    m_stopping = false;
    m_workers.reserve(num_threads);

    for (int i = 0; i < num_threads; ++i) {
        m_workers.emplace_back(&SimulationThreadPool::worker_thread, this);
    }
}

void SimulationThreadPool::stop() {
    if (m_workers.empty()) {
        throw std::logic_error(
            "SimulationThreadPool::stop() called when not started");
    }

    {
        std::lock_guard<std::mutex> lock(m_tasks_mutex);
        m_stopping = true;
    }

    m_tasks_signal.notify_all();

    for (auto &t : m_workers) {
        if (t.joinable()) {
            t.join();
        }
    }

    m_workers.clear();

    std::queue<Job> empty;
    std::swap(m_tasks, empty);
}

void SimulationThreadPool::worker_thread() {
    for (;;) {
        Job job;
        {
            std::unique_lock<std::mutex> lock(m_tasks_mutex);

            m_tasks_signal.wait(lock, [this] {
                return m_stopping || !m_tasks.empty();
            });

            if (m_stopping && m_tasks.empty()) {
                return;
            }

            job = std::move(m_tasks.front());
            m_tasks.pop();
        }
        job();
    }
}