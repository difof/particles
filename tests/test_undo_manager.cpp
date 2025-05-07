#include <catch_amalgamated.hpp>
#include <memory>
#include <string>

#include "mailbox/mailbox.hpp"
#include "undo/add_group_action.hpp"
#include "undo/clear_all_groups_action.hpp"
#include "undo/iaction.hpp"
#include "undo/remove_group_action.hpp"
#include "undo/resize_group_action.hpp"
#include "undo/undo_manager.hpp"
#include "undo/value_action.hpp"

/**
 * @brief Simple test action that tracks apply/unapply calls.
 */
class TestAction : public IAction {
  public:
    TestAction(const std::string &name, int &counter, int delta)
        : m_name(name), m_counter(counter), m_delta(delta) {}

    const char *name() const override { return m_name.c_str(); }

    void apply() override {
        m_counter += m_delta;
        m_apply_count++;
    }

    void unapply() override {
        m_counter -= m_delta;
        m_unapply_count++;
    }

    bool canCoalesce(const IAction &other) const override {
        auto *o = dynamic_cast<const TestAction *>(&other);
        return o && o->m_name == m_name;
    }

    bool coalesce(const IAction &other) override {
        auto *o = dynamic_cast<const TestAction *>(&other);
        if (!o || o->m_name != m_name)
            return false;
        m_delta += o->m_delta;
        return true;
    }

    int get_apply_count() const { return m_apply_count; }
    int get_unapply_count() const { return m_unapply_count; }
    int get_delta() const { return m_delta; }

  private:
    std::string m_name;
    int &m_counter;
    int m_delta;
    int m_apply_count = 0;
    int m_unapply_count = 0;
};

TEST_CASE("UndoManager - Basic operations", "[undo_manager]") {
    UndoManager manager;
    int counter = 0;

    SECTION("Empty manager state") {
        REQUIRE_FALSE(manager.canUndo());
        REQUIRE_FALSE(manager.canRedo());
    }

    SECTION("Single action push and undo") {
        auto action = std::make_unique<TestAction>("test", counter, 5);
        action
            ->apply(); // Apply the action before pushing (as per usage pattern)
        manager.push(std::move(action));

        REQUIRE(manager.canUndo());
        REQUIRE_FALSE(manager.canRedo());
        REQUIRE(counter == 5); // Action was applied

        manager.undo();
        REQUIRE_FALSE(manager.canUndo());
        REQUIRE(manager.canRedo());
        REQUIRE(counter == 0); // Action was unapplied
    }

    SECTION("Single action push, undo, and redo") {
        auto action = std::make_unique<TestAction>("test", counter, 3);
        action->apply(); // Apply the action before pushing
        manager.push(std::move(action));

        REQUIRE(counter == 3); // Action was applied
        manager.undo();
        REQUIRE(counter == 0); // Action was unapplied
        manager.redo();
        REQUIRE(counter == 3); // Action was reapplied
    }

    SECTION("Multiple actions") {
        auto action1 = std::make_unique<TestAction>("test1", counter, 2);
        auto action2 = std::make_unique<TestAction>("test2", counter, 3);

        action1->apply(); // Apply the action before pushing
        manager.push(std::move(action1));
        action2->apply(); // Apply the action before pushing
        manager.push(std::move(action2));

        REQUIRE(counter == 5); // Both actions were applied
        REQUIRE(manager.canUndo());
        REQUIRE_FALSE(manager.canRedo());

        manager.undo();
        REQUIRE(counter == 2); // Second action was unapplied
        REQUIRE(manager.canUndo());
        REQUIRE(manager.canRedo());

        manager.undo();
        REQUIRE(counter == 0); // First action was unapplied
        REQUIRE_FALSE(manager.canUndo());
        REQUIRE(manager.canRedo());
    }

    SECTION("Multiple undo and redo") {
        auto action1 = std::make_unique<TestAction>("test1", counter, 1);
        auto action2 = std::make_unique<TestAction>("test2", counter, 2);
        auto action3 = std::make_unique<TestAction>("test3", counter, 3);

        action1->apply(); // Apply the action before pushing
        manager.push(std::move(action1));
        action2->apply(); // Apply the action before pushing
        manager.push(std::move(action2));
        action3->apply(); // Apply the action before pushing
        manager.push(std::move(action3));

        REQUIRE(counter == 6); // All actions were applied

        // Undo all
        manager.undo();
        REQUIRE(counter == 3); // Third action was unapplied
        manager.undo();
        REQUIRE(counter == 1); // Second action was unapplied
        manager.undo();
        REQUIRE(counter == 0); // First action was unapplied

        // Redo all
        manager.redo();
        REQUIRE(counter == 1); // First action was reapplied
        manager.redo();
        REQUIRE(counter == 3); // Second action was reapplied
        manager.redo();
        REQUIRE(counter == 6); // Third action was reapplied
    }
}

TEST_CASE("UndoManager - Edge cases", "[undo_manager]") {
    UndoManager manager;
    int counter = 0;

    SECTION("Null action push") {
        manager.push(nullptr);
        REQUIRE_FALSE(manager.canUndo());
        REQUIRE_FALSE(manager.canRedo());
    }

    SECTION("Undo on empty stack") {
        manager.undo();
        REQUIRE_FALSE(manager.canUndo());
        REQUIRE_FALSE(manager.canRedo());
    }

    SECTION("Redo on empty future") {
        manager.redo();
        REQUIRE_FALSE(manager.canUndo());
        REQUIRE_FALSE(manager.canRedo());
    }

    SECTION("Undo with null action") {
        auto action = std::make_unique<TestAction>("test", counter, 5);
        action->apply(); // Apply the action before pushing
        manager.push(std::move(action));

        // Manually create a null action scenario by pushing a unique_ptr that
        // will be moved
        auto null_action = std::unique_ptr<IAction>(nullptr);
        manager.push(std::move(null_action));

        // Should still be able to undo the first action
        REQUIRE(manager.canUndo());
        manager.undo();
        REQUIRE(counter == 0); // First action was unapplied
    }
}

TEST_CASE("UndoManager - History size limits", "[undo_manager]") {
    UndoManager manager;
    int counter = 0;

    SECTION("Default size limit") {
        // Push more actions than default limit (500)
        for (int i = 0; i < 600; ++i) {
            auto action = std::make_unique<TestAction>("test", counter, 1);
            action->apply(); // Apply the action before pushing
            manager.push(std::move(action));
        }

        REQUIRE(counter == 600); // All actions were applied

        // Should be able to undo up to the limit
        for (int i = 0; i < 500; ++i) {
            manager.undo();
        }
        REQUIRE(counter == 100); // 500 actions were unapplied
        REQUIRE_FALSE(manager.canUndo());
    }

    SECTION("Custom size limit") {
        manager.setMaxSize(3);

        // Push 5 actions
        for (int i = 0; i < 5; ++i) {
            auto action = std::make_unique<TestAction>("test", counter, 1);
            action->apply(); // Apply the action before pushing
            manager.push(std::move(action));
        }

        REQUIRE(counter == 5); // All actions were applied

        // Should only be able to undo 3 actions
        for (int i = 0; i < 3; ++i) {
            manager.undo();
        }
        REQUIRE(counter == 2); // 3 actions were unapplied
        REQUIRE_FALSE(manager.canUndo());
    }

    SECTION("Zero size limit") {
        manager.setMaxSize(0);

        auto action = std::make_unique<TestAction>("test", counter, 5);
        action->apply(); // Apply the action before pushing
        manager.push(std::move(action));

        // With size 0, should keep 1 action (as per setMaxSize logic)
        REQUIRE(manager.canUndo());
        manager.undo();
        REQUIRE(counter == 0); // Action was unapplied
        REQUIRE_FALSE(manager.canUndo());
    }
}

TEST_CASE("UndoManager - Interaction coalescing", "[undo_manager]") {
    UndoManager manager;
    int counter = 0;

    SECTION("Basic interaction") {
        manager.beginInteraction(1);

        auto action1 = std::make_unique<TestAction>("test", counter, 2);
        action1->apply(); // Apply the action before pushing
        manager.push(std::move(action1));

        auto action2 = std::make_unique<TestAction>("test", counter, 3);
        action2->apply(); // Apply the action before pushing
        manager.push(std::move(action2));

        manager.endInteraction(1);

        // Actions should be coalesced into one
        REQUIRE(counter == 5); // Both actions were applied
        REQUIRE(manager.canUndo());
        REQUIRE_FALSE(manager.canRedo());

        manager.undo();
        REQUIRE(counter == 0); // Coalesced action was unapplied
    }

    SECTION("Interaction with non-coalescing actions") {
        manager.beginInteraction(1);

        auto action1 = std::make_unique<TestAction>("test1", counter, 2);
        action1->apply(); // Apply the action before pushing
        manager.push(std::move(action1));

        auto action2 = std::make_unique<TestAction>("test2", counter, 3);
        action2->apply(); // Apply the action before pushing
        manager.push(std::move(action2));

        manager.endInteraction(1);

        // Actions should not be coalesced (different names)
        REQUIRE(counter == 5); // Both actions were applied
        REQUIRE(manager.canUndo());

        manager.undo();
        REQUIRE(counter == 2); // Second action was unapplied
        manager.undo();
        REQUIRE(counter == 0); // First action was unapplied
    }

    SECTION("Multiple interactions") {
        // First interaction
        manager.beginInteraction(1);
        auto action1 = std::make_unique<TestAction>("test", counter, 2);
        action1->apply(); // Apply the action before pushing
        manager.push(std::move(action1));
        manager.endInteraction(1);

        // Second interaction
        manager.beginInteraction(2);
        auto action2 = std::make_unique<TestAction>("test", counter, 3);
        action2->apply(); // Apply the action before pushing
        manager.push(std::move(action2));
        manager.endInteraction(2);

        // Should have two separate actions
        REQUIRE(counter == 5); // Both actions were applied
        REQUIRE(manager.canUndo());

        manager.undo();
        REQUIRE(counter == 2); // Second action was unapplied
        manager.undo();
        REQUIRE(counter == 0); // First action was unapplied
    }

    SECTION("Interaction end with wrong ID") {
        manager.beginInteraction(1);
        auto action = std::make_unique<TestAction>("test", counter, 5);
        action->apply(); // Apply the action before pushing
        manager.push(std::move(action));

        // End with wrong ID - interaction should continue
        manager.endInteraction(2);

        auto action2 = std::make_unique<TestAction>("test", counter, 3);
        action2->apply(); // Apply the action before pushing
        manager.push(std::move(action2));

        // Should be coalesced since interaction is still active
        REQUIRE(counter == 8); // Both actions were applied
        manager.undo();
        REQUIRE(counter == 0); // Coalesced action was unapplied
    }
}

TEST_CASE("UndoManager - Future clearing", "[undo_manager]") {
    UndoManager manager;
    int counter = 0;

    SECTION("New action clears future") {
        auto action1 = std::make_unique<TestAction>("test1", counter, 2);
        auto action2 = std::make_unique<TestAction>("test2", counter, 3);
        auto action3 = std::make_unique<TestAction>("test3", counter, 4);

        action1->apply(); // Apply the action before pushing
        manager.push(std::move(action1));
        action2->apply(); // Apply the action before pushing
        manager.push(std::move(action2));
        action3->apply(); // Apply the action before pushing
        manager.push(std::move(action3));

        // Undo two actions
        manager.undo();
        manager.undo();
        REQUIRE(counter == 2); // Two actions were unapplied
        REQUIRE(manager.canRedo());

        // Push new action - should clear future
        auto action4 = std::make_unique<TestAction>("test4", counter, 1);
        action4->apply(); // Apply the action before pushing
        manager.push(std::move(action4));

        REQUIRE(counter == 3); // New action was applied
        REQUIRE_FALSE(manager.canRedo());
        REQUIRE(manager.canUndo());
    }
}

TEST_CASE("UndoManager - Action lifecycle", "[undo_manager]") {
    UndoManager manager;
    int counter = 0;

    SECTION("Action apply/unapply counts") {
        auto action = std::make_unique<TestAction>("test", counter, 5);
        TestAction *action_ptr = action.get();

        action->apply(); // Apply the action before pushing
        manager.push(std::move(action));

        // Action should be applied once
        REQUIRE(action_ptr->get_apply_count() == 1);
        REQUIRE(action_ptr->get_unapply_count() == 0);
        REQUIRE(counter == 5);

        manager.undo();

        // Action should be unapplied once
        REQUIRE(action_ptr->get_apply_count() == 1);
        REQUIRE(action_ptr->get_unapply_count() == 1);
        REQUIRE(counter == 0);

        manager.redo();

        // Action should be applied again
        REQUIRE(action_ptr->get_apply_count() == 2);
        REQUIRE(action_ptr->get_unapply_count() == 1);
        REQUIRE(counter == 5);
    }
}

TEST_CASE("UndoManager - Default Actions", "[undo_manager]") {
    UndoManager manager;

    SECTION("ValueAction - Basic functionality") {
        int value = 10;
        auto getter = [&value]() {
            return value;
        };
        auto setter = [&value](int v) {
            value = v;
        };

        auto action = std::make_unique<ValueAction<int>>(
            "test_key", "Test Value", getter, setter, 10, 20);

        REQUIRE(std::string(action->name()) == "Test Value");
        REQUIRE(value == 10);

        action->apply();
        REQUIRE(value == 20);

        action->unapply();
        REQUIRE(value == 10);
    }

    SECTION("ValueAction - Coalescing") {
        int value = 5;
        auto getter = [&value]() {
            return value;
        };
        auto setter = [&value](int v) {
            value = v;
        };

        auto action1 = std::make_unique<ValueAction<int>>(
            "same_key", "Value 1", getter, setter, 5, 10);
        auto action2 = std::make_unique<ValueAction<int>>(
            "same_key", "Value 2", getter, setter, 10, 15);

        // Actions with same key should be able to coalesce
        REQUIRE(action1->canCoalesce(*action2));

        // Coalescing should work
        REQUIRE(action1->coalesce(*action2));

        // After coalescing, action1 should have the final value
        action1->apply();
        REQUIRE(value == 15);
    }

    SECTION("ValueAction - Non-coalescing") {
        int value = 5;
        auto getter = [&value]() {
            return value;
        };
        auto setter = [&value](int v) {
            value = v;
        };

        auto action1 = std::make_unique<ValueAction<int>>(
            "key1", "Value 1", getter, setter, 5, 10);
        auto action2 = std::make_unique<ValueAction<int>>(
            "key2", "Value 2", getter, setter, 10, 15);

        // Actions with different keys should not coalesce
        REQUIRE_FALSE(action1->canCoalesce(*action2));
        REQUIRE_FALSE(action1->coalesce(*action2));
    }

    SECTION("AddGroupAction - Basic functionality") {
        Color test_color = {255, 0, 0, 255};
        auto action =
            std::make_unique<AddGroupAction>(100, test_color, 4096.0f, 0);

        REQUIRE(std::string(action->name()) == "Add Group");

        // Test that it doesn't coalesce with other actions
        auto other_action =
            std::make_unique<AddGroupAction>(50, test_color, 2048.0f, 1);
        REQUIRE_FALSE(action->canCoalesce(*other_action));
        REQUIRE_FALSE(action->coalesce(*other_action));
    }

    SECTION("RemoveGroupAction - Basic functionality") {
        auto backup_state = std::make_shared<mailbox::command::SeedSpec>();
        backup_state->sizes = {100, 200};
        backup_state->colors = {{255, 0, 0, 255}, {0, 255, 0, 255}};

        auto action = std::make_unique<RemoveGroupAction>(1, *backup_state);

        REQUIRE(std::string(action->name()) == "Remove Group");

        // Test that it doesn't coalesce
        auto other_action =
            std::make_unique<RemoveGroupAction>(0, *backup_state);
        REQUIRE_FALSE(action->canCoalesce(*other_action));
        REQUIRE_FALSE(action->coalesce(*other_action));
    }

    SECTION("ResizeGroupAction - Basic functionality") {
        auto action = std::make_unique<ResizeGroupAction>(0, 100, 200);

        REQUIRE(std::string(action->name()) == "Resize Group");

        // Test that it doesn't coalesce
        auto other_action = std::make_unique<ResizeGroupAction>(1, 50, 150);
        REQUIRE_FALSE(action->canCoalesce(*other_action));
        REQUIRE_FALSE(action->coalesce(*other_action));
    }

    SECTION("ClearAllGroupsAction - Basic functionality") {
        auto backup_state = std::make_shared<mailbox::command::SeedSpec>();
        backup_state->sizes = {100, 200, 300};
        backup_state->colors = {
            {255, 0, 0, 255}, {0, 255, 0, 255}, {0, 0, 255, 255}};

        auto action = std::make_unique<ClearAllGroupsAction>(*backup_state);

        REQUIRE(std::string(action->name()) == "Clear All Groups");

        // Test that it doesn't coalesce
        auto other_action =
            std::make_unique<ClearAllGroupsAction>(*backup_state);
        REQUIRE_FALSE(action->canCoalesce(*other_action));
        REQUIRE_FALSE(action->coalesce(*other_action));
    }

    SECTION("Default Actions with UndoManager") {
        int value = 0;
        auto getter = [&value]() {
            return value;
        };
        auto setter = [&value](int v) {
            value = v;
        };

        // Test ValueAction with UndoManager
        auto value_action = std::make_unique<ValueAction<int>>(
            "test", "Test Value", getter, setter, 0, 10);

        value_action->apply(); // Apply before pushing
        manager.push(std::move(value_action));

        REQUIRE(value == 10);
        REQUIRE(manager.canUndo());

        manager.undo();
        REQUIRE(value == 0);
        REQUIRE(manager.canRedo());

        manager.redo();
        REQUIRE(value == 10);
    }

    SECTION("ValueAction Coalescing in UndoManager") {
        int value = 0;
        auto getter = [&value]() {
            return value;
        };
        auto setter = [&value](int v) {
            value = v;
        };

        manager.beginInteraction(1);

        // First value change
        auto action1 = std::make_unique<ValueAction<int>>(
            "slider", "Slider Value", getter, setter, 0, 5);
        action1->apply();
        manager.push(std::move(action1));

        // Second value change (should coalesce)
        auto action2 = std::make_unique<ValueAction<int>>(
            "slider", "Slider Value", getter, setter, 5, 10);
        action2->apply();
        manager.push(std::move(action2));

        manager.endInteraction(1);

        // Should have only one action in the stack (coalesced)
        REQUIRE(value == 10);
        REQUIRE(manager.canUndo());

        manager.undo();
        REQUIRE(value == 0); // Should undo to original value
    }

    SECTION("Mixed Action Types") {
        int value = 0;
        auto getter = [&value]() {
            return value;
        };
        auto setter = [&value](int v) {
            value = v;
        };

        // Add a value action
        auto value_action = std::make_unique<ValueAction<int>>(
            "test", "Test Value", getter, setter, 0, 5);
        value_action->apply();
        manager.push(std::move(value_action));

        // Add a group action (should not coalesce)
        Color test_color = {255, 0, 0, 255};
        auto group_action =
            std::make_unique<AddGroupAction>(100, test_color, 4096.0f, 0);
        manager.push(std::move(group_action));

        REQUIRE(value == 5);
        REQUIRE(manager.canUndo());

        // Undo group action (no effect on value)
        manager.undo();
        REQUIRE(value == 5);

        // Undo value action
        manager.undo();
        REQUIRE(value == 0);
    }
}