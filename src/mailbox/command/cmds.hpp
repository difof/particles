#pragma once

#include <memory>
#include <vector>

#include "cmd_seedspec.hpp"

namespace mailbox::command {

// A full rules/radii snapshot to apply. Hot-reloadable if G same, else sim will
// require reseed.
struct RulePatch {
    int groups = 0;
    std::vector<float> r2;    // size G, r^2 per group
    std::vector<float> rules; // size G*G, row-major: rules[i*G + j]
    std::vector<Color> colors;
    std::vector<bool> enabled;
    bool hot = true; // try hot-apply without reseed
};

struct ApplyRules {
    std::shared_ptr<RulePatch> patch;
};

struct SeedWorld {
    std::shared_ptr<SeedSpec> seed; // null means clear world
};

struct ResetWorld {};

struct Quit {};

struct AddGroup {
    int size = 0;
    Color color = WHITE;
    float r2 = 4096.f;
};

struct RemoveGroup {
    int group_index = -1;
};

struct RemoveAllGroups {};

struct ResizeGroup {
    int group_index = -1;
    int new_size = 0;
};

struct Pause {};

struct Resume {};

struct OneStep {};

} // namespace mailbox::command