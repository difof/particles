#ifndef __MULTICORE_HPP
#define __MULTICORE_HPP

#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>

// Leave 1 core for render thread and 1 for OS
inline int compute_sim_threads()
{
    unsigned hc = std::thread::hardware_concurrency();
    if (hc <= 2)
        return 1;
    return int(hc) - 2;
}

struct Latch
{
    std::mutex m;
    std::condition_variable cv;
    int count = 0;
    void add(int n)
    {
        std::lock_guard<std::mutex> lock(m);
        count += n;
    }
    void count_down()
    {
        std::lock_guard<std::mutex> lock(m);
        if (--count == 0)
            cv.notify_all();
    }
    void wait()
    {
        std::unique_lock<std::mutex> lock(m);
        cv.wait(lock, [&]
                { return count == 0; });
    }
};

class ThreadPool
{
public:
    explicit ThreadPool(int threads = -1) { start(threads); }
    ~ThreadPool() { stop(); }

    void resize(int threads)
    {
        stop();
        start(threads);
    }

    // enqueue a job
    void enqueue(std::function<void()> f)
    {
        {
            std::lock_guard<std::mutex> lock(q_m);
            tasks.push(std::move(f));
        }
        q_cv.notify_one();
    }

    // parallel_for using this pool
    template <typename F>
    void parallel_for_n(int N, F fn)
    {
        if (N <= 0)
            return;
        int T = std::max(1, (int)workers.size());
        if (T == 1 || N < 1024)
        {
            fn(0, N);
            return;
        }

        int block = (N + T - 1) / T;
        int jobs = (N + block - 1) / block;
        Latch latch;
        latch.add(jobs);

        for (int t = 0; t < jobs; ++t)
        {
            int s = t * block;
            int e = std::min(N, s + block);
            enqueue([s, e, &fn, &latch]()
                    {
                fn(s,e);
                latch.count_down(); });
        }
        latch.wait();
    }

private:
    std::vector<std::thread> workers;
    std::mutex q_m;
    std::condition_variable q_cv;
    std::queue<std::function<void()>> tasks;
    bool stopping = false;

    void start(int threads)
    {
        int T = threads <= 0 ? compute_sim_threads() : threads;
        T = std::max(1, T);
        stopping = false;
        workers.reserve(T);
        for (int i = 0; i < T; ++i)
        {
            workers.emplace_back([this]()
                                 {
                for (;;) {
                    std::function<void()> job;
                    {
                        std::unique_lock<std::mutex> lock(q_m);
                        q_cv.wait(lock, [this]{ return stopping || !tasks.empty(); });
                        if (stopping && tasks.empty()) return;
                        job = std::move(tasks.front());
                        tasks.pop();
                    }
                    job();
                } });
        }
    }

    void stop()
    {
        {
            std::lock_guard<std::mutex> lock(q_m);
            stopping = true;
        }
        q_cv.notify_all();
        for (auto &t : workers)
            if (t.joinable())
                t.join();
        workers.clear();
        // drain any leftover (should be none)
        std::queue<std::function<void()>> empty;
        std::swap(tasks, empty);
    }
};

#endif