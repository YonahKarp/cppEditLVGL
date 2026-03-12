# Platform Abstraction Layer

## Goal
Create a `platforms/` directory with clean separation between platform-specific code, allowing easy switching between Mac (SDL), Pi Lite Linux (fbdev/evdev), and eventually MCU.

---

## Phase 1: Create Directory Structure and Interface

### 1.1 Create platform interface header
**File:** `platforms/platform.h`

Define common interface all platforms implement:
- `platform::init()` / `platform::shutdown()`
- `platform::get_display()` / `platform::get_keyboard_indev()`
- `platform::poll_event()` - unified event polling
- `platform::get_key_modifiers()` - get ctrl/shift/alt/meta state
- `platform::is_key_pressed(KeyCode)` - check specific key
- `platform::clipboard_set/get/free()` - clipboard operations
- `platform::get_ticks_ms()` - timing

Define platform-agnostic types:
- `KeyCode` enum (Up, Down, Left, Right, Enter, Escape, A-Z, etc.)
- `KeyModifiers` struct (ctrl, shift, alt, meta)
- `PlatformEvent` struct (Quit, Key, TextInput)

---

## Phase 2: Implement SDL Platform (Mac/Desktop)

### 2.1 Create SDL platform implementation
**File:** `platforms/sdl/platform_sdl.cpp`

Wrap existing SDL code:
- `SDL_Init()` / `SDL_Quit()`
- `lv_sdl_window_create()` / `lv_sdl_keyboard_create()`
- `SDL_WaitEventTimeout()` → `platform::poll_event()`
- `SDL_GetKeyboardState()` → `platform::is_key_pressed()`
- `SDL_GetClipboardText()` / `SDL_SetClipboardText()`

### 2.2 Create SDL CMake config
**File:** `platforms/sdl/CMakeLists.txt`

---

## Phase 3: Refactor app.cpp

### 3.1 Replace SDL includes and calls
**File:** `src/app.cpp`

Changes:
- Remove `#include <SDL2/SDL.h>`
- Add `#include "platform.h"`
- Replace `SDL_GetKeyboardState()` → `platform::is_key_pressed()`
- Replace `SDL_SCANCODE_*` → `KeyCode::*`
- Replace `SDL_SetClipboardText()` → `platform::clipboard_set()`
- Replace `SDL_GetClipboardText()` → `platform::clipboard_get()`
- Replace `SDL_WaitEventTimeout()` → `platform::poll_event()`
- Replace `SDL_Init()` / `SDL_Quit()` → `platform::init()` / `platform::shutdown()`

---

## Phase 4: Implement Linux Framebuffer Platform (Pi Lite)

### 4.1 Create Linux FB platform implementation
**File:** `platforms/linux_fb/platform_linux.cpp`

Implement:
- `lv_linux_fbdev_create()` for display (`/dev/fb0`)
- `lv_evdev_create()` for keyboard input (`/dev/input/eventX`)
- Simple RAM-based clipboard (or disabled)
- `poll()` or `select()` for event waiting

### 4.2 Create Linux FB CMake config
**File:** `platforms/linux_fb/CMakeLists.txt`

---

## Phase 5: Create MCU Stub

### 5.1 Create MCU platform stub
**File:** `platforms/mcu/platform_mcu.cpp`

Stub implementation with TODOs for:
- Display driver (SPI/parallel LCD)
- Input driver (GPIO keyboard matrix)
- No clipboard support

### 5.2 Create MCU CMake config
**File:** `platforms/mcu/CMakeLists.txt`

---

## Phase 6: Update Build System

### 6.1 Modify root CMakeLists.txt
**File:** `CMakeLists.txt`

Add platform selection:
```cmake
option(PLATFORM "Target platform: sdl, linux_fb, mcu" "sdl")
```

Conditionally include platform subdirectory.

---

## Files to Create
| File | Description |
|------|-------------|
| `platforms/platform.h` | Common interface header |
| `platforms/sdl/platform_sdl.cpp` | SDL implementation |
| `platforms/sdl/CMakeLists.txt` | SDL build config |
| `platforms/linux_fb/platform_linux.cpp` | Pi Lite implementation |
| `platforms/linux_fb/CMakeLists.txt` | Linux FB build config |
| `platforms/mcu/platform_mcu.cpp` | MCU stub |
| `platforms/mcu/CMakeLists.txt` | MCU build config |

## Files to Modify
| File | Changes |
|------|---------|
| `src/app.cpp` | Replace SDL calls with platform interface |
| `CMakeLists.txt` | Add platform selection logic |

---

## Build Commands

```bash
# Mac/Desktop (default)
cmake -B build -DPLATFORM=sdl && cmake --build build

# Pi Lite Linux
cmake -B build -DPLATFORM=linux_fb && cmake --build build

# MCU (future)
cmake -B build -DPLATFORM=mcu && cmake --build build
```

---

## Verification
1. Build with SDL platform, verify app works identically to before
2. Run on Pi Lite with linux_fb platform
3. Verify MCU stub compiles (no runtime test