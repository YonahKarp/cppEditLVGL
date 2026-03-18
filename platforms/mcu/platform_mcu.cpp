#include "platform.h"

#include <cstdlib>
#include <cstring>
#include <string>

#include "lvgl.h"

namespace platform {
namespace {

lv_display_t* g_display = nullptr;
lv_indev_t* g_keyboard = nullptr;
std::string g_clipboard;

}  // namespace

bool init(int32_t, int32_t) {
    // TODO: Initialize MCU display driver (SPI/parallel LCD).
    // TODO: Initialize MCU keyboard matrix / GPIO input.
    return true;
}

void shutdown() {
    g_clipboard.clear();
}

lv_display_t* get_display() {
    return g_display;
}

lv_indev_t* get_keyboard_indev() {
    return g_keyboard;
}

bool poll_event(PlatformEvent& event, uint32_t timeout_ms) {
    (void)timeout_ms;
    event = {};
    return false;
}

void requeue_last_event() {
}

void pump_events() {
}

KeyModifiers get_key_modifiers() {
    return {};
}

bool is_key_pressed(KeyCode) {
    return false;
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
