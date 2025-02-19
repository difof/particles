#include "add_group_action.hpp"

// AddGroupAction implementation
AddGroupAction::AddGroupAction(int size, Color color, float r2, int group_index)
    : m_size(size), m_color(color), m_r2(r2), m_group_index(group_index) {
    // The apply function will be set by the caller
    m_apply_func = []() {
    };
    m_unapply_func = []() {
    };
}

void AddGroupAction::apply() { m_apply_func(); }

void AddGroupAction::unapply() { m_unapply_func(); }

