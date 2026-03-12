#pragma once

#include "lvgl.h"

struct BatteryIconState {
    lv_obj_t* container = nullptr;
    lv_obj_t* body = nullptr;
    lv_obj_t* fill = nullptr;
    lv_obj_t* tip = nullptr;
    int percent = 100;
};

void create_battery_icon(BatteryIconState& state, lv_obj_t* parent, int percent, lv_color_t border_color, lv_color_t fill_color);
void update_battery_icon(BatteryIconState& state, int percent, lv_color_t fill_color);
void update_battery_icon_theme(BatteryIconState& state, lv_color_t border_color, lv_color_t fill_color);
