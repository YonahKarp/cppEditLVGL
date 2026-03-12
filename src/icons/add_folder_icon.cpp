#include "add_folder_icon.h"

static void draw_add_folder(lv_obj_t* canvas, int size, int canvas_width, lv_color_t color) {
    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);
  
    int margin = 2;
    int left_offset = 2;
    int folder_width = size - margin * 2 - 2;  // wider
    int folder_height = size - margin * 2 - 4;  // slightly shorter
    int top_offset = 3;  // push folder down to center vertically
    int tab_width = folder_width / 3;
    int tab_height = 3;
    
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = color;
    line_dsc.width = 1;
    line_dsc.round_start = 0;
    line_dsc.round_end = 0;
    
    // Folder outline with tab
    // Start from bottom left, go clockwise
    lv_point_precise_t points[8];
    points[0].x = margin + left_offset;
    points[0].y = top_offset + folder_height;
    points[1].x = margin + left_offset;
    points[1].y = top_offset + tab_height;
    points[2].x = margin + left_offset;
    points[2].y = top_offset;
    points[3].x = margin + left_offset + tab_width;
    points[3].y = top_offset;
    points[4].x = margin + left_offset + tab_width + tab_height;
    points[4].y = top_offset + tab_height;
    points[5].x = margin + left_offset + folder_width;
    points[5].y = top_offset + tab_height;
    points[6].x = margin + left_offset + folder_width;
    points[6].y = top_offset + folder_height;
    points[7].x = margin + left_offset;
    points[7].y = top_offset + folder_height;
    
    // Draw folder outline
    for (int i = 0; i < 7; i++) {
        line_dsc.p1.x = points[i].x;
        line_dsc.p1.y = points[i].y;
        line_dsc.p2.x = points[i + 1].x;
        line_dsc.p2.y = points[i + 1].y;
        lv_draw_line(&layer, &line_dsc);
    }
    
    // Draw filled circle with plus cutout in bottom-right corner
    int circle_radius = size / 3 - 1;
    int circle_cx = size - circle_radius + 1;
    int circle_cy = size - circle_radius - 1;
    
    // Draw filled circle using a rect with rounded corners
    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_color = color;
    rect_dsc.bg_opa = LV_OPA_COVER;
    rect_dsc.radius = circle_radius;
    
    lv_area_t circle_area;
    circle_area.x1 = circle_cx - circle_radius;
    circle_area.y1 = circle_cy - circle_radius;
    circle_area.x2 = circle_cx + circle_radius;
    circle_area.y2 = circle_cy + circle_radius;
    lv_draw_rect(&layer, &rect_dsc, &circle_area);
    
    lv_canvas_finish_layer(canvas, &layer);
    
    // Draw plus cutout by setting pixels to transparent
    int plus_size = circle_radius - 2;
    
    // Horizontal line of plus (1px thick)
    for (int x = circle_cx - plus_size; x <= circle_cx + plus_size; x++) {
        if (x >= 0 && x < canvas_width && circle_cy >= 0 && circle_cy < size) {
            lv_canvas_set_px(canvas, x, circle_cy, lv_color_hex(0x000000), LV_OPA_TRANSP);
        }
    }
    
    // Vertical line of plus (1px thick)
    for (int y = circle_cy - plus_size; y <= circle_cy + plus_size; y++) {
        if (circle_cx >= 0 && circle_cx < canvas_width && y >= 0 && y < size) {
            lv_canvas_set_px(canvas, circle_cx, y, lv_color_hex(0x000000), LV_OPA_TRANSP);
        }
    }
}

void create_add_folder_icon(AddFolderIconState& state, lv_obj_t* parent, int size, lv_color_t color) {
    state.size = size;
    int canvas_width = size + 2;
    
    state.canvas = lv_canvas_create(parent);
    lv_obj_set_size(state.canvas, canvas_width, size);
    
    // ARGB8888 requires 4 bytes per pixel. Use two alternating buffers for multiple icon instances.
    static uint8_t cbuf1[26 * 24 * 4];
    static uint8_t cbuf2[26 * 24 * 4];
    static int buf_index = 0;
    uint8_t* cbuf = (buf_index == 0) ? cbuf1 : cbuf2;
    buf_index = (buf_index + 1) % 2;
    
    lv_canvas_set_buffer(state.canvas, cbuf, canvas_width, size, LV_COLOR_FORMAT_ARGB8888);
    lv_canvas_fill_bg(state.canvas, lv_color_hex(0x000000), LV_OPA_TRANSP);
    
    draw_add_folder(state.canvas, size, canvas_width, color);
}

void update_add_folder_icon_color(AddFolderIconState& state, lv_color_t color) {
    int canvas_width = state.size + 2;
    lv_canvas_fill_bg(state.canvas, lv_color_hex(0x000000), LV_OPA_TRANSP);
    draw_add_folder(state.canvas, state.size, canvas_width, color);
}
