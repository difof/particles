#pragma once

/**
 * @brief Base interface for all undo/redo actions.
 *
 * Provides the contract for actions that can be undone and redone.
 * Supports action coalescing for better user experience during continuous
 * operations.
 */
struct IAction {
    virtual ~IAction() = default;

    /**
     * @brief Get the display name of this action.
     * @return Human-readable name for the action.
     */
    virtual const char *name() const = 0;

    /**
     * @brief Apply this action (redo).
     */
    virtual void apply() = 0;

    /**
     * @brief Undo this action.
     */
    virtual void unapply() = 0;

    /**
     * @brief Check if this action can be coalesced with another action.
     * @param other The other action to check coalescing with.
     * @return True if actions can be coalesced.
     */
    virtual bool canCoalesce(const IAction &other) const {
        (void)other;
        return false;
    }

    /**
     * @brief Coalesce this action with another action.
     * @param other The action to coalesce with.
     * @return True if coalescing was successful.
     */
    virtual bool coalesce(const IAction &other) {
        (void)other;
        return false;
    }
};

