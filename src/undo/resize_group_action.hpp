#pragma once

#include <functional>

#include "iaction.hpp"

/**
 * @brief Undo action for resizing a particle group.
 */
class ResizeGroupAction : public IAction {
  public:
    /**
     * @brief Construct a resize group action.
     * @param group_index Index of the group to resize.
     * @param old_size Original size of the group.
     * @param new_size New size of the group.
     */
    ResizeGroupAction(int group_index, int old_size, int new_size);

    const char *name() const override { return "Resize Group"; }
    void apply() override;
    void unapply() override;
    bool canCoalesce(const IAction &other) const override { return false; }
    bool coalesce(const IAction &other) override { return false; }

    /**
     * @brief Set the function to call when applying this action.
     * @param func Function to call on apply.
     */
    void set_apply_func(std::function<void()> func) { m_apply_func = func; }

    /**
     * @brief Set the function to call when unapplying this action.
     * @param func Function to call on unapply.
     */
    void set_unapply_func(std::function<void()> func) { m_unapply_func = func; }

  private:
    int m_group_index;
    int m_old_size;
    int m_new_size;
    std::function<void()> m_apply_func;
    std::function<void()> m_unapply_func;
};

