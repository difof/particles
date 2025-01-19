#include <catch_amalgamated.hpp>

#include "mailbox/command.hpp"
#include "mailbox/drawbuffer.hpp"
#include "mailbox/simconfig.hpp"
#include "mailbox/simstats.hpp"

using namespace mailbox;

TEST_CASE("SimulationConfig publish/acquire", "[mailboxes]") {
    SimulationConfig cfg;
    SimulationConfig::Snapshot s{};
    s.bounds_width = 100;
    s.bounds_height = 50;
    s.time_scale = 2.f;
    s.sim_threads = 3;
    s.draw_report.grid_data = true;
    cfg.publish(s);
    auto out = cfg.acquire();
    REQUIRE(out.bounds_width == Catch::Approx(100));
    REQUIRE(out.bounds_height == Catch::Approx(50));
    REQUIRE(out.time_scale == Catch::Approx(2.f));
    REQUIRE(out.sim_threads == 3);
    REQUIRE(out.draw_report.grid_data == true);
}

TEST_CASE("SimulationStats publish/acquire", "[mailboxes]") {
    SimulationStats stats;
    SimulationStats::Snapshot s{};
    s.effective_tps = 60;
    s.particles = 100;
    s.groups = 2;
    s.sim_threads = 4;
    s.last_step_ns = 1000;
    s.published_ns = 2000;
    s.num_steps = 42;
    stats.publish(s);
    auto out = stats.acquire();
    REQUIRE(out.effective_tps == 60);
    REQUIRE(out.particles == 100);
    REQUIRE(out.groups == 2);
    REQUIRE(out.sim_threads == 4);
    REQUIRE(out.last_step_ns == 1000);
    REQUIRE(out.published_ns == 2000);
    REQUIRE(out.num_steps == 42);
}

TEST_CASE("DrawBuffer basic write/read", "[mailboxes]") {
    DrawBuffer db;
    auto &pos = db.begin_write_pos(8);
    auto &vel = db.begin_write_vel(8);
    auto &g = db.begin_write_grid(2, 2, 2, 4.f, 8.f, 8.f);
    pos[0] = 1.f;
    pos[1] = 2.f;
    pos[2] = 3.f;
    pos[3] = 4.f;
    vel[0] = 0.1f;
    vel[1] = 0.2f;
    vel[2] = 0.3f;
    vel[3] = 0.4f;
    g.head[0] = 0;
    g.next[0] = 1;
    db.publish(123);

    auto v = db.begin_read();
    REQUIRE(v.curr != nullptr);
    REQUIRE(v.curr_vel != nullptr);
    REQUIRE(v.grid != nullptr);
    REQUIRE(v.t1 == 123);
    db.end_read(v);
}

TEST_CASE("Command queue push/drain", "[mailboxes]") {
    mailbox::command::QueueV q;
    q.push(mailbox::command::Pause{});
    q.push(mailbox::command::Resume{});
    auto cmds = q.drain();
    REQUIRE(cmds.size() == 2);
    REQUIRE(std::holds_alternative<mailbox::command::Pause>(cmds[0]));
    REQUIRE(std::holds_alternative<mailbox::command::Resume>(cmds[1]));
}
