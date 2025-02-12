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
    seed->sizes = std::vector<int>(groups, 1500);
    seed->colors = {(Color){0, 228, 114, 255}, (Color){238, 70, 82, 255},
                    (Color){227, 172, 72, 255}, (Color){0, 121, 241, 255},
                    (Color){200, 122, 255, 255}};
    seed->r2 = {80.f * 80.f, 80.f * 80.f, 96.6f * 96.6f, 80.f * 80.f,
                80.f * 80.f};
    seed->enabled = {true, true, true, true,
                     true}; // All groups enabled by default
    seed->rules = {
        // row 0
        +0.926f,
        -0.834f,
        +0.281f,
        -0.06427308f,
        +0.51738745f,
        // row 1
        -0.46170965f,
        +0.49142435f,
        +0.2760726f,
        +0.6413487f,
        -0.7276546f,
        // row 2
        -0.78747644f,
        +0.23373386f,
        -0.024112331f,
        -0.74875921f,
        +0.22836663f,
        // row 3
        +0.56558144f,
        +0.94846946f,
        -0.36052886f,
        +0.44114092f,
        -0.31766385f,
        // row 4
        std::sin(1.0f),
        std::cos(2.0f),
        +1.0f,
        -1.0f,
        +3.14f,
    };
    return seed;
}

} // namespace particles::utility
