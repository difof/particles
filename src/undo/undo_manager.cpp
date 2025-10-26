#include "undo_manager.hpp"

void UndoManager::push(std::unique_ptr<IAction> act) {
    if (!act)
        return;
    // If inside an interaction and last entry belongs to this interaction,
    // try to coalesce with the top action
    if (m_inInteraction && !m_past.empty() &&
        m_past.back().seq == m_interactionSeq) {
        auto &top = m_past.back().act;
        if (top && top->canCoalesce(*act)) {
            if (top->coalesce(*act)) {
                // Coalesced; future invalidated
                m_future.clear();
                return;
            }
        }
    }
    // Normal push path: apply already happened externally; we just record
    m_past.push_back({std::move(act), m_inInteraction ? m_interactionSeq : 0,
                      std::chrono::steady_clock::now()});
    m_future.clear();
    trim();
    ++m_state_version;
}

void UndoManager::undo() {
    if (m_past.empty())
        return;
    auto entry = std::move(m_past.back());
    m_past.pop_back();
    if (entry.act) {
        entry.act->unapply();
        m_future.push_back(std::move(entry));
    }
    ++m_state_version;
}

void UndoManager::redo() {
    if (m_future.empty())
        return;
    auto entry = std::move(m_future.back());
    m_future.pop_back();
    if (entry.act) {
        entry.act->apply();
        m_past.push_back(std::move(entry));
        trim();
    }
    ++m_state_version;
}

bool UndoManager::is_at_saved_state(unsigned long long saved_version) const {
    // If we're at the exact same version, we're definitely at the saved state
    if (m_state_version == saved_version) {
        return true;
    }

    // If we've made changes since the save, check if we've undone back to the
    // saved state We're at the saved state if:
    // 1. We have exactly the same number of actions in our past as we did when
    // we saved
    // 2. The current state is the result of applying exactly those actions

    // This is a simplified check - in practice, we'd need to track the exact
    // state For now, we'll use a heuristic: if we've undone back to having the
    // same number of actions in our past as we had when we saved, we're likely
    // at the saved state

    // Count how many actions we had when we saved (this is approximate)
    // The saved version tells us how many operations happened before the save
    // If we've undone back to having the same "distance" from the initial
    // state, we're at the saved state

    // For now, let's implement a simple version: if we're at version 0 or if
    // we've undone back to having no actions in our past, we're at a "clean"
    // state
    if (m_past.empty() && saved_version == 0) {
        return true;
    }

    // More sophisticated check would require tracking the exact state
    // For now, we'll return false for any other case
    return false;
}
