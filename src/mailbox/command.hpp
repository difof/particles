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
    // Per-group enable/disable state; optional, defaults to enabled if empty
    std::vector<bool> enabled;

    // Helpers for consumers working per group
    int group_count() const { return static_cast<int>(sizes.size()); }

    void ensure_defaults() {
        const int G = group_count();
        if (static_cast<int>(colors.size()) != G)
            colors.resize(G, WHITE);
        if (static_cast<int>(r2.size()) != G)
            r2.resize(G, 4096.f);
        if (static_cast<int>(enabled.size()) != G)
            enabled.resize(G, true);
        if (static_cast<int>(rules.size()) != G * G)
            rules.resize(G * G, 0.0f);
    }

    float get_rule(int src_group, int dst_group) const {
        const int G = group_count();
        if (G == 0)
            return 0.0f;
        const int i = src_group * G + dst_group;
        if (i < 0 || i >= static_cast<int>(rules.size()))
            return 0.0f;
        return rules[i];
    }

    void set_rule(int src_group, int dst_group, float value) {
        const int G = group_count();
        if (G <= 0)
            return;
        const int i = src_group * G + dst_group;
        if (i < 0)
            return;
        if (static_cast<int>(rules.size()) < G * G)
            rules.resize(G * G, 0.0f);
        if (i < static_cast<int>(rules.size()))
            rules[i] = value;
    }

    void resize_groups(int new_count) {
        if (new_count < 0)
            new_count = 0;
        sizes.resize(new_count, 0);
        colors.resize(new_count, WHITE);
        r2.resize(new_count, 4096.f);
        enabled.resize(new_count, true);
        rules.resize(new_count * new_count, 0.0f);
    }

    void set_group(int index, int size_val, Color color_val, float r2_val,
                   bool enabled_val) {
        const int G = group_count();
        if (index < 0)
            return;
        if (index >= G)
            resize_groups(index + 1);
        sizes[index] = size_val;
        colors[index] = color_val;
        r2[index] = r2_val;
        enabled[index] = enabled_val;
    }

    void add_group(int size_val, Color color_val, float r2_val,
                   bool enabled_val) {
        const int idx = group_count();
        resize_groups(idx + 1);
        set_group(idx, size_val, color_val, r2_val, enabled_val);
    }

    void remove_group(int index) {
        const int G = group_count();
        if (index < 0 || index >= G)
            return;
        sizes.erase(sizes.begin() + index);
        colors.erase(colors.begin() + index);
        r2.erase(r2.begin() + index);
        enabled.erase(enabled.begin() + index);
        // Rebuild rules matrix without the row/column
        const int newG = G - 1;
        std::vector<float> new_rules(newG * newG, 0.0f);
        for (int i = 0, ni = 0; i < G; ++i) {
            if (i == index)
                continue;
            for (int j = 0, nj = 0; j < G; ++j) {
                if (j == index)
                    continue;
                new_rules[ni * newG + nj] = rules[i * G + j];
                ++nj;
            }
            ++ni;
        }
        rules.swap(new_rules);
    }
};

// A full rules/radii snapshot to apply. Hot if G same, else sim will require
// reseed.
struct RulePatch {
    int groups = 0;           // G
    std::vector<float> r2;    // size G, r^2 per group
    std::vector<float> rules; // size G*G, row-major: rules[i*G + j]
    std::vector<Color> colors;
    std::vector<bool> enabled; // size G, enable/disable per group
    bool hot = true;           // try hot-apply without reseed
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

struct RemoveAllGroups {};

struct ResizeGroup {
    int group_index = -1;
    int new_size = 0;
};

struct Pause {};

struct Resume {};

struct OneStep {};

using Command =
    std::variant<SeedWorld, ResetWorld, Quit, ApplyRules, AddGroup, RemoveGroup,
                 RemoveAllGroups, ResizeGroup, Pause, Resume, OneStep>;

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