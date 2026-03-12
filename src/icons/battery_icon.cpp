#include "battery_icon.h"

static const int BATT_WIDTH = 30;
static const int BATT_HEIGHT = 14;
static const int BATT_TIP_WIDTH = 3;
static const int BATT_TIP_HEIGHT = 6;

void create_battery_icon(BatteryIconState& state, lv_obj_t* parent, int percent, lv_color_t border_color, lv_color_t fill_color) {
    state.percent = percent;
    
    state.container = lv_obj_create(parent);
    lv_obj_remove_style_all(state.container);
    lv_obj_set_size(state.container, BATT_WIDTH + BATT_TIP_WIDTH + 2, BATT_HEIGHT);
    lv_obj_clear_flag(state.container, LV_OBJ_FLAG_SCROLLABLE);
    
    // Battery body (outline)
    state.body = lv_obj_create(state.container);
    lv_obj_remove_style_all(state.body);
    lv_obj_set_size(state.body, BATT_WIDTH, BATT_HEIGHT);
    lv_obj_set_pos(state.body, 0, 0);
    lv_obj_set_style_border_width(state.body, 1, 0);
    lv_obj_set_style_border_color(state.body, border_color, 0);
    lv_obj_set_style_radius(state.body, 2, 0);
    lv_obj_set_style_bg_opa(state.body, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(state.body, LV_OBJ_FLAG_SCROLLABLE);
    
    // Battery fill (inside body)
    state.fill = lv_obj_create(state.body);
    lv_obj_remove_style_all(state.fill);
    int fill_width = (BATT_WIDTH - 4) * percent / 100;
    lv_obj_set_size(state.fill, fill_width, BATT_HEIGHT - 4);
    lv_obj_set_pos(state.fill, 2, 2);
    lv_obj_set_style_bg_color(state.fill, fill_color, 0);
    lv_obj_set_style_bg_opa(state.fill, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(state.fill, 0, 0);
    lv_obj_clear_flag(state.fill, LV_OBJ_FLAG_SCROLLABLE);
    
    // Battery tip (right side nub)
    state.tip = lv_obj_create(state.container);
    lv_obj_remove_style_all(state.tip);
    lv_obj_set_size(state.tip, BATT_TIP_WIDTH, BATT_TIP_HEIGHT);
    lv_obj_set_pos(state.tip, BATT_WIDTH, (BATT_HEIGHT - BATT_TIP_HEIGHT) / 2);
    lv_obj_set_style_bg_color(state.tip, border_color, 0);
    lv_obj_set_style_bg_opa(state.tip, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(state.tip, 0, 0);
    lv_obj_clear_flag(state.tip, LV_OBJ_FLAG_SCROLLABLE);
}

void update_battery_icon(BatteryIconState& state, int percent, lv_color_t fill_color) {
    state.percent = percent;
    
    int fill_width = (BATT_WIDTH - 4) * percent / 100;
    if (fill_width < 0) fill_width = 0;
    
    lv_obj_set_width(state.fill, fill_width);
    lv_obj_set_style_bg_color(state.fill, fill_color, 0);
}

void update_battery_icon_theme(BatteryIconState& state, lv_color_t border_color, lv_color_t fill_color) {
    lv_obj_set_style_border_color(state.body, border_color, 0);
    lv_obj_set_style_bg_color(state.tip, border_color, 0);
    lv_obj_set_style_bg_color(state.fill, fill_color, 0);
}
