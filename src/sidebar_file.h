#pragma once

#include "lvgl.h"
#include "sidebar_folder.h"

struct SidebarState;
struct EditorState;
struct Theme;

// Create a file item UI element in the sidebar
lv_obj_t* create_file_item_ui(
    SidebarState& sidebar,
    EditorState& editor,
    lv_obj_t* parent,
    const SidebarItem& item,
    int index,
    bool is_selected,
    const Theme& theme,
    lv_event_cb_t click_cb
);

// Create a deleted file item UI element in the sidebar
lv_obj_t* create_deleted_file_item_ui(
    lv_obj_t* parent,
    const std::string& filename,
    int index,
    bool is_selected,
    const Theme& theme
);
