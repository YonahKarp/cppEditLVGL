#include "magnifying_glass_icon.h"
#include <cmath>

static void draw_magnifying_glass(lv_obj_t* canvas, int size, lv_color_t color) {
    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);
    
    lv_draw_arc_dsc_t arc_dsc;
    lv_draw_arc_dsc_init(&arc_dsc);
    arc_dsc.color = color;
    arc_dsc.width = 2;
    arc_dsc.start_angle = 0;
    arc_dsc.end_angle = 360;
    
    float circle_radius = size * 0.35f;
    arc_dsc.center.x = (int)(circle_radius + 1);
    arc_dsc.center.y = (int)(circle_radius + 1);
    arc_dsc.radius = (int)circle_radius;
    
    lv_draw_arc(&layer, &arc_dsc);
    
    float circle_cx = circle_radius + 1;
    float circle_cy = circle_radius + 1;
    float handle_start_x = circle_cx + circle_radius * 0.5f;
    float handle_start_y = circle_cy + circle_radius * 0.5f;
    float handle_end_x = size - 3;
    float handle_end_y = size - 3;
    
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = color;
    line_dsc.width = 3;
    line_dsc.round_start = 1;
    line_dsc.round_end = 1;
    line_dsc.p1.x = (int)handle_start_x;
    line_dsc.p1.y = (int)handle_start_y;
    line_dsc.p2.x = (int)handle_end_x;
    line_dsc.p2.y = (int)handle_end_y;
    
    lv_draw_line(&layer, &line_dsc);
    
    lv_canvas_finish_layer(canvas, &layer);
}

void create_magnifying_glass_icon(MagnifyingGlassIconState& state, lv_obj_t* parent, int size, lv_color_t color) {
    state.size = size;
    
    state.canvas = lv_canvas_create(parent);
    lv_obj_set_size(state.canvas, size, size);
    
    // ARGB8888 requires 4 bytes per pixel. Use two alternating buffers for multiple icon instances.
    static uint8_t cbuf1[18 * 18 * 4];
    static uint8_t cbuf2[18 * 18 * 4];
    static int buf_index = 0;
    uint8_t* cbuf = (buf_index == 0) ? cbuf1 : cbuf2;
    buf_index = (buf_index + 1) % 2;
    
    lv_canvas_set_buffer(state.canvas, cbuf, size, size, LV_COLOR_FORMAT_ARGB8888);
    lv_canvas_fill_bg(state.canvas, lv_color_hex(0x000000), LV_OPA_TRANSP);
    
    draw_magnifying_glass(state.canvas, size, color);
}

void update_magnifying_glass_icon_color(MagnifyingGlassIconState& state, lv_color_t color) {
    lv_canvas_fill_bg(state.canvas, lv_color_hex(0x000000), LV_OPA_TRANSP);
    draw_magnifying_glass(state.canvas, state.size, color);
}
