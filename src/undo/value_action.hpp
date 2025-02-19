#pragma once

#include <functional>
#include <string>

#include "iaction.hpp"

/**
 * @brief Generic value action operating via getter/setter functions.
 *
 * Coalesces actions by the same key, making it ideal for continuous
 * value changes during user interactions.
 */
template <typename T>
class ValueAction : public IAction {
  public:
    using Getter = std::function<T()>;
    using Setter = std::function<void(const T &)>;

    /**
     * @brief Construct a value action.
     * @param key Unique key for coalescing.
     * @param label Display label for the action.
     * @param get Getter function for current value.
     * @param set Setter function for new value.
     * @param before Value before the change.
     * @param after Value after the change.
     */
    ValueAction(std::string key, std::string label, Getter get, Setter set,
                const T &before, const T &after)
        : m_key(std::move(key)), m_label(std::move(label)),
          m_get(std::move(get)), m_set(std::move(set)), m_before(before),
          m_after(after) {}

    const char *name() const override { return m_label.c_str(); }
    void apply() override { m_set(m_after); }
    void unapply() override { m_set(m_before); }

    bool canCoalesce(const IAction &other) const override {
        auto *o = dynamic_cast<const ValueAction<T> *>(&other);
        return o && o->m_key == m_key;
    }

    bool coalesce(const IAction &other) override {
        auto *o = dynamic_cast<const ValueAction<T> *>(&other);
        if (!o || o->m_key != m_key)
            return false;
        m_after = o->m_after;
        return true;
    }

  private:
    std::string m_key;
    std::string m_label;
    Getter m_get;
    Setter m_set;
    T m_before;
    T m_after;
};

