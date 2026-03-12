#include "sidebar_file.h"
#include "sidebar.h"
#include "theme.h"

lv_obj_t* create_file_item_ui(
    SidebarState& sidebar,
    EditorState& editor,
    lv_obj_t* parent,
    const SidebarItem& item,
    int index,
    bool is_selected,
    const Theme& theme,
    lv_event_cb_t click_cb
) {
    lv_obj_t* file_item = lv_obj_create(parent);
    lv_obj_remove_style_all(file_item);
    lv_obj_set_size(file_item, LV_PCT(100), 52);
    lv_obj_set_user_data(file_item, (void*)(intptr_t)index);
    lv_obj_add_flag(file_item, LV_OBJ_FLAG_CLICKABLE);
    
    lv_obj_set_style_bg_color(file_item, 
        is_selected ? theme.sidebar_selected : theme.sidebar_btn_normal, 0);
    lv_obj_set_style_bg_opa(file_item, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(file_item, 0, 0);
    lv_obj_add_event_cb(file_item, click_cb, LV_EVENT_CLICKED, nullptr);
    
    int left_pad = 10;
    if (!item.folder.empty() && !sidebar.searching) {
        lv_obj_set_style_margin_left(file_item, 24, 0);
    }
    
    if (!sidebar.searching && sidebar.renaming && index == sidebar.rename_file_index) {
        lv_obj_t* ta = lv_textarea_create(file_item);
        lv_obj_set_size(ta, LV_PCT(100) - 16, 28);
        lv_obj_align(ta, LV_ALIGN_LEFT_MID, left_pad - 4, 0);
        lv_textarea_set_one_line(ta, true);
        lv_textarea_set_text(ta, sidebar.rename_buffer);
        lv_obj_scroll_to_x(ta, 0, LV_ANIM_OFF);
        lv_textarea_set_cursor_pos(ta, LV_TEXTAREA_CURSOR_LAST);
        lv_obj_set_style_bg_color(ta, theme.sidebar_selected, 0);
        lv_obj_set_style_bg_opa(ta, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(ta, 0, 0);
        lv_obj_set_style_border_width(ta, 0, LV_STATE_FOCUSED);
        lv_obj_set_style_border_width(ta, 0, LV_STATE_FOCUS_KEY);
        lv_obj_set_style_outline_width(ta, 0, 0);
        lv_obj_set_style_outline_width(ta, 0, LV_STATE_FOCUSED);
        lv_obj_set_style_outline_width(ta, 0, LV_STATE_FOCUS_KEY);
        lv_obj_set_style_text_color(ta, theme.sidebar_btn_text, 0);
        lv_obj_set_style_text_font(ta, &lv_font_montserrat_16, 0);
        lv_obj_set_style_pad_left(ta, 4, 0);
        lv_obj_set_style_pad_right(ta, 4, 0);
        lv_obj_set_style_border_color(ta, theme.cursor, LV_PART_CURSOR);
        lv_obj_set_style_border_color(ta, theme.cursor, LV_PART_CURSOR | LV_STATE_FOCUSED);
        lv_obj_set_style_border_opa(ta, LV_OPA_COVER, LV_PART_CURSOR);
        lv_obj_set_style_border_opa(ta, LV_OPA_COVER, LV_PART_CURSOR | LV_STATE_FOCUSED);
        lv_obj_set_style_border_width(ta, 2, LV_PART_CURSOR);
        lv_obj_set_style_border_side(ta, LV_BORDER_SIDE_LEFT, LV_PART_CURSOR);
        lv_obj_set_style_bg_opa(ta, LV_OPA_TRANSP, LV_PART_CURSOR);
        lv_obj_set_style_bg_opa(ta, LV_OPA_TRANSP, LV_PART_CURSOR | LV_STATE_FOCUSED);
        sidebar.rename_textarea = ta;
    } else {
        // File name (top line)
        lv_obj_t* name_label = lv_label_create(file_item);
        
        std::string display_name = item.name;
        if (display_name.size() > 4 && display_name.substr(display_name.size() - 4) == ".txt") {
            display_name = display_name.substr(0, display_name.size() - 4);
        }
        
        lv_label_set_text(name_label, display_name.c_str());
        lv_label_set_long_mode(name_label, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_color(name_label, theme.sidebar_btn_text, 0);
        lv_obj_set_style_text_font(name_label, &lv_font_montserrat_16, 0);
        lv_obj_set_pos(name_label, left_pad, 8);
        lv_obj_set_width(name_label, LV_PCT(100) - left_pad - 10);
        
        // Date and snippet (bottom line)
        lv_obj_t* subtitle_label = lv_label_create(file_item);
        
        std::string subtitle;
        if (!item.date_str.empty()) {
            subtitle = item.date_str;
            if (!item.snippet.empty()) {
                subtitle += "  " + item.snippet;
            }
        } else if (!item.snippet.empty()) {
            subtitle = item.snippet;
        }
        
        lv_label_set_text(subtitle_label, subtitle.c_str());
        lv_label_set_long_mode(subtitle_label, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_color(subtitle_label, theme.text_dim, 0);
        lv_obj_set_style_text_font(subtitle_label, &lv_font_montserrat_14, 0);
        lv_obj_set_pos(subtitle_label, left_pad, 28);
        lv_obj_set_width(subtitle_label, LV_PCT(100) - left_pad - 10);
    }
    
    return file_item;
}

lv_obj_t* create_deleted_file_item_ui(
    lv_obj_t* parent,
    const std::string& filename,
    int index,
    bool is_selected,
    const Theme& theme
) {
    lv_obj_t* file_item = lv_obj_create(parent);
    lv_obj_remove_style_all(file_item);
    lv_obj_set_size(file_item, LV_PCT(100), 36);
    lv_obj_set_user_data(file_item, (void*)(intptr_t)index);
    lv_obj_add_flag(file_item, LV_OBJ_FLAG_CLICKABLE);
    
    lv_obj_set_style_bg_color(file_item, 
        is_selected ? theme.sidebar_selected : theme.sidebar_btn_normal, 0);
    lv_obj_set_style_bg_opa(file_item, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(file_item, 0, 0);
    
    lv_obj_t* label = lv_label_create(file_item);
    std::string display_name = filename;
    if (display_name.size() > 4 && display_name.substr(display_name.size() - 4) == ".txt") {
        display_name = display_name.substr(0, display_name.size() - 4);
    }
    lv_label_set_text(label, display_name.c_str());
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(label, theme.text_dim, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 10, 0);
    
    return file_item;
}
