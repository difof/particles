#include <catch_amalgamated.hpp>

#include "simulation/multicore.hpp"
#include "utility/exceptions.hpp"
#include <atomic>

TEST_CASE("SimulationThreadPool parallel_for_n sums correctly", "[multicore]") {
    SimulationThreadPool pool(std::max(1, compute_sim_threads()));
    const int N = 10'000;
    std::atomic<long long> sum{0};

    pool.parallel_for_n(
        [&](int a, int b) {
            long long local = 0;
            for (int i = a; i < b; ++i)
                local += i;
            sum.fetch_add(local, std::memory_order_relaxed);
        },
        N);

    long long expected = 1LL * (N - 1) * N / 2;
    REQUIRE(sum.load() == expected);
}

TEST_CASE("SimulationThreadPool thread count variations", "[multicore]") {
    // Test with different thread counts
    for (int threads : {1, 2, 4}) {
        SimulationThreadPool pool(threads);
        std::atomic<int> counter{0};

        pool.parallel_for_n(
            [&](int start, int end) {
                for (int i = start; i < end; ++i) {
                    counter.fetch_add(1);
                }
            },
            1000);

        REQUIRE(counter.load() == 1000);
    }
}

TEST_CASE("SimulationThreadPool resize behavior", "[multicore]") {
    SimulationThreadPool pool(1);

    // Test resize to different thread counts
    pool.resize(2);
    pool.resize(4);
    pool.resize(1);

    // Should still work after resize
    std::atomic<int> counter{0};
    pool.parallel_for_n(
        [&](int start, int end) {
            counter.fetch_add(end - start);
        },
        100);

    REQUIRE(counter.load() == 100);
}

TEST_CASE("SimulationThreadPool exception safety", "[multicore]") {
    SimulationThreadPool pool(2);

    // Test that exceptions in kernel functions are handled
    bool exception_thrown = false;
    try {
        pool.parallel_for_n(
            [&](int start, int end) {
                if (start == 0) { // Only first thread throws
                    throw std::runtime_error("Test exception");
                }
            },
            100);
    } catch (const std::exception &) {
        exception_thrown = true;
    }

    // Exception should propagate from worker thread
    REQUIRE(exception_thrown);
}

TEST_CASE("SimulationThreadPool concurrent access", "[multicore]") {
    SimulationThreadPool pool(4);
    std::atomic<int> shared_counter{0};

    // Run multiple parallel operations concurrently
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&]() {
            pool.parallel_for_n(
                [&](int start, int end) {
                    for (int j = start; j < end; ++j) {
                        shared_counter.fetch_add(1);
                    }
                },
                100);
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    REQUIRE(shared_counter.load() == 400);
}

TEST_CASE("SimulationThreadPool early termination", "[multicore]") {
    SimulationThreadPool pool(2);
    std::atomic<bool> should_stop{false};
    std::atomic<int> processed{0};

    // Test early termination scenario
    pool.parallel_for_n(
        [&](int start, int end) {
            for (int i = start; i < end && !should_stop.load(); ++i) {
                processed.fetch_add(1);
                if (i > 50) {
                    should_stop.store(true);
                }
            }
        },
        100);

    // Should have processed some items before stopping
    REQUIRE(processed.load() > 0);
    REQUIRE(processed.load() < 100);
}
