#pragma once

// Keyboard and mouse state, queryable from ANY system.
//
// Why it exists: SDL_GetKeyboardState only says whether a key is HELD. "Just
// pressed" (the edge) lived in main's event loop, out of reach of the game code,
// so each controller made up its own memory of the previous frame (a jumpPrev
// bool per component). Input stores the state of THIS frame and the PREVIOUS one
// once, and everyone queries the same thing.
//
// It is not a component: it is a STATIC class (a single global input state).
// It does not include SDL either: keys are named with the Key enum below and the
// translation to SDL scancodes happens only in Input.cpp.
//
// Use from the main loop, ONCE per frame, BEFORE updating the systems:
//     Input::update();
// Use from a system:
//     if (Input::wasPressed(Key::Space)) jump();
//     float move = Input::axis(Key::Left, Key::Right);   // -1, 0 or +1

// Available keys. More can be added: just add the value here and its case in the
// translation in Input.cpp (the compiler warns if one is missing; the switch is exhaustive).
enum class Key {
    // Letters
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    // Top-row numbers
    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
    // Arrows
    Left, Right, Up, Down,
    // Control keys
    Space, Enter, Escape, Tab, Backspace, Delete,
    LShift, RShift, LCtrl, RCtrl, LAlt, RAlt,
    // Function keys
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

    Count // end marker: how many keys Input handles (not a key)
};

enum class MouseButton { Left, Middle, Right, Count };

union SDL_Event; // forward declaration: SDL only appears in Input.cpp

class Input {
public:
    // Refreshes the state. The main loop calls it once per frame, before updating
    // the systems. It keeps the previous frame's state for the edges.
    static void update();

    // The mouse wheel is NOT a state that can be read (like a key): it only arrives
    // as an EVENT. That is why the main loop passes each event here, inside its
    // while (SDL_PollEvent(&e)), before Input::update(). Other events are ignored.
    static void processEvent(const SDL_Event& e);

    // Forgets the edges without losing which keys are held: makes the "previous"
    // state equal to the current one. Call it when switching scenes (or pressing Play),
    // so the key that caused the switch is not read as just pressed in the new scene.
    static void reset();

    // With false the keyboard reads as if no key were pressed (the mouse stays the
    // same). The editor turns it off while typing in a panel field, so those keys do
    // not move the player. When it is turned back on, keys that were already held
    // do not count as just pressed. It applies on the next
    // update().
    static void setKeyboardEnabled(bool enabled);

    static bool isDown(Key k);       // held this frame
    static bool wasPressed(Key k);   // went from released to held this frame
    static bool wasReleased(Key k);  // went from held to released this frame

    // Digital axis: +1 if only 'positive', -1 if only 'negative', 0 if neither
    // or both. Shortcut for the pattern "if (left) -1; if (right) +1".
    static float axis(Key negative, Key positive);

    static bool isMouseDown(MouseButton b);
    static bool wasMousePressed(MouseButton b);
    static bool wasMouseReleased(MouseButton b);

    // Cursor position in SCREEN pixels (not world): the window's top left corner is
    // (0,0). Converting it to the world requires the camera.
    static float mouseX();
    static float mouseY();

    // How much the wheel turned in THIS frame: >0 forward (away from the hand), <0
    // backward, 0 if it did not move. One "notch" of a regular wheel is 1.
    static float mouseWheel();
};
