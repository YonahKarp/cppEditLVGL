#include "sidebar.h"
#include "sidebar_dialog.h"
#include "editor.h"
#include "theme.h"
#include "file_manager.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <dirent.h>

static EditorState* g_editor = nullptr;
static SidebarState* g_sidebar = nullptr;

static void file_btn_click_cb(lv_event_t* e) {
    if (!g_editor || !g_sidebar) return;
    
    lv_obj_t* btn = static_cast<lv_obj_t*>(lv_event_get_target(e));
    int index = (int)(intptr_t)lv_obj_get_user_data(btn);
    
    if (index >= 0 && index < (int)g_sidebar->filtered_file_list.size()) {
        switch_to_file(*g_sidebar, *g_editor, g_sidebar->filtered_file_list[index]);
        hide_sidebar(*g_sidebar, *g_editor);
    }
}

static void new_file_btn_click_cb(lv_event_t* e) {
    if (!g_editor || !g_sidebar) return;
    create_new_file(*g_sidebar, *g_editor);
}

static void search_input_cb(lv_event_t* e) {
    if (!g_editor || !g_sidebar) return;
    
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        const char* text = lv_textarea_get_text(g_sidebar->search_input);
        strncpy(g_sidebar->search_buffer, text, sizeof(g_sidebar->search_buffer) - 1);
        g_sidebar->search_len = strlen(g_sidebar->search_buffer);
        filter_sidebar_files(*g_sidebar);
        refresh_file_list_ui(*g_sidebar, *g_editor);
    }
}

void create_sidebar_ui(SidebarState& sidebar, EditorState& editor, lv_obj_t* parent) {
    g_editor = &editor;
    g_sidebar = &sidebar;
    
    const Theme& theme = get_theme(editor.dark_theme);
    
    sidebar.sidebar_obj = lv_obj_create(parent);
    lv_obj_remove_style_all(sidebar.sidebar_obj);
    lv_obj_set_size(sidebar.sidebar_obj, SidebarState::SIDEBAR_WIDTH, LV_PCT(100));
    lv_obj_align(sidebar.sidebar_obj, LV_ALIGN_LEFT_MID, -SidebarState::SIDEBAR_WIDTH, 0);
    lv_obj_set_style_bg_color(sidebar.sidebar_obj, theme.sidebar_bg, 0);
    lv_obj_set_style_bg_opa(sidebar.sidebar_obj, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(sidebar.sidebar_obj, 10, 0);
    lv_obj_set_flex_flow(sidebar.sidebar_obj, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(sidebar.sidebar_obj, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(sidebar.sidebar_obj, LV_OBJ_FLAG_HIDDEN);
    
    lv_obj_t* header = lv_obj_create(sidebar.sidebar_obj);
    lv_obj_remove_style_all(header);
    lv_obj_set_size(header, LV_PCT(100), 40);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "Files");
    lv_obj_set_style_text_color(title, theme.sidebar_btn_text, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    
    sidebar.new_file_btn = lv_btn_create(header);
    lv_obj_set_size(sidebar.new_file_btn, 36, 36);
    lv_obj_set_style_bg_color(sidebar.new_file_btn, theme.sidebar_new_file, 0);
    lv_obj_set_style_radius(sidebar.new_file_btn, 4, 0);
    lv_obj_add_event_cb(sidebar.new_file_btn, new_file_btn_click_cb, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t* plus_icon = lv_label_create(sidebar.new_file_btn);
    lv_label_set_text(plus_icon, LV_SYMBOL_PLUS);
    lv_obj_center(plus_icon);
    lv_obj_set_style_text_color(plus_icon, theme.sidebar_btn_text, 0);
    
    // Search container - hidden by default, shown with Ctrl+F
    sidebar.search_container = lv_obj_create(sidebar.sidebar_obj);
    lv_obj_remove_style_all(sidebar.search_container);
    lv_obj_set_size(sidebar.search_container, LV_PCT(100), 36);
    lv_obj_set_style_bg_color(sidebar.search_container, theme.search_input_bg, 0);
    lv_obj_set_style_bg_opa(sidebar.search_container, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(sidebar.search_container, 4, 0);
    lv_obj_set_style_border_width(sidebar.search_container, 1, 0);
    lv_obj_set_style_border_color(sidebar.search_container, theme.sidebar_btn_hover, 0);
    lv_obj_set_style_pad_hor(sidebar.search_container, 8, 0);
    lv_obj_set_flex_flow(sidebar.search_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sidebar.search_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(sidebar.search_container, LV_OBJ_FLAG_HIDDEN);
    
    lv_obj_t* search_icon = lv_label_create(sidebar.search_container);
    lv_label_set_text(search_icon, LV_SYMBOL_EYE_OPEN);
    lv_obj_set_style_text_color(search_icon, theme.text_dim, 0);
    lv_obj_set_style_text_font(search_icon, &lv_font_montserrat_14, 0);
    
    sidebar.search_input = lv_textarea_create(sidebar.search_container);
    lv_obj_set_size(sidebar.search_input, LV_PCT(100), 28);
    lv_obj_set_flex_grow(sidebar.search_input, 1);
    lv_textarea_set_one_line(sidebar.search_input, true);
    lv_textarea_set_placeholder_text(sidebar.search_input, "Search files...");
    lv_obj_set_style_bg_opa(sidebar.search_input, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sidebar.search_input, 0, 0);
    lv_obj_set_style_text_color(sidebar.search_input, theme.search_input_text, 0);
    lv_obj_set_style_text_font(sidebar.search_input, &lv_font_montserrat_16, 0);
    lv_obj_add_event_cb(sidebar.search_input, search_input_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    
    sidebar.file_list = lv_obj_create(sidebar.sidebar_obj);
    lv_obj_remove_style_all(sidebar.file_list);
    lv_obj_set_size(sidebar.file_list, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_grow(sidebar.file_list, 1);
    lv_obj_set_flex_flow(sidebar.file_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(sidebar.file_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(sidebar.file_list, 8, 0);
    lv_obj_set_style_pad_top(sidebar.file_list, 12, 0);
    lv_obj_set_style_pad_bottom(sidebar.file_list, 12, 0);
    lv_obj_add_flag(sidebar.file_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(sidebar.file_list, theme.sidebar_bg, 0);
    lv_obj_set_style_bg_opa(sidebar.file_list, LV_OPA_COVER, 0);
    lv_obj_set_scroll_dir(sidebar.file_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(sidebar.file_list, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_width(sidebar.file_list, 4, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(sidebar.file_list, theme.text_dim, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(sidebar.file_list, LV_OPA_50, LV_PART_SCROLLBAR);
    lv_obj_clear_flag(sidebar.file_list, LV_OBJ_FLAG_SCROLL_CHAIN);
}

static void sidebar_anim_cb(void* var, int32_t v) {
    lv_obj_t* obj = (lv_obj_t*)var;
    lv_obj_set_x(obj, v);
}

void toggle_sidebar(SidebarState& sidebar, EditorState& editor) {
    if (sidebar.visible) {
        hide_sidebar(sidebar, editor);
    } else {
        show_sidebar(sidebar, editor);
    }
}

void show_sidebar(SidebarState& sidebar, EditorState& editor) {
    sidebar.visible = true;
    sidebar.selected_index = 0;
    sidebar.new_file_selected = false;
    sidebar.searching = false;
    sidebar.search_buffer[0] = '\0';
    sidebar.search_len = 0;
    sidebar.renaming = false;
    sidebar.rename_pending = false;
    
    // Hide search container by default
    lv_obj_add_flag(sidebar.search_container, LV_OBJ_FLAG_HIDDEN);
    
    // Hide cursor in editor while sidebar is open
    if (editor.textarea) {
        lv_obj_set_style_border_opa(editor.textarea, LV_OPA_TRANSP, LV_PART_CURSOR);
        lv_obj_set_style_border_opa(editor.textarea, LV_OPA_TRANSP, LV_PART_CURSOR | LV_STATE_FOCUSED);
    }
    
    scan_txt_files(sidebar, editor.user_files_dir);
    scan_deleted_files(sidebar, editor.recently_deleted_dir);
    filter_sidebar_files(sidebar);
    refresh_file_list_ui(sidebar, editor);
    
    lv_obj_clear_flag(sidebar.sidebar_obj, LV_OBJ_FLAG_HIDDEN);
    
    sidebar.animating = true;
    sidebar.anim_start_time = lv_tick_get();
    
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, sidebar.sidebar_obj);
    lv_anim_set_values(&a, -SidebarState::SIDEBAR_WIDTH, 0);
    lv_anim_set_duration(&a, SidebarState::ANIM_DURATION_MS);
    lv_anim_set_exec_cb(&a, sidebar_anim_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

static void hide_anim_ready_cb(lv_anim_t* a) {
    lv_obj_t* obj = (lv_obj_t*)a->var;
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

void hide_sidebar(SidebarState& sidebar, EditorState& editor) {
    sidebar.visible = false;
    sidebar.searching = false;
    sidebar.renaming = false;
    sidebar.rename_pending = false;
    
    // Restore cursor in editor
    if (editor.textarea) {
        lv_obj_set_style_border_opa(editor.textarea, LV_OPA_COVER, LV_PART_CURSOR);
        lv_obj_set_style_border_opa(editor.textarea, LV_OPA_COVER, LV_PART_CURSOR | LV_STATE_FOCUSED);
    }
    
    sidebar.animating = true;
    sidebar.anim_start_time = lv_tick_get();
    
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, sidebar.sidebar_obj);
    lv_anim_set_values(&a, 0, -SidebarState::SIDEBAR_WIDTH);
    lv_anim_set_duration(&a, SidebarState::ANIM_DURATION_MS);
    lv_anim_set_exec_cb(&a, sidebar_anim_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_set_completed_cb(&a, hide_anim_ready_cb);
    lv_anim_start(&a);
}

void update_sidebar_theme(SidebarState& sidebar, bool dark) {
    const Theme& theme = get_theme(dark);
    
    if (sidebar.sidebar_obj) {
        lv_obj_set_style_bg_color(sidebar.sidebar_obj, theme.sidebar_bg, 0);
    }
    
    if (sidebar.new_file_btn) {
        lv_obj_set_style_bg_color(sidebar.new_file_btn, theme.sidebar_new_file, 0);
    }
    
    if (sidebar.search_container) {
        lv_obj_set_style_bg_color(sidebar.search_container, theme.search_input_bg, 0);
        lv_obj_set_style_border_color(sidebar.search_container, theme.sidebar_btn_hover, 0);
    }
    
    if (sidebar.search_input) {
        lv_obj_set_style_text_color(sidebar.search_input, theme.search_input_text, 0);
    }
    
    if (sidebar.file_list) {
        lv_obj_set_style_bg_color(sidebar.file_list, theme.sidebar_bg, 0);
        lv_obj_set_style_bg_color(sidebar.file_list, theme.text_dim, LV_PART_SCROLLBAR);
    }
}

void scan_txt_files(SidebarState& sidebar, const std::string& user_files_dir) {
    sidebar.file_list_items.clear();
    
    DIR* dir = opendir(user_files_dir.c_str());
    if (!dir) return;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name.size() > 4 && name.substr(name.size() - 4) == ".txt") {
            sidebar.file_list_items.push_back(name);
        }
    }
    closedir(dir);
    
    std::sort(sidebar.file_list_items.begin(), sidebar.file_list_items.end(),
              [](const std::string& a, const std::string& b) {
                  std::string la = a, lb = b;
                  std::transform(la.begin(), la.end(), la.begin(), ::tolower);
                  std::transform(lb.begin(), lb.end(), lb.begin(), ::tolower);
                  return la < lb;
              });
}

void scan_deleted_files(SidebarState& sidebar, const std::string& recently_deleted_dir) {
    sidebar.deleted_file_list.clear();
    
    DIR* dir = opendir(recently_deleted_dir.c_str());
    if (!dir) return;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name.size() > 4 && name.substr(name.size() - 4) == ".txt") {
            sidebar.deleted_file_list.push_back(name);
        }
    }
    closedir(dir);
    
    std::sort(sidebar.deleted_file_list.begin(), sidebar.deleted_file_list.end(),
              [](const std::string& a, const std::string& b) {
                  std::string la = a, lb = b;
                  std::transform(la.begin(), la.end(), la.begin(), ::tolower);
                  std::transform(lb.begin(), lb.end(), lb.begin(), ::tolower);
                  return la < lb;
              });
}

static bool case_insensitive_contains(const std::string& str, const std::string& substr) {
    if (substr.empty()) return true;
    std::string str_lower = str;
    std::string substr_lower = substr;
    for (auto& c : str_lower) c = std::tolower(c);
    for (auto& c : substr_lower) c = std::tolower(c);
    return str_lower.find(substr_lower) != std::string::npos;
}

void filter_sidebar_files(SidebarState& sidebar) {
    sidebar.filtered_file_list.clear();
    sidebar.filtered_deleted_list.clear();
    
    std::string query(sidebar.search_buffer, sidebar.search_len);
    
    for (const auto& filename : sidebar.file_list_items) {
        if (case_insensitive_contains(filename, query)) {
            sidebar.filtered_file_list.push_back(filename);
        }
    }
    
    // Also filter deleted files when searching
    for (const auto& filename : sidebar.deleted_file_list) {
        if (case_insensitive_contains(filename, query)) {
            sidebar.filtered_deleted_list.push_back(filename);
        }
    }
    
    sidebar.selected_index = 0;
    sidebar.new_file_selected = false;
}

void refresh_file_list_ui(SidebarState& sidebar, EditorState& editor) {
    const Theme& theme = get_theme(editor.dark_theme);
    
    lv_obj_clean(sidebar.file_list);
    
    int regular_count = (int)sidebar.filtered_file_list.size();
    int deleted_count = sidebar.searching ? (int)sidebar.filtered_deleted_list.size() : 0;
    int total_count = regular_count + deleted_count;
    
    if (sidebar.selected_index >= total_count) {
        sidebar.selected_index = total_count > 0 ? total_count - 1 : 0;
    }
    
    // Update + button highlighting
    if (sidebar.new_file_btn) {
        if (sidebar.new_file_selected) {
            lv_obj_set_style_border_width(sidebar.new_file_btn, 2, 0);
            lv_obj_set_style_border_color(sidebar.new_file_btn, lv_color_white(), 0);
        } else {
            lv_obj_set_style_border_width(sidebar.new_file_btn, 0, 0);
        }
    }
    
    lv_obj_t* selected_btn = nullptr;
    
    // Regular files
    for (int i = 0; i < (int)sidebar.filtered_file_list.size(); i++) {
        const std::string& filename = sidebar.filtered_file_list[i];
        
        lv_obj_t* file_btn = lv_btn_create(sidebar.file_list);
        lv_obj_set_size(file_btn, LV_PCT(100), 36);
        lv_obj_set_user_data(file_btn, (void*)(intptr_t)i);
        
        bool is_selected = (!sidebar.new_file_selected && i == sidebar.selected_index);
        if (is_selected) {
            selected_btn = file_btn;
        }
        lv_obj_set_style_bg_color(file_btn, 
            is_selected ? theme.sidebar_selected : theme.sidebar_btn_normal, 0);
        lv_obj_set_style_bg_color(file_btn, theme.sidebar_btn_hover, LV_STATE_PRESSED);
        lv_obj_set_style_radius(file_btn, 4, 0);
        lv_obj_add_event_cb(file_btn, file_btn_click_cb, LV_EVENT_CLICKED, nullptr);
        
        // Show textarea if renaming this file, otherwise show label
        if (!sidebar.searching && sidebar.renaming && i == sidebar.rename_file_index) {
            lv_obj_t* ta = lv_textarea_create(file_btn);
            lv_obj_set_size(ta, LV_PCT(100) - 16, 28);
            lv_obj_align(ta, LV_ALIGN_LEFT_MID, 4, 0);
            lv_textarea_set_one_line(ta, true);
            lv_textarea_set_text(ta, sidebar.rename_buffer);
            lv_textarea_set_cursor_pos(ta, LV_TEXTAREA_CURSOR_LAST);
            lv_obj_set_style_bg_color(ta, theme.sidebar_selected, 0);
            lv_obj_set_style_bg_opa(ta, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(ta, 0, 0);
            lv_obj_set_style_text_color(ta, theme.sidebar_btn_text, 0);
            lv_obj_set_style_text_font(ta, &lv_font_montserrat_16, 0);
            lv_obj_set_style_pad_left(ta, 4, 0);
            lv_obj_set_style_pad_right(ta, 4, 0);
            // Cursor styling using theme colors
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
            lv_obj_t* label = lv_label_create(file_btn);
            
            std::string display_name = filename;
            if (display_name.size() > 4 && display_name.substr(display_name.size() - 4) == ".txt") {
                display_name = display_name.substr(0, display_name.size() - 4);
            }
            
            lv_label_set_text(label, display_name.c_str());
            lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
            lv_obj_set_style_text_color(label, theme.sidebar_btn_text, 0);
            lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
            lv_obj_align(label, LV_ALIGN_LEFT_MID, 8, 0);
        }
    }
    
    // Recently deleted files section (only shown when searching)
    if (sidebar.searching && sidebar.search_len > 0 && !sidebar.filtered_deleted_list.empty()) {
        // Section header
        lv_obj_t* header = lv_obj_create(sidebar.file_list);
        lv_obj_remove_style_all(header);
        lv_obj_set_size(header, LV_PCT(100), 24);
        
        lv_obj_t* header_label = lv_label_create(header);
        lv_label_set_text(header_label, "Recently Deleted");
        lv_obj_set_style_text_color(header_label, theme.text_dim, 0);
        lv_obj_set_style_text_font(header_label, &lv_font_montserrat_14, 0);
        lv_obj_align(header_label, LV_ALIGN_LEFT_MID, 4, 0);
        
        for (int i = 0; i < (int)sidebar.filtered_deleted_list.size(); i++) {
            const std::string& filename = sidebar.filtered_deleted_list[i];
            int adjusted_index = regular_count + i;
            
            lv_obj_t* file_btn = lv_btn_create(sidebar.file_list);
            lv_obj_set_size(file_btn, LV_PCT(100), 36);
            lv_obj_set_user_data(file_btn, (void*)(intptr_t)adjusted_index);
            
            bool is_selected = (!sidebar.new_file_selected && adjusted_index == sidebar.selected_index);
            if (is_selected) {
                selected_btn = file_btn;
            }
            lv_obj_set_style_bg_color(file_btn, 
                is_selected ? theme.sidebar_selected : theme.sidebar_btn_normal, 0);
            lv_obj_set_style_bg_color(file_btn, theme.sidebar_btn_hover, LV_STATE_PRESSED);
            lv_obj_set_style_radius(file_btn, 4, 0);
            lv_obj_set_style_bg_opa(file_btn, LV_OPA_70, 0);
            
            lv_obj_t* label = lv_label_create(file_btn);
            std::string display_name = filename;
            if (display_name.size() > 4 && display_name.substr(display_name.size() - 4) == ".txt") {
                display_name = display_name.substr(0, display_name.size() - 4);
            }
            lv_label_set_text(label, display_name.c_str());
            lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
            lv_obj_set_style_text_color(label, theme.text_dim, 0);
            lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
            lv_obj_align(label, LV_ALIGN_LEFT_MID, 8, 0);
        }
    }
    
    // Scroll selected item into view
    if (selected_btn) {
        lv_obj_scroll_to_view(selected_btn, LV_ANIM_OFF);
    }
}

void switch_to_file(SidebarState& sidebar, EditorState& editor, const std::string& filename) {
    save_editor_content(editor);
    
    std::string new_path = editor.user_files_dir + "/" + filename;
    load_file_into_editor(editor, new_path.c_str());
    
    editor.state_pending_save = true;
    editor.state_change_time = lv_tick_get();
}

void create_new_file(SidebarState& sidebar, EditorState& editor) {
    save_editor_content(editor);
    
    int counter = 1;
    std::string new_filename;
    std::string new_path;
    
    while (true) {
        if (counter == 1) {
            new_filename = "Untitled.txt";
        } else {
            new_filename = "Untitled " + std::to_string(counter) + ".txt";
        }
        new_path = editor.user_files_dir + "/" + new_filename;
        
        std::ifstream test(new_path);
        if (!test.good()) {
            break;
        }
        counter++;
    }
    
    std::ofstream new_file(new_path);
    new_file.close();
    
    scan_txt_files(sidebar, editor.user_files_dir);
    filter_sidebar_files(sidebar);
    refresh_file_list_ui(sidebar, editor);
    
    load_file_into_editor(editor, new_path.c_str());
    hide_sidebar(sidebar, editor);
}

void delete_file(SidebarState& sidebar, EditorState& editor, int index) {
    if (index < 0 || index >= (int)sidebar.filtered_file_list.size()) return;
    
    const std::string& filename = sidebar.filtered_file_list[index];
    std::string file_path = editor.user_files_dir + "/" + filename;
    std::string deleted_path = editor.recently_deleted_dir + "/" + filename;
    
    std::rename(file_path.c_str(), deleted_path.c_str());
    
    scan_txt_files(sidebar, editor.user_files_dir);
    scan_deleted_files(sidebar, editor.recently_deleted_dir);
    filter_sidebar_files(sidebar);
    
    if (file_path == editor.current_file_path) {
        if (!sidebar.filtered_file_list.empty()) {
            switch_to_file(sidebar, editor, sidebar.filtered_file_list[0]);
        } else {
            create_new_file(sidebar, editor);
            return;
        }
    }
    
    refresh_file_list_ui(sidebar, editor);
}

void restore_file(SidebarState& sidebar, EditorState& editor, int deleted_index) {
    if (deleted_index < 0 || deleted_index >= (int)sidebar.filtered_deleted_list.size()) return;
    
    const std::string& filename = sidebar.filtered_deleted_list[deleted_index];
    std::string src_path = editor.recently_deleted_dir + "/" + filename;
    std::string dest_path = editor.user_files_dir + "/" + filename;
    
    // Check if file already exists in user files
    std::ifstream test(dest_path);
    if (test.good()) {
        test.close();
        std::string base = filename.substr(0, filename.size() - 4);
        int counter = 1;
        while (true) {
            dest_path = editor.user_files_dir + "/" + base + " (" + std::to_string(counter) + ").txt";
            std::ifstream check(dest_path);
            if (!check.good()) {
                break;
            }
            check.close();
            counter++;
        }
    }
    
    std::rename(src_path.c_str(), dest_path.c_str());
    
    scan_txt_files(sidebar, editor.user_files_dir);
    scan_deleted_files(sidebar, editor.recently_deleted_dir);
    filter_sidebar_files(sidebar);
    refresh_file_list_ui(sidebar, editor);
    
    // Switch to the restored file
    size_t last_slash = dest_path.find_last_of('/');
    if (last_slash != std::string::npos) {
        std::string restored_filename = dest_path.substr(last_slash + 1);
        switch_to_file(sidebar, editor, restored_filename);
    }
}

void toggle_sidebar_search(SidebarState& sidebar, EditorState& editor) {
    if (sidebar.searching) {
        sidebar.searching = false;
        sidebar.search_buffer[0] = '\0';
        sidebar.search_len = 0;
        lv_obj_add_flag(sidebar.search_container, LV_OBJ_FLAG_HIDDEN);
        lv_textarea_set_text(sidebar.search_input, "");
        filter_sidebar_files(sidebar);
        refresh_file_list_ui(sidebar, editor);
    } else {
        sidebar.searching = true;
        sidebar.search_buffer[0] = '\0';
        sidebar.search_len = 0;
        sidebar.renaming = false;
        sidebar.rename_pending = false;
        lv_obj_clear_flag(sidebar.search_container, LV_OBJ_FLAG_HIDDEN);
        lv_textarea_set_text(sidebar.search_input, "");
        filter_sidebar_files(sidebar);
        refresh_file_list_ui(sidebar, editor);
    }
}

void handle_sidebar_text_input(SidebarState& sidebar, EditorState& editor, const char* text) {
    if (sidebar.searching) {
        // Text input goes to search
        if (sidebar.search_len < (int)sizeof(sidebar.search_buffer) - 1) {
            int text_len = strlen(text);
            int remaining = sizeof(sidebar.search_buffer) - 1 - sidebar.search_len;
            int copy_len = text_len < remaining ? text_len : remaining;
            memcpy(sidebar.search_buffer + sidebar.search_len, text, copy_len);
            sidebar.search_len += copy_len;
            sidebar.search_buffer[sidebar.search_len] = '\0';
            lv_textarea_set_text(sidebar.search_input, sidebar.search_buffer);
            filter_sidebar_files(sidebar);
            refresh_file_list_ui(sidebar, editor);
        }
    }
    // Renaming now uses a textarea, so text input is handled by LVGL
}

void handle_sidebar_backspace(SidebarState& sidebar, EditorState& editor) {
    if (sidebar.searching) {
        // Backspace removes from search
        if (sidebar.search_len > 0) {
            sidebar.search_len--;
            sidebar.search_buffer[sidebar.search_len] = '\0';
            lv_textarea_set_text(sidebar.search_input, sidebar.search_buffer);
            filter_sidebar_files(sidebar);
            refresh_file_list_ui(sidebar, editor);
        }
    }
    // Renaming now uses a textarea, so backspace is handled by LVGL
    // If not searching, backspace does nothing (no file deletion)
}

void start_rename(SidebarState& sidebar, EditorState& editor) {
    if (sidebar.searching) return;
    if (sidebar.new_file_selected) return;
    if (sidebar.selected_index < 0 || sidebar.selected_index >= (int)sidebar.filtered_file_list.size()) return;
    
    sidebar.renaming = true;
    sidebar.rename_file_index = sidebar.selected_index;
    sidebar.rename_textarea = nullptr;
    
    // Initialize rename buffer with current filename (without .txt)
    std::string filename = sidebar.filtered_file_list[sidebar.selected_index];
    if (filename.size() > 4 && filename.substr(filename.size() - 4) == ".txt") {
        filename = filename.substr(0, filename.size() - 4);
    }
    strncpy(sidebar.rename_buffer, filename.c_str(), sizeof(sidebar.rename_buffer) - 1);
    sidebar.rename_buffer[sizeof(sidebar.rename_buffer) - 1] = '\0';
    sidebar.rename_len = strlen(sidebar.rename_buffer);
    
    refresh_file_list_ui(sidebar, editor);
}

void commit_rename(SidebarState& sidebar, EditorState& editor) {
    if (!sidebar.renaming || !sidebar.rename_textarea) return;
    
    const char* new_name = lv_textarea_get_text(sidebar.rename_textarea);
    if (!new_name || strlen(new_name) == 0) {
        cancel_rename(sidebar, editor);
        return;
    }
    
    if (sidebar.rename_file_index < 0 || 
        sidebar.rename_file_index >= (int)sidebar.file_list_items.size()) {
        cancel_rename(sidebar, editor);
        return;
    }
    
    std::string old_filename = sidebar.file_list_items[sidebar.rename_file_index];
    std::string old_path = editor.user_files_dir + "/" + old_filename;
    
    std::string new_filename = new_name;
    if (new_filename.size() < 4 || new_filename.substr(new_filename.size() - 4) != ".txt") {
        new_filename += ".txt";
    }
    std::string new_path = editor.user_files_dir + "/" + new_filename;
    
    if (old_path != new_path) {
        std::ifstream test(new_path);
        if (!test.good()) {
            std::rename(old_path.c_str(), new_path.c_str());
            
            bool was_current_file = (editor.current_file_path == old_path);
            sidebar.file_list_items[sidebar.rename_file_index] = new_filename;
            
            if (was_current_file) {
                editor.current_file_path = new_path;
                editor.temp_file_path = new_path + ".tmp";
                update_filename_display(editor);
            }
        }
    }
    
    sidebar.renaming = false;
    sidebar.rename_pending = false;
    sidebar.rename_textarea = nullptr;
    
    filter_sidebar_files(sidebar);
    refresh_file_list_ui(sidebar, editor);
}

void cancel_rename(SidebarState& sidebar, EditorState& editor) {
    sidebar.renaming = false;
    sidebar.rename_pending = false;
    sidebar.rename_textarea = nullptr;
    refresh_file_list_ui(sidebar, editor);
}

void process_rename_save(SidebarState& sidebar, EditorState& editor, uint32_t debounce_delay) {
    // This function is now unused since we use commit_rename on Enter press
    // Kept for API compatibility
    (void)sidebar;
    (void)editor;
    (void)debounce_delay;
}
