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
- SDL2
- C++17 compiler

## Building

### macOS

```bash
# Install dependencies
brew install sdl2

# Build
mkdir build
cd build
cmake ..
make
./justtype_lvgl
```

### Linux

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt install libsdl2-dev

# Build
mkdir build
cd build
cmake ..
make
./justtype_lvgl
```

### Embedded MCU

For embedded targets, you'll need to:

1. Replace the SDL drivers with your MCU's display and input drivers
2. Configure `lv_conf.h` for your target (memory, display settings, etc.)
3. Remove SDL-related code from `app.cpp` and replace with your HAL

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Meta (GUI key) | Toggle sidebar |
| Ctrl + F | Search in document |
| Ctrl + T | Toggle dark/light theme |
| Ctrl + + | Increase font size |
| Ctrl + - | Decrease font size |
| Ctrl + A | Select all |
| Ctrl + Z | Undo |
| Escape | Close sidebar/search |
| Ctrl+Shift+Alt+Escape | Quit application |

### Search Mode

| Shortcut | Action |
|----------|--------|
| Enter | Next match |
| Shift + Enter | Previous match |
| Escape | Close search |

## Project Structure

```
justTypeLVGL/
├── CMakeLists.txt          # Build configuration
├── lv_conf.h               # LVGL configuration
├── README.md               # This file
├── fonts/                  # Font files (optional)
└── src/
    ├── main.cpp            # Entry point
    ├── app.h/cpp           # Application initialization
    ├── editor.h/cpp        # Text editor component
    ├── sidebar.h/cpp       # File management sidebar
    ├── search.h/cpp        # Search functionality
    ├── file_manager.h/cpp  # File system operations
    └── theme.h             # Color themes
```

## Porting to Embedded

The project uses LVGL's SDL drivers for desktop development. To port to an embedded target:

1. **Display Driver**: Replace `lv_sdl_window_create()` with your display driver
2. **Input Driver**: Replace `lv_sdl_mouse_create()` and `lv_sdl_keyboard_create()` with your input drivers
3. **Tick Source**: Implement `lv_tick_inc()` from your timer interrupt
4. **File System**: Modify file operations in `file_manager.cpp` for your storage

See the [LVGL porting guide](https://docs.lvgl.io/master/porting/index.html) for details.

## License

MIT License
