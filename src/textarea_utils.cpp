#include "textarea_utils.h"

#include <cstring>
#include <string>

bool textarea_has_selection(lv_obj_t* textarea) {
    if (!textarea || !lv_obj_check_type(textarea, &lv_textarea_class)) return false;

    lv_obj_t* label = lv_textarea_get_label(textarea);
    if (!label) return false;

    uint32_t sel_start = lv_label_get_text_selection_start(label);
    uint32_t sel_end = lv_label_get_text_selection_end(label);

    return sel_start != LV_LABEL_TEXT_SELECTION_OFF &&
           sel_end != LV_LABEL_TEXT_SELECTION_OFF &&
           sel_start != sel_end;
}

static uint32_t get_selection_length(lv_obj_t* textarea) {
    if (!textarea || !lv_obj_check_type(textarea, &lv_textarea_class)) {
        return 0;
    }

    lv_obj_t* label = lv_textarea_get_label(textarea);
    if (!label) return 0;

    uint32_t sel_start = lv_label_get_text_selection_start(label);
    uint32_t sel_end = lv_label_get_text_selection_end(label);

    if (sel_start == LV_LABEL_TEXT_SELECTION_OFF || sel_end == LV_LABEL_TEXT_SELECTION_OFF) {
        return 0;
    }

    return (sel_start > sel_end) ? (sel_start - sel_end) : (sel_end - sel_start);
}

bool textarea_delete_selected_text(lv_obj_t* textarea, uint32_t* out_cursor_pos) {
    if (!textarea || !lv_obj_check_type(textarea, &lv_textarea_class)) return false;

    lv_obj_t* label = lv_textarea_get_label(textarea);
    if (!label) return false;

    uint32_t sel_start = lv_label_get_text_selection_start(label);
    uint32_t sel_end = lv_label_get_text_selection_end(label);

    if (sel_start == LV_LABEL_TEXT_SELECTION_OFF || sel_end == LV_LABEL_TEXT_SELECTION_OFF) {
        return false;
    }

    if (sel_start > sel_end) {
        uint32_t tmp = sel_start;
        sel_start = sel_end;
        sel_end = tmp;
    }

    uint32_t chars_to_delete = sel_end - sel_start;

    lv_label_set_text_selection_start(label, LV_LABEL_TEXT_SELECTION_OFF);
    lv_label_set_text_selection_end(label, LV_LABEL_TEXT_SELECTION_OFF);

    lv_textarea_set_cursor_pos(textarea, sel_end);
    for (uint32_t i = 0; i < chars_to_delete; i++) {
        lv_textarea_delete_char(textarea);
    }

    if (out_cursor_pos) {
        *out_cursor_pos = sel_start;
    }

    return true;
}

static size_t utf8_prefix_bytes(const char* text, size_t max_bytes) {
    size_t i = 0;
    while (text && text[i] != '\0' && i < max_bytes) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        size_t char_len = 1;

        if ((c & 0x80U) == 0) {
            char_len = 1;
        } else if ((c & 0xE0U) == 0xC0U) {
            char_len = 2;
        } else if ((c & 0xF0U) == 0xE0U) {
            char_len = 3;
        } else if ((c & 0xF8U) == 0xF0U) {
            char_len = 4;
        } else {
            break;
        }

        if (i + char_len > max_bytes) {
            break;
        }

        i += char_len;
    }

    return i;
}

bool textarea_add_text_with_limit(lv_obj_t* textarea, const char* text, size_t max_bytes) {
    if (!textarea || !lv_obj_check_type(textarea, &lv_textarea_class) || !text || text[0] == '\0') {
        return false;
    }

    if (max_bytes == 0) {
        lv_textarea_add_text(textarea, text);
        return true;
    }

    const char* current_text = lv_textarea_get_text(textarea);
    size_t current_len = current_text ? std::strlen(current_text) : 0;
    size_t selected_len = static_cast<size_t>(get_selection_length(textarea));
    size_t used_len = current_len >= selected_len ? (current_len - selected_len) : 0;

    if (used_len >= max_bytes) {
        return false;
    }

    size_t available = max_bytes - used_len;
    size_t insert_bytes = utf8_prefix_bytes(text, available);
    if (insert_bytes == 0) {
        return false;
    }

    textarea_delete_selected_text(textarea);

    if (text[insert_bytes] == '\0') {
        lv_textarea_add_text(textarea, text);
        return true;
    }

    std::string truncated(text, insert_bytes);
    lv_textarea_add_text(textarea, truncated.c_str());
    return true;
}
