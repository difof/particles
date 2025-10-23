#include "key_manager.hpp"

#include <raylib.h>

void KeyManager::on_key_pressed(int key, std::function<void()> handler,
                                bool ctrl, bool shift, bool alt) {
    m_handlers.emplace_back(key, Mode::Pressed, ctrl, shift, alt,
                            std::move(handler));
}

void KeyManager::on_key_down(int key, std::function<void()> handler, bool ctrl,
                             bool shift, bool alt) {
    m_handlers.emplace_back(key, Mode::Down, ctrl, shift, alt,
                            std::move(handler));
}

void KeyManager::on_key_repeat(int key, std::function<void()> handler,
                               bool ctrl, bool shift, bool alt) {
    m_handlers.emplace_back(key, Mode::Repeat, ctrl, shift, alt,
                            std::move(handler));
}

void KeyManager::process(bool imgui_captured) {
    // Skip all handlers if ImGui has captured keyboard input
    if (imgui_captured) {
        return;
    }

    for (const auto &handler : m_handlers) {
        // Check if modifiers match
        if (!check_modifiers(handler)) {
            continue;
        }

        // Check key state based on mode
        bool should_trigger = false;
        switch (handler.mode) {
        case Mode::Pressed:
            should_trigger = IsKeyPressed(handler.key);
            break;
        case Mode::Down:
            should_trigger = IsKeyDown(handler.key);
            break;
        case Mode::Repeat:
            should_trigger = IsKeyPressedRepeat(handler.key);
            break;
        }

        if (should_trigger) {
            handler.callback();
        }
    }
}

void KeyManager::clear() { m_handlers.clear(); }

bool KeyManager::check_modifiers(const Handler &handler) const {
    // Check Ctrl/Cmd/Super modifier
    if (handler.ctrl) {
        bool ctrl_down =
            IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) ||
            IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
        if (!ctrl_down) {
            return false;
        }
    }

    // Check Shift modifier
    if (handler.shift) {
        bool shift_down =
            IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        if (!shift_down) {
            return false;
        }
    }

    // Check Alt modifier
    if (handler.alt) {
        bool alt_down = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
        if (!alt_down) {
            return false;
        }
    }

    // If no modifiers required, ensure no modifiers are pressed
    if (!handler.ctrl && !handler.shift && !handler.alt) {
        bool any_modifier =
            IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) ||
            IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER) ||
            IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT) ||
            IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
        if (any_modifier) {
            return false;
        }
    }

    return true;
}
