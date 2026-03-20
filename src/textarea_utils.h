#pragma once

#include <cstddef>
#include <cstdint>

#include "lvgl.h"

bool textarea_has_selection(lv_obj_t* textarea);
bool textarea_delete_selected_text(lv_obj_t* textarea, uint32_t* out_cursor_pos = nullptr);
bool textarea_add_text_with_limit(lv_obj_t* textarea, const char* text, size_t max_bytes);
