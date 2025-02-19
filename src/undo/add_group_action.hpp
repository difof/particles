#pragma once

#include <functional>
#include <raylib.h>

#include "iaction.hpp"

/**
 * @brief Undo action for adding a particle group.
 */
class AddGroupAction : public IAction {
  public:
    /**
     * @brief Construct an add group action.
     * @param size Number of particles in the group.
     * @param color Color of the particles.
     * @param r2 Interaction radius squared.
     * @param group_index Index where the group will be added.
     */
    AddGroupAction(int size, Color color, float r2, int group_index);

    const char *name() const override { return "Add Group"; }
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
    int m_size;
    Color m_color;
    float m_r2;
    int m_group_index;
    std::function<void()> m_apply_func;
    std::function<void()> m_unapply_func;
};

