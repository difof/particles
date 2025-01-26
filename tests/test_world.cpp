#include <catch_amalgamated.hpp>

#include "simulation/world.hpp"
#include "utility/exceptions.hpp"

TEST_CASE("World add/reset/remove group and rule tables", "[world]") {
    World w;

    // add groups with particles
    int g0 = w.add_group(3, RED);
    int g1 = w.add_group(2, BLUE);
    REQUIRE(g0 == 0);
    REQUIRE(g1 == 1);
    REQUIRE(w.get_groups_size() == 2);
    REQUIRE(w.get_particles_size() == 5);

    // finalize_groups builds particle->group map
    w.finalize_groups();
    REQUIRE(w.group_of(0) == 0);
    REQUIRE(w.group_of(4) == 1);

    // rule tables
    w.init_rule_tables(2);
    w.set_rule(0, 1, 0.5f);
    w.set_r2(0, 9.f);
    REQUIRE(w.rule_val(0, 1) == Catch::Approx(0.5f));
    REQUIRE(w.r2_of(0) == Catch::Approx(9.f));
    REQUIRE(w.max_interaction_radius() == Catch::Approx(3.f));

    // remove a group updates structures
    w.remove_group(0);
    REQUIRE(w.get_groups_size() == 1);
    REQUIRE(w.get_particles_size() == 2);

    // reset clears
    w.reset();
    REQUIRE(w.get_groups_size() == 0);
    REQUIRE(w.get_particles_size() == 0);
}

TEST_CASE("World error handling", "[world]") {
    World w;

    // Test invalid group operations
    REQUIRE_THROWS_AS(w.add_group(-1, RED), particles::SimulationError);
    REQUIRE_THROWS_AS(w.add_group(0, RED), particles::SimulationError);

    // Test invalid rule table initialization
    REQUIRE_THROWS_AS(w.init_rule_tables(-1), particles::SimulationError);

    // Test out-of-bounds access (should be safe)
    w.add_group(5, RED);
    w.finalize_groups();
    w.init_rule_tables(1);

    // These should not throw (bounds checking in getters)
    REQUIRE_NOTHROW(w.get_px(0));
    REQUIRE_NOTHROW(w.get_py(0));
    REQUIRE_NOTHROW(w.get_vx(0));
    REQUIRE_NOTHROW(w.get_vy(0));
    REQUIRE_NOTHROW(w.group_of(0));

    // Test max interaction radius with various configs
    w.set_r2(0, 100.0f);
    REQUIRE(w.max_interaction_radius() == Catch::Approx(10.0f));

    w.set_r2(0, 0.0f);
    REQUIRE(w.max_interaction_radius() == Catch::Approx(0.0f));
}

TEST_CASE("World memory management", "[world]") {
    World w;

    // Add many groups and reset multiple times
    for (int i = 0; i < 5; ++i) {
        w.add_group(100, RED);
        w.add_group(50, BLUE);
        w.finalize_groups();
        w.init_rule_tables(w.get_groups_size());

        REQUIRE(w.get_groups_size() == 2);
        REQUIRE(w.get_particles_size() == 150);

        w.reset();
        REQUIRE(w.get_groups_size() == 0);
        REQUIRE(w.get_particles_size() == 0);
    }
}

TEST_CASE("World rule table edge cases", "[world]") {
    World w;

    // Test with empty rules
    w.add_group(5, RED);
    w.finalize_groups();
    w.init_rule_tables(1);

    // All rules should be zero initially
    REQUIRE(w.rule_val(0, 0) == Catch::Approx(0.0f));

    // Test rule setting and getting
    w.set_rule(0, 0, 1.5f);
    REQUIRE(w.rule_val(0, 0) == Catch::Approx(1.5f));

    // Test radius setting
    w.set_r2(0, 25.0f);
    REQUIRE(w.r2_of(0) == Catch::Approx(25.0f));
}
