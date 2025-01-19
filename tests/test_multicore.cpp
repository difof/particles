#include <catch_amalgamated.hpp>

#include "simulation/multicore.hpp"
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
