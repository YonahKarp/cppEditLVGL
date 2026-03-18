#include "platform.h"

#include <SDL2/SDL.h>
#include <cstring>

#include "lvgl.h"
#include "src/drivers/sdl/lv_sdl_keyboard.h"
#include "src/drivers/sdl/lv_sdl_window.h"

namespace platform {
namespace {

lv_display_t* g_display = nullptr;
lv_indev_t* g_keyboard = nullptr;
SDL_Event g_last_event {};
bool g_has_last_event = false;

KeyCode scancode_to_key(SDL_Scancode scancode) {
    switch (scancode) {
        case SDL_SCANCODE_A: return KeyCode::A;
        case SDL_SCANCODE_C: return KeyCode::C;
        case SDL_SCANCODE_F: return KeyCode::F;
        case SDL_SCANCODE_T: return KeyCode::T;
        case SDL_SCANCODE_V: return KeyCode::V;
        case SDL_SCANCODE_X: return KeyCode::X;
        case SDL_SCANCODE_EQUALS: return KeyCode::Equals;
        case SDL_SCANCODE_MINUS: return KeyCode::Minus;
        case SDL_SCANCODE_BACKSPACE: return KeyCode::Backspace;
        case SDL_SCANCODE_DELETE: return KeyCode::Delete;
        case SDL_SCANCODE_RETURN:
        case SDL_SCANCODE_KP_ENTER: return KeyCode::Enter;
        case SDL_SCANCODE_ESCAPE: return KeyCode::Escape;
        case SDL_SCANCODE_TAB: return KeyCode::Tab;
        case SDL_SCANCODE_UP: return KeyCode::Up;
        case SDL_SCANCODE_DOWN: return KeyCode::Down;
        case SDL_SCANCODE_LEFT: return KeyCode::Left;
        case SDL_SCANCODE_RIGHT: return KeyCode::Right;
        case SDL_SCANCODE_HOME: return KeyCode::Home;
        case SDL_SCANCODE_END: return KeyCode::End;
        case SDL_SCANCODE_LSHIFT:
        case SDL_SCANCODE_RSHIFT: return KeyCode::Shift;
        case SDL_SCANCODE_LCTRL:
        case SDL_SCANCODE_RCTRL: return KeyCode::Control;
        case SDL_SCANCODE_LALT:
        case SDL_SCANCODE_RALT: return KeyCode::Alt;
        case SDL_SCANCODE_LGUI:
        case SDL_SCANCODE_RGUI: return KeyCode::Meta;
        default: return KeyCode::Unknown;
    }
}

KeyModifiers modifiers_from_mod_state(SDL_Keymod mod) {
    KeyModifiers modifiers {};
    modifiers.ctrl = (mod & KMOD_CTRL) != 0;
    modifiers.shift = (mod & KMOD_SHIFT) != 0;
    modifiers.alt = (mod & KMOD_ALT) != 0;
    modifiers.meta = (mod & KMOD_GUI) != 0;
    return modifiers;
}

bool is_scancode_pressed(SDL_Scancode scancode) {
    const uint8_t* state = SDL_GetKeyboardState(nullptr);
    return state && state[scancode];
}

}  // namespace

bool init(int32_t width, int32_t height) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        return false;
    }

    SDL_StartTextInput();

    g_display = lv_sdl_window_create(width, height);
    if (!g_display) {
        SDL_Quit();
        return false;
    }

    g_keyboard = lv_sdl_keyboard_create();
    if (!g_keyboard) {
        g_display = nullptr;
        SDL_StopTextInput();
        SDL_Quit();
        return false;
    }

    return true;
}

void shutdown() {
    g_display = nullptr;
    g_keyboard = nullptr;
    g_has_last_event = false;
    lv_sdl_quit();
    SDL_StopTextInput();
    SDL_Quit();
}

lv_display_t* get_display() {
    return g_display;
}

lv_indev_t* get_keyboard_indev() {
    return g_keyboard;
}

bool poll_event(PlatformEvent& event, uint32_t timeout_ms) {
    event = {};
    SDL_Event raw {};
    if (!SDL_WaitEventTimeout(&raw, static_cast<int>(timeout_ms))) {
        g_has_last_event = false;
        return false;
    }

    g_last_event = raw;
    g_has_last_event = true;

    switch (raw.type) {
        case SDL_QUIT:
            event.type = PlatformEvent::Type::Quit;
            break;
        case SDL_KEYDOWN:
            event.type = PlatformEvent::Type::KeyDown;
            event.key = scancode_to_key(raw.key.keysym.scancode);
            event.modifiers = modifiers_from_mod_state(static_cast<SDL_Keymod>(raw.key.keysym.mod));
            event.repeat = raw.key.repeat != 0;
            break;
        case SDL_KEYUP:
            event.type = PlatformEvent::Type::KeyUp;
            event.key = scancode_to_key(raw.key.keysym.scancode);
            event.modifiers = modifiers_from_mod_state(static_cast<SDL_Keymod>(raw.key.keysym.mod));
            break;
        case SDL_TEXTINPUT:
            event.type = PlatformEvent::Type::TextInput;
            std::strncpy(event.text, raw.text.text, sizeof(event.text) - 1);
            event.text[sizeof(event.text) - 1] = '\0';
            break;
        default:
            event.type = PlatformEvent::Type::None;
            break;
    }

    return true;
}

void requeue_last_event() {
    if (!g_has_last_event) {
        return;
    }

    SDL_PushEvent(&g_last_event);
    g_has_last_event = false;
}

void pump_events() {
    SDL_PumpEvents();
}

KeyModifiers get_key_modifiers() {
    SDL_Keymod mod = SDL_GetModState();
    return modifiers_from_mod_state(mod);
}

bool is_key_pressed(KeyCode key) {
    switch (key) {
        case KeyCode::A: return is_scancode_pressed(SDL_SCANCODE_A);
        case KeyCode::C: return is_scancode_pressed(SDL_SCANCODE_C);
        case KeyCode::F: return is_scancode_pressed(SDL_SCANCODE_F);
        case KeyCode::T: return is_scancode_pressed(SDL_SCANCODE_T);
        case KeyCode::V: return is_scancode_pressed(SDL_SCANCODE_V);
        case KeyCode::X: return is_scancode_pressed(SDL_SCANCODE_X);
        case KeyCode::Equals: return is_scancode_pressed(SDL_SCANCODE_EQUALS) || is_scancode_pressed(SDL_SCANCODE_KP_PLUS);
        case KeyCode::Minus: return is_scancode_pressed(SDL_SCANCODE_MINUS) || is_scancode_pressed(SDL_SCANCODE_KP_MINUS);
        case KeyCode::Backspace: return is_scancode_pressed(SDL_SCANCODE_BACKSPACE);
        case KeyCode::Delete: return is_scancode_pressed(SDL_SCANCODE_DELETE);
        case KeyCode::Enter: return is_scancode_pressed(SDL_SCANCODE_RETURN) || is_scancode_pressed(SDL_SCANCODE_KP_ENTER);
        case KeyCode::Escape: return is_scancode_pressed(SDL_SCANCODE_ESCAPE);
        case KeyCode::Tab: return is_scancode_pressed(SDL_SCANCODE_TAB);
        case KeyCode::Up: return is_scancode_pressed(SDL_SCANCODE_UP);
        case KeyCode::Down: return is_scancode_pressed(SDL_SCANCODE_DOWN);
        case KeyCode::Left: return is_scancode_pressed(SDL_SCANCODE_LEFT);
        case KeyCode::Right: return is_scancode_pressed(SDL_SCANCODE_RIGHT);
        case KeyCode::Home: return is_scancode_pressed(SDL_SCANCODE_HOME);
        case KeyCode::End: return is_scancode_pressed(SDL_SCANCODE_END);
        case KeyCode::Shift: return is_scancode_pressed(SDL_SCANCODE_LSHIFT) || is_scancode_pressed(SDL_SCANCODE_RSHIFT);
        case KeyCode::Control: return is_scancode_pressed(SDL_SCANCODE_LCTRL) || is_scancode_pressed(SDL_SCANCODE_RCTRL);
        case KeyCode::Alt: return is_scancode_pressed(SDL_SCANCODE_LALT) || is_scancode_pressed(SDL_SCANCODE_RALT);
        case KeyCode::Meta: return is_scancode_pressed(SDL_SCANCODE_LGUI) || is_scancode_pressed(SDL_SCANCODE_RGUI);
        default: return false;
    }
}

bool clipboard_set(const char* text) {
    if (!text) {
        return false;
    }
    return SDL_SetClipboardText(text) == 0;
}

char* clipboard_get() {
    return SDL_GetClipboardText();
}

void clipboard_free(char* text) {
    SDL_free(text);
}

uint32_t get_ticks_ms() {
    return lv_tick_get();
}

}  // namespace platform
