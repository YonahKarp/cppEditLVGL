#include "platform.h"

#include <cstdlib>
#include <cstring>
#include <string>

#include "lvgl.h"

#if defined(__linux__)
#include <array>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>

#include "src/drivers/display/fb/lv_linux_fbdev.h"
#include "src/drivers/evdev/lv_evdev.h"
#endif

namespace platform {
namespace {

lv_display_t* g_display = nullptr;
lv_indev_t* g_keyboard = nullptr;
std::string g_clipboard;

#if defined(__linux__)
int g_evdev_fd = -1;
std::array<bool, KEY_MAX + 1> g_key_state {};

KeyCode linux_key_to_keycode(uint16_t code) {
    switch (code) {
        case KEY_A: return KeyCode::A;
        case KEY_C: return KeyCode::C;
        case KEY_F: return KeyCode::F;
        case KEY_T: return KeyCode::T;
        case KEY_V: return KeyCode::V;
        case KEY_X: return KeyCode::X;
        case KEY_EQUAL:
        case KEY_KPPLUS: return KeyCode::Equals;
        case KEY_MINUS:
        case KEY_KPMINUS: return KeyCode::Minus;
        case KEY_BACKSPACE: return KeyCode::Backspace;
        case KEY_DELETE: return KeyCode::Delete;
        case KEY_ENTER:
        case KEY_KPENTER: return KeyCode::Enter;
        case KEY_ESC: return KeyCode::Escape;
        case KEY_TAB: return KeyCode::Tab;
        case KEY_UP: return KeyCode::Up;
        case KEY_DOWN: return KeyCode::Down;
        case KEY_LEFT: return KeyCode::Left;
        case KEY_RIGHT: return KeyCode::Right;
        case KEY_HOME: return KeyCode::Home;
        case KEY_END: return KeyCode::End;
        case KEY_LEFTSHIFT:
        case KEY_RIGHTSHIFT: return KeyCode::Shift;
        case KEY_LEFTCTRL:
        case KEY_RIGHTCTRL: return KeyCode::Control;
        case KEY_LEFTALT:
        case KEY_RIGHTALT: return KeyCode::Alt;
        case KEY_LEFTMETA:
        case KEY_RIGHTMETA: return KeyCode::Meta;
        default: return KeyCode::Unknown;
    }
}

bool raw_pressed(uint16_t code) {
    if (code > KEY_MAX) {
        return false;
    }
    return g_key_state[code];
}

KeyModifiers current_modifiers() {
    KeyModifiers modifiers {};
    modifiers.ctrl = raw_pressed(KEY_LEFTCTRL) || raw_pressed(KEY_RIGHTCTRL);
    modifiers.shift = raw_pressed(KEY_LEFTSHIFT) || raw_pressed(KEY_RIGHTSHIFT);
    modifiers.alt = raw_pressed(KEY_LEFTALT) || raw_pressed(KEY_RIGHTALT);
    modifiers.meta = raw_pressed(KEY_LEFTMETA) || raw_pressed(KEY_RIGHTMETA);
    return modifiers;
}

void sleep_timeout(uint32_t timeout_ms) {
    struct pollfd fd {};
    (void)poll(&fd, 0, static_cast<int>(timeout_ms));
}
#endif

}  // namespace

bool init(int32_t, int32_t) {
#if defined(__linux__)
    const char* fb_file = std::getenv("JUSTTYPE_FBDEV");
    const char* evdev_file = std::getenv("JUSTTYPE_EVDEV");
    if (!fb_file) {
        fb_file = "/dev/fb0";
    }
    if (!evdev_file) {
        evdev_file = "/dev/input/event0";
    }

    g_display = lv_linux_fbdev_create();
    if (!g_display) {
        return false;
    }
    lv_linux_fbdev_set_file(g_display, fb_file);

    g_keyboard = lv_evdev_create(LV_INDEV_TYPE_KEYPAD, evdev_file);
    g_evdev_fd = open(evdev_file, O_RDONLY | O_NONBLOCK);
    if (g_evdev_fd < 0) {
        std::memset(g_key_state.data(), 0, g_key_state.size() * sizeof(bool));
    }

    return true;
#else
    return false;
#endif
}

void shutdown() {
#if defined(__linux__)
    if (g_evdev_fd >= 0) {
        close(g_evdev_fd);
        g_evdev_fd = -1;
    }
#endif
    g_display = nullptr;
    g_keyboard = nullptr;
    g_clipboard.clear();
}

lv_display_t* get_display() {
    return g_display;
}

lv_indev_t* get_keyboard_indev() {
    return g_keyboard;
}

bool poll_event(PlatformEvent& event, uint32_t timeout_ms) {
    event = {};

#if defined(__linux__)
    if (g_evdev_fd < 0) {
        sleep_timeout(timeout_ms);
        return false;
    }

    struct pollfd pfd {};
    pfd.fd = g_evdev_fd;
    pfd.events = POLLIN;

    int ready = poll(&pfd, 1, static_cast<int>(timeout_ms));
    if (ready <= 0 || (pfd.revents & POLLIN) == 0) {
        return false;
    }

    struct input_event input {};
    ssize_t bytes = read(g_evdev_fd, &input, sizeof(input));
    if (bytes != static_cast<ssize_t>(sizeof(input))) {
        return false;
    }

    if (input.type != EV_KEY) {
        event.type = PlatformEvent::Type::None;
        return true;
    }

    if (input.code <= KEY_MAX) {
        g_key_state[input.code] = input.value != 0;
    }

    event.key = linux_key_to_keycode(input.code);
    event.modifiers = current_modifiers();
    event.repeat = input.value == 2;
    event.type = (input.value == 0) ? PlatformEvent::Type::KeyUp : PlatformEvent::Type::KeyDown;
    return true;
#else
    (void)timeout_ms;
    return false;
#endif
}

void requeue_last_event() {
}

void pump_events() {
}

KeyModifiers get_key_modifiers() {
#if defined(__linux__)
    return current_modifiers();
#else
    return {};
#endif
}

bool is_key_pressed(KeyCode key) {
#if defined(__linux__)
    switch (key) {
        case KeyCode::A: return raw_pressed(KEY_A);
        case KeyCode::C: return raw_pressed(KEY_C);
        case KeyCode::F: return raw_pressed(KEY_F);
        case KeyCode::T: return raw_pressed(KEY_T);
        case KeyCode::V: return raw_pressed(KEY_V);
        case KeyCode::X: return raw_pressed(KEY_X);
        case KeyCode::Equals: return raw_pressed(KEY_EQUAL) || raw_pressed(KEY_KPPLUS);
        case KeyCode::Minus: return raw_pressed(KEY_MINUS) || raw_pressed(KEY_KPMINUS);
        case KeyCode::Backspace: return raw_pressed(KEY_BACKSPACE);
        case KeyCode::Delete: return raw_pressed(KEY_DELETE);
        case KeyCode::Enter: return raw_pressed(KEY_ENTER) || raw_pressed(KEY_KPENTER);
        case KeyCode::Escape: return raw_pressed(KEY_ESC);
        case KeyCode::Tab: return raw_pressed(KEY_TAB);
        case KeyCode::Up: return raw_pressed(KEY_UP);
        case KeyCode::Down: return raw_pressed(KEY_DOWN);
        case KeyCode::Left: return raw_pressed(KEY_LEFT);
        case KeyCode::Right: return raw_pressed(KEY_RIGHT);
        case KeyCode::Home: return raw_pressed(KEY_HOME);
        case KeyCode::End: return raw_pressed(KEY_END);
        case KeyCode::Shift: return raw_pressed(KEY_LEFTSHIFT) || raw_pressed(KEY_RIGHTSHIFT);
        case KeyCode::Control: return raw_pressed(KEY_LEFTCTRL) || raw_pressed(KEY_RIGHTCTRL);
        case KeyCode::Alt: return raw_pressed(KEY_LEFTALT) || raw_pressed(KEY_RIGHTALT);
        case KeyCode::Meta: return raw_pressed(KEY_LEFTMETA) || raw_pressed(KEY_RIGHTMETA);
        default: return false;
    }
#else
    (void)key;
    return false;
#endif
}

bool clipboard_set(const char* text) {
    g_clipboard = text ? text : "";
    return true;
}

char* clipboard_get() {
    char* text = static_cast<char*>(std::malloc(g_clipboard.size() + 1));
    if (!text) {
        return nullptr;
    }
    std::memcpy(text, g_clipboard.c_str(), g_clipboard.size() + 1);
    return text;
}

void clipboard_free(char* text) {
    std::free(text);
}

uint32_t get_ticks_ms() {
    return lv_tick_get();
}

}  // namespace platform
