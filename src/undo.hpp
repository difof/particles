#ifndef __UNDO_HPP
#define __UNDO_HPP

#include <functional>
#include <imgui.h>
#include <memory>
#include <raylib.h>
#include <string>
#include <utility>
#include <vector>

// Forward declarations
namespace mailbox::command {
struct SeedSpec;
}

// Simple, generic undo/redo with interaction-based coalescing.

struct IAction {
    virtual ~IAction() = default;
    virtual const char *name() const = 0;
    virtual void apply() = 0;
    virtual void unapply() = 0;
    virtual bool canCoalesce(const IAction &other) const {
        (void)other;
        return false;
    }
    virtual bool coalesce(const IAction &other) {
        (void)other;
        return false;
    }
};

class UndoManager {
  public:
    void setMaxSize(size_t n) {
        m_max = (n == 0 ? 1 : n);
        trim();
    }

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

    bool canUndo() const { return !m_past.empty(); }
    bool canRedo() const { return !m_future.empty(); }

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

    void beginInteraction(ImGuiID id) {
        m_inInteraction = true;
        m_interactionId = id;
        ++m_interactionSeq;
    }

    void endInteraction(ImGuiID id) {
        if (m_inInteraction && id == m_interactionId) {
            m_inInteraction = false;
            m_interactionId = 0;
        }
    }

  private:
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

// Generic value action operating via getter/setter. Coalesces by same key.
template <typename T>
class ValueAction : public IAction {
  public:
    using Getter = std::function<T()>;
    using Setter = std::function<void(const T &)>;

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

// Forward declarations for group undo actions
class Simulation;
class World;

// Custom undo action for group removal
class RemoveGroupAction : public IAction {
  public:
    RemoveGroupAction(int group_index,
                      std::shared_ptr<mailbox::command::SeedSpec> backup_state);
    const char *name() const override { return "Remove Group"; }
    void apply() override;
    void unapply() override;
    bool canCoalesce(const IAction &other) const override { return false; }
    bool coalesce(const IAction &other) override { return false; }

    void set_apply_func(std::function<void()> func) { m_apply_func = func; }
    void set_unapply_func(std::function<void()> func) { m_unapply_func = func; }

  private:
    int m_group_index;
    std::shared_ptr<mailbox::command::SeedSpec> m_backup_state;
    std::function<void()> m_apply_func;
    std::function<void()> m_unapply_func;
};

// Custom undo action for group addition
class AddGroupAction : public IAction {
  public:
    AddGroupAction(int size, Color color, float r2, int group_index);
    const char *name() const override { return "Add Group"; }
    void apply() override;
    void unapply() override;
    bool canCoalesce(const IAction &other) const override { return false; }
    bool coalesce(const IAction &other) override { return false; }

    void set_apply_func(std::function<void()> func) { m_apply_func = func; }
    void set_unapply_func(std::function<void()> func) { m_unapply_func = func; }

  private:
    int m_size;
    Color m_color;
    float m_r2;
    int m_group_index;
    std::function<void()> m_apply_func;
    std::function<void()> m_unapply_func;
};

// Custom undo action for clearing all groups
class ClearAllGroupsAction : public IAction {
  public:
    ClearAllGroupsAction(
        std::shared_ptr<mailbox::command::SeedSpec> backup_state);
    const char *name() const override { return "Clear All Groups"; }
    void apply() override;
    void unapply() override;
    bool canCoalesce(const IAction &other) const override { return false; }
    bool coalesce(const IAction &other) override { return false; }

    void set_apply_func(std::function<void()> func) { m_apply_func = func; }
    void set_unapply_func(std::function<void()> func) { m_unapply_func = func; }

  private:
    std::shared_ptr<mailbox::command::SeedSpec> m_backup_state;
    std::function<void()> m_apply_func;
    std::function<void()> m_unapply_func;
};

// Custom undo action for group resize
class ResizeGroupAction : public IAction {
  public:
    ResizeGroupAction(int group_index, int old_size, int new_size);
    const char *name() const override { return "Resize Group"; }
    void apply() override;
    void unapply() override;
    bool canCoalesce(const IAction &other) const override { return false; }
    bool coalesce(const IAction &other) override { return false; }

    void set_apply_func(std::function<void()> func) { m_apply_func = func; }
    void set_unapply_func(std::function<void()> func) { m_unapply_func = func; }

  private:
    int m_group_index;
    int m_old_size;
    int m_new_size;
    std::function<void()> m_apply_func;
    std::function<void()> m_unapply_func;
};

#endif
