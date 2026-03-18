#include "sidebar_file_dialog.h"
#include "sidebar.h"
#include "editor.h"
#include "theme.h"
#include "sidebar_folder.h"
#include "platform.h"
#include <cstring>
#include <fstream>

static void build_dropdown_options(SidebarState& sidebar, std::string& options) {
    options = "(root)";
    for (const auto& folder : sidebar.folder_data.folders) {
        options += "\n";
        options += folder;
    }
}

static int get_folder_index(SidebarState& sidebar, const std::string& folder) {
    if (folder.empty()) return 0;
    for (int i = 0; i < (int)sidebar.folder_data.folders.size(); i++) {
        if (sidebar.folder_data.folders[i] == folder) {
            return i + 1;
        }
    }
    return 0;
}

static std::string get_folder_from_index(SidebarState& sidebar, int index) {
    if (index <= 0) return "";
    if (index > (int)sidebar.folder_data.folders.size()) return "";
    return sidebar.folder_data.folders[index - 1];
}

void show_file_dialog(SidebarState& sidebar, EditorState& editor, int mode) {
    const Theme& theme = get_theme(editor.dark_theme);
    
    sidebar.file_dialog_active = true;
    sidebar.file_dialog_mode = mode;
    sidebar.file_dialog_selection = 0;
    
    lv_obj_t* parent = lv_obj_get_parent(sidebar.sidebar_obj);
    
    sidebar.file_dialog = lv_obj_create(parent);
    lv_obj_remove_style_all(sidebar.file_dialog);
    lv_obj_set_size(sidebar.file_dialog, 300, 220);
    lv_obj_center(sidebar.file_dialog);
    lv_obj_set_style_bg_color(sidebar.file_dialog, theme.sidebar_bg, 0);
    lv_obj_set_style_bg_opa(sidebar.file_dialog, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(sidebar.file_dialog, 8, 0);
    lv_obj_set_style_border_width(sidebar.file_dialog, 1, 0);
    lv_obj_set_style_border_color(sidebar.file_dialog, theme.sidebar_btn_hover, 0);
    lv_obj_set_style_pad_left(sidebar.file_dialog, 16, 0);
    lv_obj_set_style_pad_right(sidebar.file_dialog, 16, 0);
    lv_obj_set_style_pad_top(sidebar.file_dialog, 16, 0);
    lv_obj_set_style_pad_bottom(sidebar.file_dialog, 24, 0);
    lv_obj_set_flex_flow(sidebar.file_dialog, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(sidebar.file_dialog, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(sidebar.file_dialog, 12, 0);
    
    lv_obj_t* title = lv_label_create(sidebar.file_dialog);
    const char* title_text = (mode == 0) ? "New File" : "Move / Rename";
    lv_label_set_text(title, title_text);
    lv_obj_set_style_text_color(title, theme.sidebar_btn_text, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    
    lv_obj_t* name_label = lv_label_create(sidebar.file_dialog);
    lv_label_set_text(name_label, "Name:");
    lv_obj_set_style_text_color(name_label, theme.text_dim, 0);
    lv_obj_set_style_text_font(name_label, &lv_font_montserrat_14, 0);
    
    sidebar.file_dialog_name_input = lv_textarea_create(sidebar.file_dialog);
    lv_obj_set_size(sidebar.file_dialog_name_input, LV_PCT(100), 36);
    lv_textarea_set_one_line(sidebar.file_dialog_name_input, true);
    lv_obj_set_style_bg_color(sidebar.file_dialog_name_input, theme.search_input_bg, 0);
    lv_obj_set_style_bg_opa(sidebar.file_dialog_name_input, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sidebar.file_dialog_name_input, 2, 0);
    lv_obj_set_style_border_color(sidebar.file_dialog_name_input, theme.sidebar_btn_hover, 0);
    lv_obj_set_style_radius(sidebar.file_dialog_name_input, 4, 0);
    lv_obj_set_style_text_color(sidebar.file_dialog_name_input, theme.sidebar_btn_text, 0);
    lv_obj_set_style_text_font(sidebar.file_dialog_name_input, &lv_font_montserrat_16, 0);
    lv_obj_set_style_pad_left(sidebar.file_dialog_name_input, 8, 0);
    lv_obj_set_style_outline_width(sidebar.file_dialog_name_input, 0, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(sidebar.file_dialog_name_input, 0, LV_STATE_FOCUS_KEY);
    lv_obj_set_style_border_color(sidebar.file_dialog_name_input, theme.cursor, LV_PART_CURSOR);
    lv_obj_set_style_border_color(sidebar.file_dialog_name_input, theme.cursor, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_border_opa(sidebar.file_dialog_name_input, LV_OPA_COVER, LV_PART_CURSOR);
    lv_obj_set_style_border_opa(sidebar.file_dialog_name_input, LV_OPA_COVER, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(sidebar.file_dialog_name_input, 2, LV_PART_CURSOR);
    lv_obj_set_style_border_side(sidebar.file_dialog_name_input, LV_BORDER_SIDE_LEFT, LV_PART_CURSOR);
    lv_obj_set_style_bg_opa(sidebar.file_dialog_name_input, LV_OPA_TRANSP, LV_PART_CURSOR);
    
    lv_obj_t* folder_label = lv_label_create(sidebar.file_dialog);
    lv_label_set_text(folder_label, "Folder:");
    lv_obj_set_style_text_color(folder_label, theme.text_dim, 0);
    lv_obj_set_style_text_font(folder_label, &lv_font_montserrat_14, 0);
    
    sidebar.file_dialog_folder_dropdown = lv_dropdown_create(sidebar.file_dialog);
    lv_obj_set_size(sidebar.file_dialog_folder_dropdown, LV_PCT(100), 36);
    lv_obj_set_style_bg_color(sidebar.file_dialog_folder_dropdown, theme.search_input_bg, 0);
    lv_obj_set_style_bg_opa(sidebar.file_dialog_folder_dropdown, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sidebar.file_dialog_folder_dropdown, 1, 0);
    lv_obj_set_style_border_color(sidebar.file_dialog_folder_dropdown, theme.sidebar_btn_hover, 0);
    lv_obj_set_style_radius(sidebar.file_dialog_folder_dropdown, 4, 0);
    lv_obj_set_style_text_color(sidebar.file_dialog_folder_dropdown, theme.sidebar_btn_text, 0);
    lv_obj_set_style_text_font(sidebar.file_dialog_folder_dropdown, &lv_font_montserrat_16, 0);
    lv_obj_set_style_pad_left(sidebar.file_dialog_folder_dropdown, 8, 0);
    
    std::string options;
    build_dropdown_options(sidebar, options);
    lv_dropdown_set_options(sidebar.file_dialog_folder_dropdown, options.c_str());
    
    std::string current_folder = get_current_folder(editor.current_file_path, editor.user_files_dir);
    int folder_index = get_folder_index(sidebar, current_folder);
    
    if (mode == 0) {
        lv_textarea_set_text(sidebar.file_dialog_name_input, "Untitled");
        lv_dropdown_set_selected(sidebar.file_dialog_folder_dropdown, folder_index);
    } else {
        // Get the selected item from display_items (which includes folders)
        if (sidebar.selected_index >= 0 && sidebar.selected_index < (int)sidebar.folder_data.display_items.size()) {
            const SidebarItem& item = sidebar.folder_data.display_items[sidebar.selected_index];
            if (item.is_file()) {
                std::string display_name = item.name;
                if (display_name.size() > 4 && display_name.substr(display_name.size() - 4) == ".txt") {
                    display_name = display_name.substr(0, display_name.size() - 4);
                }
                lv_textarea_set_text(sidebar.file_dialog_name_input, display_name.c_str());
                int entry_folder_index = get_folder_index(sidebar, item.folder);
                lv_dropdown_set_selected(sidebar.file_dialog_folder_dropdown, entry_folder_index);
            }
        }
    }
    
    lv_obj_t* spacer = lv_obj_create(sidebar.file_dialog);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_size(spacer, LV_PCT(100), 12);
    
    lv_obj_t* btn_container = lv_obj_create(sidebar.file_dialog);
    lv_obj_remove_style_all(btn_container);
    lv_obj_set_size(btn_container, LV_PCT(100), 36);
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(btn_container, 12, 0);
    
    sidebar.file_dialog_cancel_btn = lv_btn_create(btn_container);
    lv_obj_set_size(sidebar.file_dialog_cancel_btn, 80, 32);
    lv_obj_set_style_bg_color(sidebar.file_dialog_cancel_btn, theme.sidebar_btn_normal, 0);
    lv_obj_set_style_radius(sidebar.file_dialog_cancel_btn, 4, 0);
    lv_obj_t* cancel_label = lv_label_create(sidebar.file_dialog_cancel_btn);
    lv_label_set_text(cancel_label, "Cancel");
    lv_obj_set_style_text_color(cancel_label, theme.sidebar_btn_text, 0);
    lv_obj_set_style_text_font(cancel_label, &lv_font_montserrat_14, 0);
    lv_obj_center(cancel_label);
    
    sidebar.file_dialog_ok_btn = lv_btn_create(btn_container);
    lv_obj_set_size(sidebar.file_dialog_ok_btn, 80, 32);
    lv_obj_set_style_bg_color(sidebar.file_dialog_ok_btn, theme.sidebar_selected, 0);
    lv_obj_set_style_radius(sidebar.file_dialog_ok_btn, 4, 0);
    lv_obj_t* ok_label = lv_label_create(sidebar.file_dialog_ok_btn);
    lv_label_set_text(ok_label, "OK");
    lv_obj_set_style_text_color(ok_label, theme.sidebar_btn_text, 0);
    lv_obj_set_style_text_font(ok_label, &lv_font_montserrat_14, 0);
    lv_obj_center(ok_label);
    
    lv_obj_set_style_border_color(sidebar.file_dialog_name_input, lv_color_white(), 0);
    lv_textarea_set_cursor_pos(sidebar.file_dialog_name_input, LV_TEXTAREA_CURSOR_LAST);
}

void hide_file_dialog(SidebarState& sidebar) {
    if (sidebar.file_dialog) {
        lv_obj_delete(sidebar.file_dialog);
        sidebar.file_dialog = nullptr;
        sidebar.file_dialog_name_input = nullptr;
        sidebar.file_dialog_folder_dropdown = nullptr;
        sidebar.file_dialog_ok_btn = nullptr;
        sidebar.file_dialog_cancel_btn = nullptr;
    }
    sidebar.file_dialog_active = false;
    sidebar.file_dialog_close_time = lv_tick_get();
}

static void update_dialog_selection_ui(SidebarState& sidebar, EditorState& editor) {
    const Theme& theme = get_theme(editor.dark_theme);
    
    if (sidebar.file_dialog_name_input) {
        lv_obj_set_style_border_color(sidebar.file_dialog_name_input,
            sidebar.file_dialog_selection == 0 ? lv_color_white() : theme.sidebar_btn_hover, 0);
        if (sidebar.file_dialog_selection == 0) {
            lv_obj_add_state(sidebar.file_dialog_name_input, LV_STATE_FOCUSED);
            lv_obj_set_style_opa(sidebar.file_dialog_name_input, LV_OPA_COVER, LV_PART_CURSOR);
            lv_obj_clear_flag(sidebar.file_dialog_name_input, LV_OBJ_FLAG_HIDDEN);
            lv_textarea_set_accepted_chars(sidebar.file_dialog_name_input, nullptr);
        } else {
            lv_obj_clear_state(sidebar.file_dialog_name_input, LV_STATE_FOCUSED);
            lv_obj_set_style_opa(sidebar.file_dialog_name_input, LV_OPA_TRANSP, LV_PART_CURSOR);
            lv_textarea_set_accepted_chars(sidebar.file_dialog_name_input, "");
        }
    }
    if (sidebar.file_dialog_folder_dropdown) {
        lv_obj_set_style_border_width(sidebar.file_dialog_folder_dropdown,
            sidebar.file_dialog_selection == 1 ? 2 : 1, 0);
        lv_obj_set_style_border_color(sidebar.file_dialog_folder_dropdown,
            sidebar.file_dialog_selection == 1 ? lv_color_white() : theme.sidebar_btn_hover, 0);
        
        if (sidebar.file_dialog_selection == 1) {
            lv_dropdown_open(sidebar.file_dialog_folder_dropdown);
            lv_obj_t* list = lv_dropdown_get_list(sidebar.file_dialog_folder_dropdown);
            if (list) {
                lv_obj_set_style_bg_color(list, theme.sidebar_bg, 0);
                lv_obj_set_style_bg_opa(list, LV_OPA_COVER, 0);
                lv_obj_set_style_border_color(list, theme.sidebar_btn_hover, 0);
                lv_obj_set_style_border_width(list, 1, 0);
                lv_obj_set_style_text_color(list, theme.sidebar_btn_text, 0);
                lv_obj_set_style_text_font(list, &lv_font_montserrat_16, 0);
                lv_obj_set_style_bg_color(list, theme.sidebar_selected, LV_PART_SELECTED);
                lv_obj_set_style_text_color(list, theme.sidebar_btn_text, LV_PART_SELECTED);
                lv_dropdown_close(sidebar.file_dialog_folder_dropdown);
                lv_dropdown_open(sidebar.file_dialog_folder_dropdown);
            }
        } else {
            lv_dropdown_close(sidebar.file_dialog_folder_dropdown);
        }
    }
    if (sidebar.file_dialog_ok_btn) {
        lv_obj_set_style_border_width(sidebar.file_dialog_ok_btn,
            sidebar.file_dialog_selection == 2 ? 2 : 0, 0);
        lv_obj_set_style_border_color(sidebar.file_dialog_ok_btn, lv_color_white(), 0);
    }
    if (sidebar.file_dialog_cancel_btn) {
        lv_obj_set_style_border_width(sidebar.file_dialog_cancel_btn,
            sidebar.file_dialog_selection == 3 ? 2 : 0, 0);
        lv_obj_set_style_border_color(sidebar.file_dialog_cancel_btn, lv_color_white(), 0);
    }
}

void handle_file_dialog_keyboard(SidebarState& sidebar, EditorState& editor) {
    static uint32_t last_nav_time = 0;
    static uint32_t dialog_open_time = 0;
    uint32_t now = lv_tick_get();
    
    if (dialog_open_time == 0 || !sidebar.file_dialog_active) {
        dialog_open_time = now;
    }
    
    if (now - dialog_open_time < 200) {
        return;
    }
    
    bool nav_debounce = (now - last_nav_time) > 120;
    
    if (!nav_debounce) return;
    
    if (platform::is_key_pressed(platform::KeyCode::Escape)) {
        last_nav_time = now;
        dialog_open_time = 0;
        hide_file_dialog(sidebar);
        return;
    }
    
    if (platform::is_key_pressed(platform::KeyCode::Enter)) {
        last_nav_time = now;
        dialog_open_time = 0;
        if (sidebar.file_dialog_selection == 3) {
            hide_file_dialog(sidebar);
        } else {
            confirm_file_dialog(sidebar, editor);
        }
        return;
    }
    
    if (platform::is_key_pressed(platform::KeyCode::Up)) {
        last_nav_time = now;
        if (sidebar.file_dialog_selection == 1) {
            uint32_t sel = lv_dropdown_get_selected(sidebar.file_dialog_folder_dropdown);
            if (sel > 0) {
                lv_dropdown_set_selected(sidebar.file_dialog_folder_dropdown, sel - 1);
            } else {
                sidebar.file_dialog_selection = 0;
                update_dialog_selection_ui(sidebar, editor);
            }
        } else if (sidebar.file_dialog_selection > 0) {
            sidebar.file_dialog_selection--;
            update_dialog_selection_ui(sidebar, editor);
        }
        return;
    }
    
    if (platform::is_key_pressed(platform::KeyCode::Down)) {
        last_nav_time = now;
        if (sidebar.file_dialog_selection == 1) {
            uint32_t sel = lv_dropdown_get_selected(sidebar.file_dialog_folder_dropdown);
            uint32_t count = lv_dropdown_get_option_count(sidebar.file_dialog_folder_dropdown);
            if (sel < count - 1) {
                lv_dropdown_set_selected(sidebar.file_dialog_folder_dropdown, sel + 1);
            }
        } else if (sidebar.file_dialog_selection < 3) {
            sidebar.file_dialog_selection++;
            update_dialog_selection_ui(sidebar, editor);
        }
        return;
    }
    
    if (sidebar.file_dialog_selection == 1) {
        if (platform::is_key_pressed(platform::KeyCode::Left)) {
            last_nav_time = now;
            uint32_t sel = lv_dropdown_get_selected(sidebar.file_dialog_folder_dropdown);
            if (sel > 0) {
                lv_dropdown_set_selected(sidebar.file_dialog_folder_dropdown, sel - 1);
            }
            return;
        }
        if (platform::is_key_pressed(platform::KeyCode::Right)) {
            last_nav_time = now;
            uint32_t sel = lv_dropdown_get_selected(sidebar.file_dialog_folder_dropdown);
            uint32_t count = lv_dropdown_get_option_count(sidebar.file_dialog_folder_dropdown);
            if (sel < count - 1) {
                lv_dropdown_set_selected(sidebar.file_dialog_folder_dropdown, sel + 1);
            }
            return;
        }
    }
    
    if (sidebar.file_dialog_selection == 0 || sidebar.file_dialog_selection == 2 || sidebar.file_dialog_selection == 3) {
        if (platform::is_key_pressed(platform::KeyCode::Left)) {
            last_nav_time = now;
            if (sidebar.file_dialog_selection == 3) {
                sidebar.file_dialog_selection = 2;
                update_dialog_selection_ui(sidebar, editor);
            }
            return;
        }
        if (platform::is_key_pressed(platform::KeyCode::Right)) {
            last_nav_time = now;
            if (sidebar.file_dialog_selection == 2) {
                sidebar.file_dialog_selection = 3;
                update_dialog_selection_ui(sidebar, editor);
            }
            return;
        }
    }
}

void confirm_file_dialog(SidebarState& sidebar, EditorState& editor) {
    const char* name = lv_textarea_get_text(sidebar.file_dialog_name_input);
    if (!name || strlen(name) == 0) {
        hide_file_dialog(sidebar);
        return;
    }
    
    uint32_t folder_idx = lv_dropdown_get_selected(sidebar.file_dialog_folder_dropdown);
    std::string folder = get_folder_from_index(sidebar, folder_idx);
    
    if (sidebar.file_dialog_mode == 0) {
        std::string filename = name;
        if (filename.size() < 4 || filename.substr(filename.size() - 4) != ".txt") {
            filename += ".txt";
        }
        
        std::string new_path;
        if (folder.empty()) {
            new_path = editor.user_files_dir + "/" + filename;
        } else {
            new_path = editor.user_files_dir + "/" + folder + "/" + filename;
        }
        
        int counter = 1;
        std::string base_name = filename.substr(0, filename.size() - 4);
        while (true) {
            std::ifstream test(new_path);
            if (!test.good()) break;
            test.close();
            counter++;
            filename = base_name + " " + std::to_string(counter) + ".txt";
            if (folder.empty()) {
                new_path = editor.user_files_dir + "/" + filename;
            } else {
                new_path = editor.user_files_dir + "/" + folder + "/" + filename;
            }
        }
        
        std::ofstream new_file(new_path);
        new_file.close();
        
        save_editor_content(editor);
        
        scan_folders_and_files(sidebar.folder_data, editor.user_files_dir);
        filter_folder_entries(sidebar.folder_data, std::string(sidebar.search_buffer, sidebar.search_len));
        sidebar.folder_data.build_display_items(sidebar.searching);
        
        // Find and select the new file in the updated list
        for (int i = 0; i < (int)sidebar.folder_data.display_items.size(); i++) {
            const SidebarItem& di = sidebar.folder_data.display_items[i];
            if (di.is_file() && di.name == filename && di.folder == folder) {
                sidebar.selected_index = i;
                break;
            }
        }
        
        refresh_file_list_ui(sidebar, editor);
        
        load_file_into_editor(editor, new_path.c_str());
        hide_file_dialog(sidebar);
    } else {
        // Get the selected item from display_items (which includes folders)
        if (sidebar.selected_index >= 0 && sidebar.selected_index < (int)sidebar.folder_data.display_items.size()) {
            const SidebarItem& item = sidebar.folder_data.display_items[sidebar.selected_index];
            if (!item.is_file()) {
                hide_file_dialog(sidebar);
                return;
            }
            
            // Create a SidebarFileEntry from the SidebarItem
            SidebarFileEntry entry;
            entry.filename = item.name;
            entry.folder = item.folder;
            
            std::string old_path;
            if (entry.folder.empty()) {
                old_path = editor.user_files_dir + "/" + entry.filename;
            } else {
                old_path = editor.user_files_dir + "/" + entry.folder + "/" + entry.filename;
            }
            
            bool was_current_file = (editor.current_file_path == old_path);
            
            if (move_file(editor.user_files_dir, entry, name, folder)) {
                std::string new_filename = name;
                if (new_filename.size() < 4 || new_filename.substr(new_filename.size() - 4) != ".txt") {
                    new_filename += ".txt";
                }
                
                std::string new_path;
                if (folder.empty()) {
                    new_path = editor.user_files_dir + "/" + new_filename;
                } else {
                    new_path = editor.user_files_dir + "/" + folder + "/" + new_filename;
                }
                
                if (was_current_file) {
                    editor.current_file_path = new_path;
                    editor.temp_file_path = new_path + ".tmp";
                    update_filename_display(editor);
                }
                
                scan_folders_and_files(sidebar.folder_data, editor.user_files_dir);
                filter_folder_entries(sidebar.folder_data, std::string(sidebar.search_buffer, sidebar.search_len));
                
                // Find and select the moved file in the updated list
                sidebar.folder_data.build_display_items(sidebar.searching);
                for (int i = 0; i < (int)sidebar.folder_data.display_items.size(); i++) {
                    const SidebarItem& di = sidebar.folder_data.display_items[i];
                    if (di.is_file() && di.name == new_filename && di.folder == folder) {
                        sidebar.selected_index = i;
                        break;
                    }
                }
                
                refresh_file_list_ui(sidebar, editor);
            }
        }
        hide_file_dialog(sidebar);
    }
}
