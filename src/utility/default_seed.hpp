#pragma once

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include <raylib.h>

#include "../utility/exceptions.hpp"
#include "../utility/logger.hpp"
#include "mailbox/command/cmd_seedspec.hpp"

namespace particles::utility {

/**
 * @brief Creates the default seed specification used throughout the application
 * @return Shared pointer to the default seed specification
 */
inline std::shared_ptr<mailbox::command::SeedSpec> create_default_seed() {
    auto seed = std::make_shared<mailbox::command::SeedSpec>();
    const int groups = 5;

    seed->resize_groups(groups);

    seed->set_group(0, 1500, (Color){0, 228, 114, 255}, 80.f * 80.f, true);
    seed->set_group(1, 1500, (Color){238, 70, 82, 255}, 80.f * 80.f, true);
    seed->set_group(2, 1500, (Color){227, 172, 72, 255}, 96.6f * 96.6f, true);
    seed->set_group(3, 1500, (Color){0, 121, 241, 255}, 80.f * 80.f, true);
    seed->set_group(4, 1500, (Color){200, 122, 255, 255}, 80.f * 80.f, true);

    // row 0
    seed->set_rule(0, 0, +0.926f);
    seed->set_rule(0, 1, -0.834f);
    seed->set_rule(0, 2, +0.281f);
    seed->set_rule(0, 3, -0.06427308f);
    seed->set_rule(0, 4, +0.51738745f);

    // row 1
    seed->set_rule(1, 0, -0.46170965f);
    seed->set_rule(1, 1, +0.49142435f);
    seed->set_rule(1, 2, +0.2760726f);
    seed->set_rule(1, 3, +0.6413487f);
    seed->set_rule(1, 4, -0.7276546f);

    // row 2
    seed->set_rule(2, 0, -0.78747644f);
    seed->set_rule(2, 1, +0.23373386f);
    seed->set_rule(2, 2, -0.024112331f);
    seed->set_rule(2, 3, -0.74875921f);
    seed->set_rule(2, 4, +0.22836663f);

    // row 3
    seed->set_rule(3, 0, +0.56558144f);
    seed->set_rule(3, 1, +0.94846946f);
    seed->set_rule(3, 2, -0.36052886f);
    seed->set_rule(3, 3, +0.44114092f);
    seed->set_rule(3, 4, -0.31766385f);

    // row 4
    seed->set_rule(4, 0, std::sin(1.0f));
    seed->set_rule(4, 1, std::cos(2.0f));
    seed->set_rule(4, 2, +1.0f);
    seed->set_rule(4, 3, -1.0f);
    seed->set_rule(4, 4, +3.14f);

    return seed;
}

} // namespace particles::utility
