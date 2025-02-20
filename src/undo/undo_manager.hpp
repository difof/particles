#pragma once

#include <functional>
#include <imgui.h>
#include <memory>
#include <raylib.h>
#include <string>
#include <utility>
#include <vector>

#include "iaction.hpp"

/**
 * @brief Manages undo/redo functionality with interaction-based coalescing.
 *
 * Provides a complete undo/redo system that can coalesce related actions
 * during user interactions for better UX. Supports limiting history size
 * and interaction-based grouping of actions.
 */
class UndoManager {
  public:
    /**
     * @brief Set the maximum number of actions to keep in history.
     * @param n Maximum number of actions (0 means 1).
     */
    void setMaxSize(size_t n) {
        m_max = (n == 0 ? 1 : n);
        trim();
    }

    /**
     * @brief Push a new action onto the undo stack.
     * @param act The action to push (will be moved).
     */
    void push(std::unique_ptr<IAction> act) {
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
        m_past.push_back(
            {std::move(act), m_inInteraction ? m_interactionSeq : 0});
        m_future.clear();
        trim();
    }

    /**
     * @brief Check if undo is available.
     * @return True if there are actions to undo.
     */
    bool canUndo() const { return !m_past.empty(); }

    /**
     * @brief Check if redo is available.
     * @return True if there are actions to redo.
     */
    bool canRedo() const { return !m_future.empty(); }

    /**
     * @brief Undo the last action.
     */
    void undo() {
        if (m_past.empty())
            return;
        auto entry = std::move(m_past.back());
        m_past.pop_back();
        if (entry.act) {
            entry.act->unapply();
            m_future.push_back(std::move(entry));
        }
    }

    /**
     * @brief Redo the next action.
     */
    void redo() {
        if (m_future.empty())
            return;
        auto entry = std::move(m_future.back());
        m_future.pop_back();
        if (entry.act) {
            entry.act->apply();
            m_past.push_back(std::move(entry));
            trim();
        }
    }

    /**
     * @brief Begin an interaction sequence for coalescing.
     * @param id ImGui ID for the interaction.
     */
    void beginInteraction(ImGuiID id) {
        m_inInteraction = true;
        m_interactionId = id;
        ++m_interactionSeq;
    }

    /**
     * @brief End an interaction sequence.
     * @param id ImGui ID for the interaction.
     */
    void endInteraction(ImGuiID id) {
        if (m_inInteraction && id == m_interactionId) {
            m_inInteraction = false;
            m_interactionId = 0;
        }
    }

  private:
    /**
     * @brief Trim the history to the maximum size.
     */
    void trim() {
        if (m_past.size() > m_max)
            m_past.erase(m_past.begin(),
                         m_past.begin() + (m_past.size() - m_max));
    }

  private:
    struct Entry {
        std::unique_ptr<IAction> act;
        unsigned long long seq;
    };
    std::vector<Entry> m_past;
    std::vector<Entry> m_future;
    size_t m_max = 500;
    bool m_inInteraction = false;
    ImGuiID m_interactionId = 0;
    unsigned long long m_interactionSeq = 0;
};
