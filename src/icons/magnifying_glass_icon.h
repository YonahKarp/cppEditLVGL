#pragma once

#include "lvgl.h"

struct MagnifyingGlassIconState {
    lv_obj_t* canvas = nullptr;
    int size = 16;
};

void create_magnifying_glass_icon(MagnifyingGlassIconState& state, lv_obj_t* parent, int size, lv_color_t color);
void update_magnifying_glass_icon_color(MagnifyingGlassIconState& state, lv_color_t color);
