#pragma once

#include <functional>
#include <memory>

#include "iaction.hpp"
#include "mailbox/mailbox.hpp"

/**
 * @brief Undo action for removing a particle group.
 */
class RemoveGroupAction : public IAction {
  public:
    /**
     * @brief Construct a remove group action.
     * @param group_index Index of the group to remove.
     * @param backup_state Backup state before removal.
     */
    RemoveGroupAction(int group_index,
                      std::shared_ptr<mailbox::command::SeedSpec> backup_state);

    const char *name() const override { return "Remove Group"; }
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
    std::shared_ptr<mailbox::command::SeedSpec> m_backup_state;
    std::function<void()> m_apply_func;
    std::function<void()> m_unapply_func;
};

