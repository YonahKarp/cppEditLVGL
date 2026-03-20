#pragma once

#include <cstdint>

#include "lvgl.h"

namespace platform {

enum class KeyCode {
    Unknown = 0,
    A,
    C,
    F,
    T,
    V,
    X,
    Equals,
    Minus,
    Backspace,
    Delete,
    Enter,
    Escape,
    Grave,
    Tab,
    Up,
    Down,
    Left,
    Right,
    Home,
    End,
    Shift,
    Control,
    Alt,
    Meta
};

struct KeyModifiers {
    bool ctrl = false;
    bool shift = false;
    bool alt = false;
    bool meta = false;
};

struct PlatformEvent {
    enum class Type {
        None = 0,
        Quit,
        KeyDown,
        KeyUp,
        TextInput
    };

    Type type = Type::None;
    KeyCode key = KeyCode::Unknown;
    KeyModifiers modifiers {};
    bool repeat = false;
    char text[32] = {0};
};

bool init(int32_t width, int32_t height);
void shutdown();

lv_display_t* get_display();
lv_indev_t* get_keyboard_indev();

bool poll_event(PlatformEvent& event, uint32_t timeout_ms);
void requeue_last_event();
void pump_events();

KeyModifiers get_key_modifiers();
bool is_key_pressed(KeyCode key);

bool clipboard_set(const char* text);
char* clipboard_get();
void clipboard_free(char* text);

uint32_t get_ticks_ms();

}  // namespace platform
