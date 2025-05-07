#include "remove_group_action.hpp"

// RemoveGroupAction implementation
RemoveGroupAction::RemoveGroupAction(int group_index,
                                     mailbox::command::SeedSpec backup_state)
    : m_group_index(group_index), m_backup_state(backup_state) {
    // The apply function will be set by the caller
    m_apply_func = []() {
    };
    m_unapply_func = []() {
    };
}

void RemoveGroupAction::apply() { m_apply_func(); }

void RemoveGroupAction::unapply() { m_unapply_func(); }
