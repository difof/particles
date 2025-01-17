#ifndef __MAILBOX_COMMAND_HPP
#define __MAILBOX_COMMAND_HPP

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <raylib.h>
#include <variant>
#include <vector>

namespace mailbox::command {

// World seeding specification
struct SeedSpec {
    // Particle groups sizes and colors; G is sizes.size()
    std::vector<int> sizes;
    std::vector<Color> colors;
    // Per-group radii squared; optional, defaults applied if empty
    std::vector<float> r2;
    // Full rule matrix row-major (G*G). Optional; if empty defaults used
    std::vector<float> rules;
};

// A full rules/radii snapshot to apply. Hot if G same, else sim will require
// reseed.
struct RulePatch {
    int groups = 0;           // G
    std::vector<float> r2;    // size G, r^2 per group
    std::vector<float> rules; // size G*G, row-major: rules[i*G + j]
    std::vector<Color> colors;
    bool hot = true; // try hot-apply without reseed
};

struct SeedWorld {
    std::shared_ptr<SeedSpec> seed; // null means clear world
};

struct ResetWorld {};

struct Quit {};

struct ApplyRules {
    std::shared_ptr<RulePatch> patch;
};

struct AddGroup {
    int size = 0;
    Color color = WHITE;
    float r2 = 4096.f;
};

struct RemoveGroup {
    int group_index = -1;
};

struct Pause {};

struct Resume {};

struct OneStep {};

using Command = std::variant<SeedWorld, ResetWorld, Quit, ApplyRules, AddGroup,
                             RemoveGroup, Pause, Resume, OneStep>;

class QueueV {
  public:
    void push(const Command &cmd) {
        std::lock_guard<std::mutex> lk(m_);
        q_.push_back(cmd);
    }

    std::vector<Command> drain() {
        std::vector<Command> out;
        std::lock_guard<std::mutex> lk(m_);
        out.swap(q_);
        return out;
    }

  private:
    std::mutex m_;
    std::vector<Command> q_;
};
} // namespace mailbox::command

#endif