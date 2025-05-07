#include "clear_all_groups_action.hpp"

// ClearAllGroupsAction implementation
ClearAllGroupsAction::ClearAllGroupsAction(
    mailbox::command::SeedSpec backup_state)
    : m_backup_state(backup_state) {
    // The apply function will be set by the caller
    m_apply_func = []() {
    };
    m_unapply_func = []() {
    };
}

void ClearAllGroupsAction::apply() { m_apply_func(); }

void ClearAllGroupsAction::unapply() { m_unapply_func(); }
