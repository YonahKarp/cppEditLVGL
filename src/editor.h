#pragma once

#include "lvgl.h"
#include <string>
#include <cstdint>

constexpr int TEXT_BUFFER_SIZE = 512 * 1024;

struct SearchState;
struct SidebarState;

enum class JumpDirection { None = 0, Top = -1, Bottom = 1 };

struct EditorState {
    lv_obj_t* textarea = nullptr;
    lv_obj_t* status_bar = nullptr;
    lv_obj_t* filename_label = nullptr;
    lv_obj_t* word_count_label = nullptr;
    lv_obj_t* search_container = nullptr;
    lv_obj_t* search_input = nullptr;
    lv_obj_t* match_label = nullptr;
    
    lv_obj_t* battery_container = nullptr;
    lv_obj_t* battery_body = nullptr;
    lv_obj_t* battery_fill = nullptr;
    lv_obj_t* battery_tip = nullptr;
    int battery_percent = 100;
    
    std::string current_file_path;
    std::string temp_file_path;
    std::string state_file_path;
    std::string user_files_dir;
    std::string recently_deleted_dir;

    bool content_pending_save = false;
    uint32_t content_change_time = 0;
    bool state_pending_save = false;
    uint32_t state_change_time = 0;
    int pending_cursor_pos = 0;

    bool first_frame = true;
    int saved_cursor_pos = -1;

    bool dark_theme = true;
    int font_size_index = 3;

    int cached_word_count = 0;
};

void create_editor_ui(EditorState& state, lv_obj_t* parent);
void set_search_input_callback(EditorState& state, lv_event_cb_t cb, void* user_data);
void load_file_into_editor(EditorState& state, const char* file_path);
void save_editor_content(EditorState& state);
void save_editor_state(EditorState& state);
void update_editor_theme(EditorState& state, bool dark);
void update_word_count(EditorState& state);
void update_filename_display(EditorState& state);
void process_pending_saves(EditorState& state, uint32_t debounce_delay);
void update_battery_display(EditorState& state);

int count_words(const char* text, int text_len);
