#include "sidebar.h"
#include "sidebar_dialog.h"
#include "sidebar_file_dialog.h"
#include "sidebar_file.h"
#include "editor.h"
#include "theme.h"
#include "file_manager.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <dirent.h>
#include <set>

static EditorState* g_editor = nullptr;
static SidebarState* g_sidebar = nullptr;

static void file_btn_click_cb(lv_event_t* e) {
    if (!g_editor || !g_sidebar) return;
    
    lv_obj_t* btn = static_cast<lv_obj_t*>(lv_event_get_target(e));
    int index = (int)(intptr_t)lv_obj_get_user_data(btn);
    
    if (index >= 0 && index < (int)g_sidebar->folder_data.display_items.size()) {
        const SidebarItem& item = g_sidebar->folder_data.display_items[index];
        if (item.is_file()) {
            SidebarFileEntry entry;
            entry.filename = item.name;
            entry.folder = item.folder;
            switch_to_file_entry(*g_sidebar, *g_editor, entry);
            hide_sidebar(*g_sidebar, *g_editor);
        } else {
            // Toggle folder expansion (collapsed_folders tracks closed folders)
            if (g_sidebar->folder_data.collapsed_folders.find(item.name) != 
                g_sidebar->folder_data.collapsed_folders.end()) {
                g_sidebar->folder_data.collapsed_folders.erase(item.name);
            } else {
                g_sidebar->folder_data.collapsed_folders.insert(item.name);
            }
            g_editor->state_pending_save = true;
            g_editor->state_change_time = lv_tick_get();
            refresh_file_list_ui(*g_sidebar, *g_editor);
        }
    }
}

static void new_file_btn_click_cb(lv_event_t* e) {
    if (!g_editor || !g_sidebar) return;
    show_file_dialog(*g_sidebar, *g_editor, 0);
}

static void new_folder_btn_click_cb(lv_event_t* e) {
    if (!g_editor || !g_sidebar) return;
    create_folder(g_editor->user_files_dir);
    scan_folders_and_files(g_sidebar->folder_data, g_editor->user_files_dir);
    filter_folder_entries(g_sidebar->folder_data, std::string(g_sidebar->search_buffer, g_sidebar->search_len));
    refresh_file_list_ui(*g_sidebar, *g_editor);
}

static void search_input_cb(lv_event_t* e) {
    if (!g_editor || !g_sidebar) return;
    if (!g_sidebar->searching) return;
    
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
    
    lv_obj_t* btn_container = lv_obj_create(header);
    lv_obj_remove_style_all(btn_container);
    lv_obj_set_size(btn_container, 80, 36);  // Fixed width for 2 buttons (36+8+36)
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(btn_container, 8, 0);
    
    sidebar.new_folder_btn = lv_btn_create(btn_container);
    lv_obj_set_size(sidebar.new_folder_btn, 36, 36);
    lv_obj_set_style_bg_color(sidebar.new_folder_btn, theme.sidebar_new_file, 0);
    lv_obj_set_style_bg_opa(sidebar.new_folder_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(sidebar.new_folder_btn, 4, 0);
    lv_obj_set_style_shadow_width(sidebar.new_folder_btn, 0, 0);
    lv_obj_add_event_cb(sidebar.new_folder_btn, new_folder_btn_click_cb, LV_EVENT_CLICKED, nullptr);
    
    create_add_folder_icon(sidebar.add_folder_icon, sidebar.new_folder_btn, 20, theme.sidebar_btn_text);
    lv_obj_center(sidebar.add_folder_icon.canvas);
    
    sidebar.new_file_btn = lv_btn_create(btn_container);
    lv_obj_set_size(sidebar.new_file_btn, 36, 36);
    lv_obj_set_style_bg_color(sidebar.new_file_btn, theme.sidebar_new_file, 0);
    lv_obj_set_style_bg_opa(sidebar.new_file_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(sidebar.new_file_btn, 4, 0);
    lv_obj_set_style_shadow_width(sidebar.new_file_btn, 0, 0);
    lv_obj_add_event_cb(sidebar.new_file_btn, new_file_btn_click_cb, LV_EVENT_CLICKED, nullptr);
    
    create_add_document_icon(sidebar.add_document_icon, sidebar.new_file_btn, 20, theme.sidebar_btn_text);
    lv_obj_center(sidebar.add_document_icon.canvas);
    
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
    
    create_magnifying_glass_icon(sidebar.search_icon, sidebar.search_container, 16, theme.text_dim);
    
    sidebar.search_input = lv_textarea_create(sidebar.search_container);
    lv_obj_set_size(sidebar.search_input, LV_PCT(100), 28);
    lv_obj_set_flex_grow(sidebar.search_input, 1);
    lv_textarea_set_one_line(sidebar.search_input, true);
    lv_textarea_set_placeholder_text(sidebar.search_input, "Search files...");
    lv_obj_set_style_bg_opa(sidebar.search_input, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sidebar.search_input, 0, 0);
    lv_obj_set_style_border_width(sidebar.search_input, 0, LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(sidebar.search_input, 0, LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_width(sidebar.search_input, 0, 0);
    lv_obj_set_style_outline_width(sidebar.search_input, 0, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(sidebar.search_input, 0, LV_STATE_FOCUS_KEY);
    lv_obj_set_style_text_color(sidebar.search_input, theme.search_input_text, 0);
    lv_obj_set_style_text_font(sidebar.search_input, &lv_font_montserrat_16, 0);
    lv_obj_set_style_border_color(sidebar.search_input, theme.cursor, LV_PART_CURSOR);
    lv_obj_set_style_border_color(sidebar.search_input, theme.cursor, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_border_opa(sidebar.search_input, LV_OPA_COVER, LV_PART_CURSOR);
    lv_obj_set_style_border_opa(sidebar.search_input, LV_OPA_COVER, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(sidebar.search_input, 2, LV_PART_CURSOR);
    lv_obj_set_style_border_side(sidebar.search_input, LV_BORDER_SIDE_LEFT, LV_PART_CURSOR);
    lv_obj_set_style_bg_opa(sidebar.search_input, LV_OPA_TRANSP, LV_PART_CURSOR);
    lv_obj_add_event_cb(sidebar.search_input, search_input_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    
    sidebar.file_list = lv_obj_create(sidebar.sidebar_obj);
    lv_obj_remove_style_all(sidebar.file_list);
    lv_obj_set_size(sidebar.file_list, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_grow(sidebar.file_list, 1);
    lv_obj_set_flex_flow(sidebar.file_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(sidebar.file_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(sidebar.file_list, 4, 0);
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
    sidebar.new_folder_selected = false;
    sidebar.searching = false;
    sidebar.search_buffer[0] = '\0';
    sidebar.search_len = 0;
    sidebar.renaming = false;
    sidebar.rename_pending = false;
    
    lv_obj_add_flag(sidebar.search_container, LV_OBJ_FLAG_HIDDEN);
    
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
    
    if (sidebar.new_folder_btn) {
        lv_obj_set_style_bg_color(sidebar.new_folder_btn, theme.sidebar_new_file, 0);
    }
    
    if (sidebar.add_document_icon.canvas) {
        update_add_document_icon_color(sidebar.add_document_icon, theme.sidebar_btn_text);
    }
    
    if (sidebar.add_folder_icon.canvas) {
        update_add_folder_icon_color(sidebar.add_folder_icon, theme.sidebar_btn_text);
    }
    
    if (sidebar.search_container) {
        lv_obj_set_style_bg_color(sidebar.search_container, theme.search_input_bg, 0);
        lv_obj_set_style_border_color(sidebar.search_container, theme.sidebar_btn_hover, 0);
    }
    
    if (sidebar.search_input) {
        lv_obj_set_style_text_color(sidebar.search_input, theme.search_input_text, 0);
        lv_obj_set_style_border_color(sidebar.search_input, theme.cursor, LV_PART_CURSOR);
        lv_obj_set_style_border_color(sidebar.search_input, theme.cursor, LV_PART_CURSOR | LV_STATE_FOCUSED);
    }
    
    if (sidebar.search_icon.canvas) {
        update_magnifying_glass_icon_color(sidebar.search_icon, theme.text_dim);
    }
    
    if (sidebar.file_list) {
        lv_obj_set_style_bg_color(sidebar.file_list, theme.sidebar_bg, 0);
        lv_obj_set_style_bg_color(sidebar.file_list, theme.text_dim, LV_PART_SCROLLBAR);
    }
}

static bool case_insensitive_less(const std::string& a, const std::string& b) {
    size_t min_len = a.size() < b.size() ? a.size() : b.size();
    for (size_t i = 0; i < min_len; ++i) {
        char ca = std::tolower(static_cast<unsigned char>(a[i]));
        char cb = std::tolower(static_cast<unsigned char>(b[i]));
        if (ca < cb) return true;
        if (ca > cb) return false;
    }
    return a.size() < b.size();
}

void scan_txt_files(SidebarState& sidebar, const std::string& user_files_dir) {
    scan_folders_and_files(sidebar.folder_data, user_files_dir);
    
    sidebar.file_list_items.clear();
    for (const auto& entry : sidebar.folder_data.all_entries) {
        sidebar.file_list_items.push_back(sidebar.folder_data.get_relative_path(entry));
    }
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
    
    std::sort(sidebar.deleted_file_list.begin(), sidebar.deleted_file_list.end(), case_insensitive_less);
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
    std::string query(sidebar.search_buffer, sidebar.search_len);
    
    filter_folder_entries(sidebar.folder_data, query);
    
    sidebar.filtered_file_list.clear();
    for (const auto& entry : sidebar.folder_data.filtered_entries) {
        sidebar.filtered_file_list.push_back(sidebar.folder_data.get_relative_path(entry));
    }
    
    sidebar.filtered_deleted_list.clear();
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
    
    // Build display items
    sidebar.folder_data.build_display_items(sidebar.searching);
    
    int display_count = (int)sidebar.folder_data.display_items.size();
    int deleted_count = sidebar.searching ? (int)sidebar.filtered_deleted_list.size() : 0;
    int total_count = display_count + deleted_count;
    
    if (sidebar.selected_index >= total_count) {
        sidebar.selected_index = total_count > 0 ? total_count - 1 : 0;
    }
    
    if (sidebar.new_file_btn) {
        if (sidebar.new_file_selected) {
            lv_obj_set_style_border_width(sidebar.new_file_btn, 2, 0);
            lv_obj_set_style_border_color(sidebar.new_file_btn, lv_color_white(), 0);
        } else {
            lv_obj_set_style_border_width(sidebar.new_file_btn, 0, 0);
        }
    }
    
    if (sidebar.new_folder_btn) {
        if (sidebar.new_folder_selected) {
            lv_obj_set_style_border_width(sidebar.new_folder_btn, 2, 0);
            lv_obj_set_style_border_color(sidebar.new_folder_btn, lv_color_white(), 0);
        } else {
            lv_obj_set_style_border_width(sidebar.new_folder_btn, 0, 0);
        }
    }
    
    lv_obj_t* selected_btn = nullptr;
    
    for (int i = 0; i < display_count; i++) {
        const SidebarItem& item = sidebar.folder_data.display_items[i];
        bool is_selected = (!sidebar.new_file_selected && !sidebar.new_folder_selected && i == sidebar.selected_index);
        
        if (item.is_folder()) {
            bool is_expanded = sidebar.folder_data.collapsed_folders.find(item.name) == 
                              sidebar.folder_data.collapsed_folders.end();
            
            lv_obj_t* folder_btn = lv_btn_create(sidebar.file_list);
            lv_obj_set_size(folder_btn, LV_PCT(100), 36);
            lv_obj_set_user_data(folder_btn, (void*)(intptr_t)i);
            
            if (is_selected) {
                selected_btn = folder_btn;
                lv_obj_set_style_bg_color(folder_btn, theme.sidebar_selected, 0);
                lv_obj_set_style_bg_opa(folder_btn, LV_OPA_COVER, 0);
            } else {
                lv_obj_set_style_bg_opa(folder_btn, LV_OPA_TRANSP, 0);
            }
            lv_obj_set_style_bg_color(folder_btn, theme.sidebar_btn_hover, LV_STATE_PRESSED);
            lv_obj_set_style_bg_opa(folder_btn, LV_OPA_COVER, LV_STATE_PRESSED);
            lv_obj_set_style_radius(folder_btn, 4, 0);
            lv_obj_set_style_shadow_width(folder_btn, 0, 0);
            
            lv_obj_set_flex_flow(folder_btn, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(folder_btn, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_left(folder_btn, 8, 0);
            lv_obj_set_style_pad_column(folder_btn, 6, 0);
            
            if (!sidebar.searching && sidebar.renaming && i == sidebar.rename_file_index) {
                // Keep chevron and folder icon visible during rename
                lv_obj_t* chevron = lv_label_create(folder_btn);
                lv_label_set_text(chevron, is_expanded ? LV_SYMBOL_DOWN : LV_SYMBOL_RIGHT);
                lv_obj_set_style_text_color(chevron, theme.text_dim, 0);
                lv_obj_set_style_text_font(chevron, &lv_font_montserrat_14, 0);
                
                lv_obj_t* folder_icon = lv_label_create(folder_btn);
                lv_label_set_text(folder_icon, LV_SYMBOL_DIRECTORY);
                lv_obj_set_style_text_color(folder_icon, theme.sidebar_btn_text, 0);
                lv_obj_set_style_text_font(folder_icon, &lv_font_montserrat_14, 0);
                
                lv_obj_t* ta = lv_textarea_create(folder_btn);
                lv_obj_set_flex_grow(ta, 1);
                lv_obj_set_height(ta, 28);
                lv_textarea_set_one_line(ta, true);
                lv_textarea_set_text(ta, sidebar.rename_buffer);
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
                lv_obj_t* chevron = lv_label_create(folder_btn);
                lv_label_set_text(chevron, is_expanded ? LV_SYMBOL_DOWN : LV_SYMBOL_RIGHT);
                lv_obj_set_style_text_color(chevron, theme.text_dim, 0);
                lv_obj_set_style_text_font(chevron, &lv_font_montserrat_14, 0);
                
                lv_obj_t* folder_icon = lv_label_create(folder_btn);
                lv_label_set_text(folder_icon, LV_SYMBOL_DIRECTORY);
                lv_obj_set_style_text_color(folder_icon, theme.sidebar_btn_text, 0);
                lv_obj_set_style_text_font(folder_icon, &lv_font_montserrat_14, 0);
                
                lv_obj_t* label = lv_label_create(folder_btn);
                lv_label_set_text(label, item.name.c_str());
                lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
                lv_obj_set_style_text_color(label, theme.sidebar_btn_text, 0);
                lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
            }
        } else {
            lv_obj_t* file_item = create_file_item_ui(
                sidebar, editor, sidebar.file_list, item, i, is_selected, theme, file_btn_click_cb);
            if (is_selected) {
                selected_btn = file_item;
            }
        }
    }
    
    if (sidebar.searching && sidebar.search_len > 0 && !sidebar.filtered_deleted_list.empty()) {
        lv_obj_t* header = lv_obj_create(sidebar.file_list);
        lv_obj_remove_style_all(header);
        lv_obj_set_size(header, LV_PCT(100), 24);
        
        lv_obj_t* header_label = lv_label_create(header);
        lv_label_set_text(header_label, "Recently Deleted");
        lv_obj_set_style_text_color(header_label, theme.text_dim, 0);
        lv_obj_set_style_text_font(header_label, &lv_font_montserrat_14, 0);
        lv_obj_align(header_label, LV_ALIGN_LEFT_MID, 10, 0);
        
        for (int i = 0; i < (int)sidebar.filtered_deleted_list.size(); i++) {
            const std::string& filename = sidebar.filtered_deleted_list[i];
            int adjusted_index = display_count + i;
            bool is_selected = (!sidebar.new_file_selected && adjusted_index == sidebar.selected_index);
            
            lv_obj_t* file_item = create_deleted_file_item_ui(
                sidebar.file_list, filename, adjusted_index, is_selected, theme);
            if (is_selected) {
                selected_btn = file_item;
            }
        }
    }
    
    if (selected_btn) {
        lv_obj_scroll_to_view(selected_btn, LV_ANIM_OFF);
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
}

void handle_sidebar_backspace(SidebarState& sidebar, EditorState& editor) {
    if (sidebar.searching) {
        if (sidebar.search_len > 0) {
            sidebar.search_len--;
            sidebar.search_buffer[sidebar.search_len] = '\0';
            lv_textarea_set_text(sidebar.search_input, sidebar.search_buffer);
            filter_sidebar_files(sidebar);
            refresh_file_list_ui(sidebar, editor);
        }
    }
}

void process_rename_save(SidebarState& sidebar, EditorState& editor, uint32_t debounce_delay) {
    (void)sidebar;
    (void)editor;
    (void)debounce_delay;
}
