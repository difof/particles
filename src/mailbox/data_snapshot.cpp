#include "data_snapshot.hpp"

namespace mailbox {

int WorldSnapshot::get_groups_size() const noexcept { return group_count; }

int WorldSnapshot::get_particles_size() const noexcept {
    return particles_count;
}

int WorldSnapshot::get_group_start(int group_index) const noexcept {
    if (group_index < 0 || (size_t)group_index >= group_ranges.size() / 2)
        return 0;
    return group_ranges[group_index * 2 + 0];
}

int WorldSnapshot::get_group_end(int group_index) const noexcept {
    if (group_index < 0 || (size_t)group_index >= group_ranges.size() / 2)
        return 0;
    return group_ranges[group_index * 2 + 1];
}

int WorldSnapshot::get_group_size(int group_index) const noexcept {
    return get_group_end(group_index) - get_group_start(group_index);
}

Color WorldSnapshot::get_group_color(int group_index) const noexcept {
    if (group_index < 0 || (size_t)group_index >= group_colors.size())
        return WHITE;
    return group_colors[group_index];
}

float WorldSnapshot::r2_of(int group_index) const noexcept {
    if (group_index < 0 || (size_t)group_index >= group_radii2.size())
        return 0.f;
    return group_radii2[group_index];
}

bool WorldSnapshot::is_group_enabled(int group_index) const noexcept {
    if (group_index < 0 || (size_t)group_index >= group_enabled.size())
        return true; // default to enabled
    return group_enabled[group_index];
}

float WorldSnapshot::rule_val(int source_group, int destination_group) const {
    if (source_group < 0 || destination_group < 0 ||
        source_group >= group_count || destination_group >= group_count)
        return 0.0f;

    size_t index =
        (size_t)source_group * (size_t)group_count + (size_t)destination_group;
    if (index >= rules.size())
        return 0.0f;

    return rules[index];
}

WorldSnapshotRuleRowView
WorldSnapshot::rules_of(int source_group) const noexcept {
    if (source_group < 0 || source_group >= group_count ||
        rules.size() < (size_t)group_count * (size_t)group_count)
        return {nullptr, group_count};

    return {&rules[source_group * group_count], group_count};
}

int WorldSnapshot::group_of(int particle_index) const noexcept {
    if (particle_index < 0 || (size_t)particle_index >= particle_groups.size())
        return 0;
    return particle_groups[particle_index];
}

} // namespace mailbox
