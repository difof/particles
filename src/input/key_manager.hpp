#pragma once

#include <functional>
#include <vector>

#include <raylib.h>

/**
 * @brief Centralized keyboard input manager using callback-based handlers.
 *
 * Provides a clean API for registering keyboard shortcuts with modifier
 * support. Automatically handles ImGui keyboard capture state to prevent
 * conflicts.
 */
class KeyManager {
  public:
    /**
     * @brief Key input modes for different behaviors.
     */
    enum class Mode {
        Pressed, ///< Triggered once when key is pressed
        Down,    ///< Triggered continuously while key is held
        Repeat   ///< Triggered repeatedly while key is held (with repeat delay)
    };

    /**
     * @brief Register a handler for key press events.
     * @param key The raylib key code
     * @param handler Function to call when key is pressed
     * @param ctrl Whether Ctrl/Cmd/Super modifier is required
     * @param shift Whether Shift modifier is required
     * @param alt Whether Alt modifier is required
     */
    void on_key_pressed(int key, std::function<void()> handler,
                        bool ctrl = false, bool shift = false,
                        bool alt = false);

    /**
     * @brief Register a handler for key down events (continuous).
     * @param key The raylib key code
     * @param handler Function to call while key is held
     * @param ctrl Whether Ctrl/Cmd/Super modifier is required
     * @param shift Whether Shift modifier is required
     * @param alt Whether Alt modifier is required
     */
    void on_key_down(int key, std::function<void()> handler, bool ctrl = false,
                     bool shift = false, bool alt = false);

    /**
     * @brief Register a handler for key repeat events.
     * @param key The raylib key code
     * @param handler Function to call on key repeat
     * @param ctrl Whether Ctrl/Cmd/Super modifier is required
     * @param shift Whether Shift modifier is required
     * @param alt Whether Alt modifier is required
     */
    void on_key_repeat(int key, std::function<void()> handler,
                       bool ctrl = false, bool shift = false, bool alt = false);

    /**
     * @brief Process all registered handlers for the current frame.
     * @param imgui_captured Whether ImGui has captured keyboard input
     */
    void process(bool imgui_captured);

    /**
     * @brief Clear all registered handlers.
     */
    void clear();

  private:
    /**
     * @brief Internal structure storing handler information.
     */
    struct Handler {
        int key;
        Mode mode;
        bool ctrl;
        bool shift;
        bool alt;
        std::function<void()> callback;

        Handler(int k, Mode m, bool c, bool s, bool a, std::function<void()> cb)
            : key(k), mode(m), ctrl(c), shift(s), alt(a),
              callback(std::move(cb)) {}
    };

    /**
     * @brief Check if modifier keys match the handler requirements.
     * @param handler The handler to check modifiers for
     * @return true if modifiers match, false otherwise
     */
    bool check_modifiers(const Handler &handler) const;

    /** @brief Vector storing all registered handlers. */
    std::vector<Handler> m_handlers;
};
