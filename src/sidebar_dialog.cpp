#include "sidebar_dialog.h"
#include "sidebar.h"
#include "editor.h"
#include "theme.h"

static lv_obj_t* create_confirmation_dialog(
    lv_obj_t* screen,
    const Theme& theme,
    int width,
    int height,
    const char* message,
    const char* action_btn_text,
    lv_color_t action_btn_color,
    lv_obj_t** out_dialog_label,
    lv_obj_t** out_action_btn,
    lv_obj_t** out_cancel_btn
) {
    lv_obj_t* dialog = lv_obj_create(screen);
    lv_obj_set_size(dialog, width, height);
    lv_obj_center(dialog);
    lv_obj_set_style_bg_color(dialog, theme.sidebar_bg, 0);
    lv_obj_set_style_bg_opa(dialog, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dialog, 2, 0);
    lv_obj_set_style_border_color(dialog, theme.sidebar_btn_hover, 0);
    lv_obj_set_style_radius(dialog, 8, 0);
    lv_obj_set_style_pad_all(dialog, 15, 0);
    lv_obj_set_flex_flow(dialog, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(dialog, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    lv_obj_t* label = lv_label_create(dialog);
    lv_label_set_text(label, message);
    lv_obj_set_style_text_color(label, theme.sidebar_btn_text, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    *out_dialog_label = label;
    
    lv_obj_t* btn_row = lv_obj_create(dialog);
    lv_obj_remove_style_all(btn_row);
    lv_obj_set_size(btn_row, LV_PCT(100), 40);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    lv_obj_t* action_btn = lv_btn_create(btn_row);
    lv_obj_set_size(action_btn, 100, 35);
    lv_obj_set_style_bg_color(action_btn, action_btn_color, 0);
    lv_obj_set_style_radius(action_btn, 4, 0);
    
    lv_obj_t* action_label = lv_label_create(action_btn);
    lv_label_set_text(action_label, action_btn_text);
    lv_obj_center(action_label);
    lv_obj_set_style_text_color(action_label, theme.sidebar_btn_text, 0);
    lv_obj_set_style_text_font(action_label, &lv_font_montserrat_14, 0);
    *out_action_btn = action_btn;
    
    lv_obj_t* cancel_btn = lv_btn_create(btn_row);
    lv_obj_set_size(cancel_btn, 100, 35);
    lv_obj_set_style_bg_color(cancel_btn, theme.sidebar_btn_normal, 0);
    lv_obj_set_style_radius(cancel_btn, 4, 0);
    
    lv_obj_t* cancel_label = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_label, "Cancel");
    lv_obj_center(cancel_label);
    lv_obj_set_style_text_color(cancel_label, theme.sidebar_btn_text, 0);
    lv_obj_set_style_text_font(cancel_label, &lv_font_montserrat_14, 0);
    *out_cancel_btn = cancel_btn;
    
    return dialog;
}

void show_delete_dialog(SidebarState& sidebar, EditorState& editor, int index) {
    sidebar.folder_data.build_display_items(sidebar.searching);
    if (index < 0 || index >= (int)sidebar.folder_data.display_items.size()) return;
    
    const SidebarItem& item = sidebar.folder_data.display_items[index];
    
    sidebar.confirm_delete = true;
    sidebar.delete_index = index;
    sidebar.delete_item_name = item.name;
    sidebar.delete_item_folder = item.folder;
    sidebar.delete_item_is_folder = item.is_folder();
    sidebar.dialog_selection = 1;  // Default to Cancel
    
    const Theme& theme = get_theme(editor.dark_theme);
    
    if (sidebar.delete_dialog) {
        lv_obj_delete(sidebar.delete_dialog);
    }
    
    std::string display_name = item.name;
    if (item.is_file() && display_name.size() > 4 && display_name.substr(display_name.size() - 4) == ".txt") {
        display_name = display_name.substr(0, display_name.size() - 4);
    }
    
    std::string msg;
    if (item.is_folder()) {
        msg = "Delete folder \"" + display_name + "\"?\nAll files inside will be deleted.";
    } else {
        msg = "Delete \"" + display_name + "\"?";
    }
    
    sidebar.delete_dialog = create_confirmation_dialog(
        lv_scr_act(),
        theme,
        280, item.is_folder() ? 140 : 120,
        msg.c_str(),
        "Delete",
        theme.sidebar_delete,
        &sidebar.delete_dialog_label,
        &sidebar.delete_btn,
        &sidebar.cancel_btn
    );
    
    update_delete_dialog_selection(sidebar, editor);
}

void hide_delete_dialog(SidebarState& sidebar) {
    sidebar.confirm_delete = false;
    sidebar.delete_index = -1;
    sidebar.delete_item_name.clear();
    sidebar.delete_item_folder.clear();
    sidebar.delete_item_is_folder = false;
    
    if (sidebar.delete_dialog) {
        lv_obj_delete(sidebar.delete_dialog);
        sidebar.delete_dialog = nullptr;
        sidebar.delete_dialog_label = nullptr;
        sidebar.delete_btn = nullptr;
        sidebar.cancel_btn = nullptr;
    }
}

void update_delete_dialog_selection(SidebarState& sidebar, EditorState& editor) {
    if (!sidebar.delete_dialog || !sidebar.delete_btn || !sidebar.cancel_btn) return;
    
    if (sidebar.dialog_selection == 0) {
        lv_obj_set_style_border_width(sidebar.delete_btn, 2, 0);
        lv_obj_set_style_border_color(sidebar.delete_btn, lv_color_white(), 0);
        lv_obj_set_style_border_width(sidebar.cancel_btn, 0, 0);
    } else {
        lv_obj_set_style_border_width(sidebar.delete_btn, 0, 0);
        lv_obj_set_style_border_width(sidebar.cancel_btn, 2, 0);
        lv_obj_set_style_border_color(sidebar.cancel_btn, lv_color_white(), 0);
    }
}

void confirm_delete_action(SidebarState& sidebar, EditorState& editor) {
    if (!sidebar.confirm_delete) return;
    
    if (sidebar.dialog_selection == 0) {
        delete_file(sidebar, editor, sidebar.delete_index);
    }
    
    hide_delete_dialog(sidebar);
}

void show_restore_dialog(SidebarState& sidebar, EditorState& editor, int deleted_index) {
    if (deleted_index < 0 || deleted_index >= (int)sidebar.filtered_deleted_list.size()) return;
    
    sidebar.confirm_restore = true;
    sidebar.restore_index = deleted_index;
    sidebar.dialog_selection = 0;  // Default to Restore
    
    const Theme& theme = get_theme(editor.dark_theme);
    
    if (sidebar.restore_dialog) {
        lv_obj_delete(sidebar.restore_dialog);
    }
    
    sidebar.restore_dialog = create_confirmation_dialog(
        lv_scr_act(),
        theme,
        300, 130,
        "This file was deleted.\nWould you like to restore it?",
        "Restore",
        lv_color_hex(0x4a7c4a),
        &sidebar.restore_dialog_label,
        &sidebar.restore_btn,
        &sidebar.restore_cancel_btn
    );
    
    update_restore_dialog_selection(sidebar, editor);
}

void hide_restore_dialog(SidebarState& sidebar) {
    sidebar.confirm_restore = false;
    sidebar.restore_index = -1;
    
    if (sidebar.restore_dialog) {
        lv_obj_delete(sidebar.restore_dialog);
        sidebar.restore_dialog = nullptr;
        sidebar.restore_dialog_label = nullptr;
        sidebar.restore_btn = nullptr;
        sidebar.restore_cancel_btn = nullptr;
    }
}

void update_restore_dialog_selection(SidebarState& sidebar, EditorState& editor) {
    if (!sidebar.restore_dialog || !sidebar.restore_btn || !sidebar.restore_cancel_btn) return;
    
    if (sidebar.dialog_selection == 0) {
        lv_obj_set_style_border_width(sidebar.restore_btn, 2, 0);
        lv_obj_set_style_border_color(sidebar.restore_btn, lv_color_white(), 0);
        lv_obj_set_style_border_width(sidebar.restore_cancel_btn, 0, 0);
    } else {
        lv_obj_set_style_border_width(sidebar.restore_btn, 0, 0);
        lv_obj_set_style_border_width(sidebar.restore_cancel_btn, 2, 0);
        lv_obj_set_style_border_color(sidebar.restore_cancel_btn, lv_color_white(), 0);
    }
}

void confirm_restore_action(SidebarState& sidebar, EditorState& editor) {
    if (!sidebar.confirm_restore) return;
    
    if (sidebar.dialog_selection == 0) {
        restore_file(sidebar, editor, sidebar.restore_index);
    }
    
    hide_restore_dialog(sidebar);
}
