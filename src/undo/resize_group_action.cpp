#include "resize_group_action.hpp"

// ResizeGroupAction implementation
ResizeGroupAction::ResizeGroupAction(int group_index, int old_size,
                                     int new_size)
    : m_group_index(group_index), m_old_size(old_size), m_new_size(new_size) {
    // The apply function will be set by the caller
    m_apply_func = []() {
    };
    m_unapply_func = []() {
    };
}

void ResizeGroupAction::apply() { m_apply_func(); }

void ResizeGroupAction::unapply() { m_unapply_func(); }
