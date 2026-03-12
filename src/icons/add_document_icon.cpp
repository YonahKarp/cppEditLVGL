#include "add_document_icon.h"

static void draw_add_document(lv_obj_t* canvas, int size, int canvas_width, lv_color_t color) {
    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);
    
    int margin = 2;
    int left_offset = 3;
    int doc_width = size - margin * 2 - 4;
    int doc_height = size - margin * 2;
    int fold_size = doc_width / 3;
    
    // Document outline with folded corner
    lv_point_precise_t points[6];
    points[0].x = margin + left_offset;
    points[0].y = margin;
    points[1].x = margin + left_offset + doc_width - fold_size;
    points[1].y = margin;
    points[2].x = margin + left_offset + doc_width;
    points[2].y = margin + fold_size;
    points[3].x = margin + left_offset + doc_width;
    points[3].y = margin + doc_height;
    points[4].x = margin + left_offset;
    points[4].y = margin + doc_height;
    points[5].x = margin + left_offset;
    points[5].y = margin;
    
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = color;
    line_dsc.width = 1;
    line_dsc.round_start = 0;
    line_dsc.round_end = 0;
    
    // Draw document outline
    for (int i = 0; i < 5; i++) {
        line_dsc.p1.x = points[i].x;
        line_dsc.p1.y = points[i].y;
        line_dsc.p2.x = points[i + 1].x;
        line_dsc.p2.y = points[i + 1].y;
        lv_draw_line(&layer, &line_dsc);
    }
    
    // Draw fold line
    line_dsc.p1.x = margin + left_offset + doc_width - fold_size;
    line_dsc.p1.y = margin;
    line_dsc.p2.x = margin + left_offset + doc_width - fold_size;
    line_dsc.p2.y = margin + fold_size;
    lv_draw_line(&layer, &line_dsc);
    
    line_dsc.p1.x = margin + left_offset + doc_width - fold_size;
    line_dsc.p1.y = margin + fold_size;
    line_dsc.p2.x = margin + left_offset + doc_width;
    line_dsc.p2.y = margin + fold_size;
    lv_draw_line(&layer, &line_dsc);
    
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

void create_add_document_icon(AddDocumentIconState& state, lv_obj_t* parent, int size, lv_color_t color) {
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
    
    draw_add_document(state.canvas, size, canvas_width, color);
}

void update_add_document_icon_color(AddDocumentIconState& state, lv_color_t color) {
    int canvas_width = state.size + 2;
    lv_canvas_fill_bg(state.canvas, lv_color_hex(0x000000), LV_OPA_TRANSP);
    draw_add_document(state.canvas, state.size, canvas_width, color);
}
