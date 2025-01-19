#include <catch_amalgamated.hpp>

#include "simulation/world.hpp"

TEST_CASE("World add/reset/remove group and rule tables", "[world]") {
    World w;

    // add groups with particles
    int g0 = w.add_group(3, RED);
    int g1 = w.add_group(2, BLUE);
    REQUIRE(g0 == 0);
    REQUIRE(g1 == 1);
    REQUIRE(w.get_groups_size() == 2);
    REQUIRE(w.get_particles_count() == 5);

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
    REQUIRE(w.get_particles_count() == 2);

    // reset clears
    w.reset();
    REQUIRE(w.get_groups_size() == 0);
    REQUIRE(w.get_particles_count() == 0);
}
