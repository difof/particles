#include "mailbox/mailbox.hpp"
#include "simulation/simulation.hpp"
#include "simulation/world.hpp"
#include "undo.hpp"

// RemoveGroupAction implementation
RemoveGroupAction::RemoveGroupAction(
    int group_index, std::shared_ptr<mailbox::command::SeedSpec> backup_state)
    : m_group_index(group_index), m_backup_state(backup_state) {
    // The apply function will be set by the caller
    m_apply_func = []() {
    };
    m_unapply_func = []() {
    };
}

void RemoveGroupAction::apply() { m_apply_func(); }

void RemoveGroupAction::unapply() { m_unapply_func(); }

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

// ClearAllGroupsAction implementation
ClearAllGroupsAction::ClearAllGroupsAction(
    std::shared_ptr<mailbox::command::SeedSpec> backup_state)
    : m_backup_state(backup_state) {
    // The apply function will be set by the caller
    m_apply_func = []() {
    };
    m_unapply_func = []() {
    };
}

void ClearAllGroupsAction::apply() { m_apply_func(); }

void ClearAllGroupsAction::unapply() { m_unapply_func(); }

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
