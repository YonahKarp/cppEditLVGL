#pragma once

#include "lvgl.h"

struct EditorState;
struct SidebarState;
struct SearchState;

struct App {
    lv_display_t* display = nullptr;
    lv_indev_t* keyboard_indev = nullptr;
    lv_group_t* input_group = nullptr;
    
    lv_obj_t* screen = nullptr;
    
    int window_width = 1024;
    int window_height = 600;
    
    EditorState* editor = nullptr;
    SidebarState* sidebar = nullptr;
    SearchState* search = nullptr;
};

bool init_app(App& app);
void shutdown_app(App& app);
void run_app(App& app);
