#pragma once

#include "lvgl.h"
#include <string>
#include <vector>

struct EditorState;

struct SidebarState {
    lv_obj_t* sidebar_obj = nullptr;
    lv_obj_t* file_list = nullptr;
    lv_obj_t* new_file_btn = nullptr;
    lv_obj_t* search_container = nullptr;
    lv_obj_t* search_input = nullptr;
    lv_obj_t* delete_dialog = nullptr;
    lv_obj_t* delete_dialog_label = nullptr;
    lv_obj_t* delete_btn = nullptr;
    lv_obj_t* cancel_btn = nullptr;
    lv_obj_t* restore_dialog = nullptr;
    lv_obj_t* restore_dialog_label = nullptr;
    lv_obj_t* restore_btn = nullptr;
    lv_obj_t* restore_cancel_btn = nullptr;
    
    bool visible = false;
    std::vector<std::string> file_list_items;
    std::vector<std::string> filtered_file_list;
    std::vector<std::string> deleted_file_list;
    std::vector<std::string> filtered_deleted_list;
    int selected_index = 0;
    bool new_file_selected = false;
    
    bool confirm_delete = false;
    int delete_index = -1;
    int dialog_selection = 1;  // 0 = Delete/Restore, 1 = Cancel (default)
    
    bool confirm_restore = false;
    int restore_index = -1;
    
    bool searching = false;
    char search_buffer[256] = "";
    int search_len = 0;
    
    bool renaming = false;
    char rename_buffer[256] = "";
    int rename_len = 0;
    bool rename_pending = false;
    uint32_t rename_change_time = 0;
    int rename_file_index = -1;
    
    static constexpr int SIDEBAR_WIDTH = 280;
    static constexpr uint32_t ANIM_DURATION_MS = 150;
};

void create_sidebar_ui(SidebarState& sidebar, EditorState& editor, lv_obj_t* parent);
void toggle_sidebar(SidebarState& sidebar, EditorState& editor);
void show_sidebar(SidebarState& sidebar, EditorState& editor);
void hide_sidebar(SidebarState& sidebar, EditorState& editor);
void update_sidebar_theme(SidebarState& sidebar, bool dark);

void scan_txt_files(SidebarState& sidebar, const std::string& user_files_dir);
void scan_deleted_files(SidebarState& sidebar, const std::string& recently_deleted_dir);
void filter_sidebar_files(SidebarState& sidebar);
void refresh_file_list_ui(SidebarState& sidebar, EditorState& editor);

void switch_to_file(SidebarState& sidebar, EditorState& editor, const std::string& filename);
void create_new_file(SidebarState& sidebar, EditorState& editor);
void delete_file(SidebarState& sidebar, EditorState& editor, int index);
void restore_file(SidebarState& sidebar, EditorState& editor, int deleted_index);

void show_delete_dialog(SidebarState& sidebar, EditorState& editor, int index);
void hide_delete_dialog(SidebarState& sidebar);
void update_delete_dialog_selection(SidebarState& sidebar, EditorState& editor);
void confirm_delete_action(SidebarState& sidebar, EditorState& editor);

void show_restore_dialog(SidebarState& sidebar, EditorState& editor, int deleted_index);
void hide_restore_dialog(SidebarState& sidebar);
void update_restore_dialog_selection(SidebarState& sidebar, EditorState& editor);
void confirm_restore_action(SidebarState& sidebar, EditorState& editor);

void toggle_sidebar_search(SidebarState& sidebar, EditorState& editor);
void handle_sidebar_text_input(SidebarState& sidebar, EditorState& editor, const char* text);
void handle_sidebar_backspace(SidebarState& sidebar, EditorState& editor);
void start_rename(SidebarState& sidebar, EditorState& editor);
void process_rename_save(SidebarState& sidebar, EditorState& editor, uint32_t debounce_delay);
