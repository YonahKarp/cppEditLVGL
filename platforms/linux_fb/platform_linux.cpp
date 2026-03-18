#include "platform.h"

#include <cstdlib>
#include <cstring>
#include <string>

#include "lvgl.h"

#if defined(__linux__)
#include <array>
#include <cstdio>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

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
bool g_evdev_grabbed = false;
bool g_caps_lock = false;
std::array<bool, KEY_MAX + 1> g_key_state {};
PlatformEvent g_pending_text_event {};
bool g_has_pending_text_event = false;

bool g_termios_saved = false;
termios g_original_termios {};

constexpr int kMaxEventDevices = 32;

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

bool key_bit_is_set(const unsigned long* key_bits, int key_code) {
    constexpr int bits_per_long = static_cast<int>(sizeof(unsigned long) * 8);
    int word = key_code / bits_per_long;
    int bit = key_code % bits_per_long;
    return ((key_bits[word] >> bit) & 1UL) != 0;
}

char shifted_digit_symbol(uint16_t code) {
    switch (code) {
        case KEY_1: return '!';
        case KEY_2: return '@';
        case KEY_3: return '#';
        case KEY_4: return '$';
        case KEY_5: return '%';
        case KEY_6: return '^';
        case KEY_7: return '&';
        case KEY_8: return '*';
        case KEY_9: return '(';
        case KEY_0: return ')';
        default: return '\0';
    }
}

char keycode_to_ascii(uint16_t code, bool shift, bool caps_lock) {
    if (code >= KEY_A && code <= KEY_Z) {
        char base = static_cast<char>('a' + (code - KEY_A));
        bool upper = shift ^ caps_lock;
        return upper ? static_cast<char>(base - 'a' + 'A') : base;
    }

    if (code >= KEY_1 && code <= KEY_0) {
        if (shift) {
            return shifted_digit_symbol(code);
        }
        if (code == KEY_0) {
            return '0';
        }
        return static_cast<char>('1' + (code - KEY_1));
    }

    switch (code) {
        case KEY_SPACE: return ' ';
        case KEY_MINUS: return shift ? '_' : '-';
        case KEY_EQUAL: return shift ? '+' : '=';
        case KEY_LEFTBRACE: return shift ? '{' : '[';
        case KEY_RIGHTBRACE: return shift ? '}' : ']';
        case KEY_BACKSLASH: return shift ? '|' : '\\';
        case KEY_SEMICOLON: return shift ? ':' : ';';
        case KEY_APOSTROPHE: return shift ? '"' : '\'';
        case KEY_GRAVE: return shift ? '~' : '`';
        case KEY_COMMA: return shift ? '<' : ',';
        case KEY_DOT: return shift ? '>' : '.';
        case KEY_SLASH: return shift ? '?' : '/';
        case KEY_KP0: return '0';
        case KEY_KP1: return '1';
        case KEY_KP2: return '2';
        case KEY_KP3: return '3';
        case KEY_KP4: return '4';
        case KEY_KP5: return '5';
        case KEY_KP6: return '6';
        case KEY_KP7: return '7';
        case KEY_KP8: return '8';
        case KEY_KP9: return '9';
        case KEY_KPDOT: return '.';
        case KEY_KPSLASH: return '/';
        case KEY_KPASTERISK: return '*';
        case KEY_KPMINUS: return '-';
        case KEY_KPPLUS: return '+';
        default: return '\0';
    }
}

void queue_text_event(char ch) {
    if (ch == '\0') {
        return;
    }

    g_pending_text_event = {};
    g_pending_text_event.type = PlatformEvent::Type::TextInput;
    g_pending_text_event.text[0] = ch;
    g_pending_text_event.text[1] = '\0';
    g_has_pending_text_event = true;
}

bool device_looks_like_keyboard(int fd) {
    std::array<unsigned long, (KEY_MAX / (sizeof(unsigned long) * 8)) + 1> key_bits {};
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits.data()) < 0) {
        return false;
    }

    return key_bit_is_set(key_bits.data(), KEY_A) &&
           key_bit_is_set(key_bits.data(), KEY_Z) &&
           key_bit_is_set(key_bits.data(), KEY_ENTER) &&
           key_bit_is_set(key_bits.data(), KEY_SPACE);
}

std::string detect_keyboard_evdev() {
    for (int i = 0; i < kMaxEventDevices; ++i) {
        char path[64];
        std::snprintf(path, sizeof(path), "/dev/input/event%d", i);

        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) {
            continue;
        }

        bool is_keyboard = device_looks_like_keyboard(fd);
        close(fd);
        if (is_keyboard) {
            return std::string(path);
        }
    }

    return "";
}

void set_terminal_raw_mode() {
    if (!isatty(STDIN_FILENO)) {
        return;
    }

    if (tcgetattr(STDIN_FILENO, &g_original_termios) != 0) {
        return;
    }

    termios raw = g_original_termios;
    raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0) {
        g_termios_saved = true;
    }
}

void restore_terminal_mode() {
    if (g_termios_saved) {
        tcsetattr(STDIN_FILENO, TCSANOW, &g_original_termios);
        g_termios_saved = false;
    }
}
#endif

}  // namespace

bool init(int32_t, int32_t) {
#if defined(__linux__)
    g_caps_lock = false;
    g_pending_text_event = {};
    g_has_pending_text_event = false;

    const char* fb_file = std::getenv("JUSTTYPE_FBDEV");
    const char* evdev_from_env = std::getenv("JUSTTYPE_EVDEV");
    if (!fb_file) {
        fb_file = "/dev/fb0";
    }

    std::string evdev_path;
    if (evdev_from_env && evdev_from_env[0] != '\0') {
        evdev_path = evdev_from_env;
    } else {
        evdev_path = detect_keyboard_evdev();
        if (evdev_path.empty()) {
            evdev_path = "/dev/input/event0";
        }
    }

    g_display = lv_linux_fbdev_create();
    if (!g_display) {
        return false;
    }
    lv_linux_fbdev_set_file(g_display, fb_file);

    g_keyboard = lv_evdev_create(LV_INDEV_TYPE_KEYPAD, evdev_path.c_str());
    g_evdev_fd = open(evdev_path.c_str(), O_RDONLY | O_NONBLOCK);
    if (g_evdev_fd < 0) {
        std::memset(g_key_state.data(), 0, g_key_state.size() * sizeof(bool));
        std::fprintf(stderr, "Warning: unable to open keyboard device '%s'\n", evdev_path.c_str());
    } else {
        int grab = 1;
        if (ioctl(g_evdev_fd, EVIOCGRAB, grab) == 0) {
            g_evdev_grabbed = true;
        }
    }

    if (!g_keyboard) {
        std::fprintf(stderr, "Warning: lv_evdev_create failed for '%s'\n", evdev_path.c_str());
    }

    set_terminal_raw_mode();
    return true;
#else
    return false;
#endif
}

void shutdown() {
#if defined(__linux__)
    g_pending_text_event = {};
    g_has_pending_text_event = false;

    if (g_evdev_fd >= 0) {
        if (g_evdev_grabbed) {
            int release = 0;
            (void)ioctl(g_evdev_fd, EVIOCGRAB, release);
            g_evdev_grabbed = false;
        }
        close(g_evdev_fd);
        g_evdev_fd = -1;
    }
    restore_terminal_mode();
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
    if (g_has_pending_text_event) {
        event = g_pending_text_event;
        g_pending_text_event = {};
        g_has_pending_text_event = false;
        return true;
    }

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

    if (input.code == KEY_CAPSLOCK && input.value == 1) {
        g_caps_lock = !g_caps_lock;
    }

    if (input.code <= KEY_MAX) {
        g_key_state[input.code] = input.value != 0;
    }

    event.key = linux_key_to_keycode(input.code);
    event.modifiers = current_modifiers();
    event.repeat = input.value == 2;
    event.type = (input.value == 0) ? PlatformEvent::Type::KeyUp : PlatformEvent::Type::KeyDown;

    if (input.value != 0) {
        char ch = keycode_to_ascii(input.code, event.modifiers.shift, g_caps_lock);
        queue_text_event(ch);
    }

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
