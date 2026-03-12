#pragma once

#include "lvgl.h"

struct AddDocumentIconState {
    lv_obj_t* canvas = nullptr;
    int size = 20;
};

void create_add_document_icon(AddDocumentIconState& state, lv_obj_t* parent, int size, lv_color_t color);
void update_add_document_icon_color(AddDocumentIconState& state, lv_color_t color);
