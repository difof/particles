#ifndef __MAILBOX_COMMAND_HPP
#define __MAILBOX_COMMAND_HPP

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <raylib.h>
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

// Add/remove groups. (Remove uses group index in [0..G-1])
struct AddGroupCmd {
    int size = 0;
    Color color = WHITE;
    float r2 = 4096.f;
};
struct RemoveGroupCmd {
    int group_index = -1;
};

struct Command {
    enum class Kind {
        SeedWorld, // uses seed
        ResetWorld,
        Quit,
        ApplyRules,  // uses ptr RulePatch
        AddGroup,    // uses add_group
        RemoveGroup, // uses rem_group
        Pause,
        Resume,
        OneStep
    } kind;

    // Generic small numeric payload (kept for future tiny knobs)
    float a = 0.f, b = 0.f, c = 0.f;

    // Large payloads via shared_ptr so queue stays small & movable
    std::shared_ptr<SeedSpec> seed;
    std::shared_ptr<RulePatch> rules;
    std::shared_ptr<AddGroupCmd> add_group;
    std::shared_ptr<RemoveGroupCmd> rem_group;
};

class Queue {
  public:
    void push(const Command &cmd) {
        std::lock_guard<std::mutex> lk(m_);
        q_.push_back(cmd);
    }

    // Called only by the simulation thread; returns and clears current batch
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