# JustType LVGL

A distraction-free text editor built with LVGL, designed for embedded MCUs. This is a port of the original JustType editor from SDL/Nuklear to LVGL.

## Features

- **Distraction-free editing**: Clean, minimal interface focused on writing
- **File management sidebar**: Quick access to your text files
- **Search functionality**: Find text within your documents
- **Dark/Light themes**: Switch between themes with Ctrl+T
- **Font size control**: Adjust font size with Ctrl+/Ctrl-
- **Auto-save**: Content is automatically saved after typing stops
- **Word count**: Real-time word count display

## Requirements

- CMake 3.14+
- C++17 compiler
- pthreads

### Platform-specific requirements

- `PLATFORM=sdl` (default): SDL2 + `pkg-config`
- `PLATFORM=linux_fb`: Linux framebuffer + evdev devices (`/dev/fb*`, `/dev/input/event*`)

## Building

### macOS (SDL)

```bash
# Install dependencies
brew install sdl2 pkg-config

# Configure + build
cmake -S . -B build -DPLATFORM=sdl
cmake --build build -j

# Run
./build/justtype_lvgl
```

### Linux (SDL, recommended)

```bash
# Ubuntu/Debian dependencies
sudo apt update
sudo apt install -y build-essential cmake pkg-config libsdl2-dev

# Configure + build
cmake -S . -B build -DPLATFORM=sdl
cmake --build build -j

# Run
./build/justtype_lvgl
```

### Linux (Framebuffer + evdev)

This target does **not** use SDL. It is intended for direct Linux console/framebuffer usage.

```bash
# Ubuntu/Debian dependencies
sudo apt update
sudo apt install -y build-essential cmake linux-libc-dev

# Configure + build
cmake -S . -B build-fb -DPLATFORM=linux_fb
cmake --build build-fb -j

# Run (usually needs device access to /dev/fb0 and /dev/input/event*)
sudo ./build-fb/justtype_lvgl
```

Optional runtime device overrides:

```bash
JUSTTYPE_FBDEV=/dev/fb1 JUSTTYPE_EVDEV=/dev/input/event3 sudo ./build-fb/justtype_lvgl
```
