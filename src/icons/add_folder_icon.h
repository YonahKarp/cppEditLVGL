#pragma once

#include "lvgl.h"

struct AddFolderIconState {
    lv_obj_t* canvas = nullptr;
    int size = 20;
};

void create_add_folder_icon(AddFolderIconState& state, lv_obj_t* parent, int size, lv_color_t color);
void update_add_folder_icon_color(AddFolderIconState& state, lv_color_t color);
