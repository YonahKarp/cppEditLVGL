#include "app.h"
#include "editor.h"
#include "sidebar.h"
#include "search.h"
#include "theme.h"
#include "file_manager.h"
#include <fstream>
#include <cstdio>
#include <sys/stat.h>
#include <string>

#include "lvgl.h"
#include <SDL2/SDL.h>

static EditorState g_editor;
static SidebarState g_sidebar;
static SearchState g_search;
static lv_group_t* g_input_group = nullptr;
static int32_t g_selection_start = -1;

static void focus_editor_textarea() {
    if (g_input_group) {
        lv_group_focus_obj(g_editor.textarea);
    }
}

static void focus_sidebar_search() {
    if (g_input_group) {
        lv_group_focus_obj(g_sidebar.search_input);
    }
}

static void focus_editor_search() {
    if (g_input_group) {
        lv_group_focus_obj(g_editor.search_input);
    }
}

static void highlight_current_match() {
    if (!g_search.active || g_search.current_match_index < 0 ||
        g_search.current_match_index >= static_cast<int>(g_search.matches.size())) {
        return;
    }
    
    const SearchMatch& match = g_search.matches[g_search.current_match_index];
    
    lv_obj_t* label = lv_textarea_get_label(g_editor.textarea);
    if (label) {
        lv_label_set_text_selection_start(label, match.start);
        lv_label_set_text_selection_end(label, match.end);
        lv_obj_invalidate(g_editor.textarea);
    }
    
    lv_textarea_set_cursor_pos(g_editor.textarea, match.start);
}

static void update_match_label() {
    if (g_search.matches.empty()) {
        lv_label_set_text(g_editor.match_label, "0/0");
    } else {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d/%d", 
                 g_search.current_match_index + 1, 
                 static_cast<int>(g_search.matches.size()));
        lv_label_set_text(g_editor.match_label, buf);
    }
}

static void search_input_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_VALUE_CHANGED) {
        const char* query = lv_textarea_get_text(g_editor.search_input);
        int query_len = strlen(query);
        
        if (query_len > 0 && query_len < static_cast<int>(sizeof(g_search.query))) {
            strncpy(g_search.query, query, sizeof(g_search.query) - 1);
            g_search.query[sizeof(g_search.query) - 1] = '\0';
            g_search.query_len = query_len;
            
            const char* text = lv_textarea_get_text(g_editor.textarea);
            int text_len = strlen(text);
            
            perform_search(g_search, text, text_len);
            update_match_label();
            highlight_current_match();
        } else {
            g_search.query[0] = '\0';
            g_search.query_len = 0;
            g_search.matches.clear();
            g_search.current_match_index = -1;
            update_match_label();
            
            lv_obj_t* label = lv_textarea_get_label(g_editor.textarea);
            if (label) {
                lv_label_set_text_selection_start(label, LV_LABEL_TEXT_SELECTION_OFF);
                lv_label_set_text_selection_end(label, LV_LABEL_TEXT_SELECTION_OFF);
                lv_obj_invalidate(g_editor.textarea);
            }
        }
    }
}

static std::string get_selected_text(lv_obj_t* textarea) {
    if (!textarea || !lv_obj_check_type(textarea, &lv_textarea_class)) {
        return "";
    }
    
    lv_obj_t* label = lv_textarea_get_label(textarea);
    if (!label) return "";
    
    uint32_t sel_start = lv_label_get_text_selection_start(label);
    uint32_t sel_end = lv_label_get_text_selection_end(label);
    
    if (sel_start == LV_LABEL_TEXT_SELECTION_OFF || sel_end == LV_LABEL_TEXT_SELECTION_OFF) {
        return "";
    }
    
    if (sel_start > sel_end) {
        uint32_t tmp = sel_start;
        sel_start = sel_end;
        sel_end = tmp;
    }
    
    const char* text = lv_textarea_get_text(textarea);
    if (!text) return "";
    
    uint32_t text_len = strlen(text);
    if (sel_start >= text_len) return "";
    if (sel_end > text_len) sel_end = text_len;
    
    return std::string(text + sel_start, sel_end - sel_start);
}

static void clear_selection(lv_obj_t* textarea) {
    if (!textarea) return;
    lv_obj_t* label = lv_textarea_get_label(textarea);
    if (label) {
        lv_label_set_text_selection_start(label, LV_LABEL_TEXT_SELECTION_OFF);
        lv_label_set_text_selection_end(label, LV_LABEL_TEXT_SELECTION_OFF);
        lv_obj_invalidate(textarea);
    }
    g_selection_start = -1;
}

static void delete_selected_text(lv_obj_t* textarea) {
    if (!textarea || !lv_obj_check_type(textarea, &lv_textarea_class)) return;
    
    lv_obj_t* label = lv_textarea_get_label(textarea);
    if (!label) return;
    
    uint32_t sel_start = lv_label_get_text_selection_start(label);
    uint32_t sel_end = lv_label_get_text_selection_end(label);
    
    if (sel_start == LV_LABEL_TEXT_SELECTION_OFF || sel_end == LV_LABEL_TEXT_SELECTION_OFF) {
        return;
    }
    
    if (sel_start > sel_end) {
        uint32_t tmp = sel_start;
        sel_start = sel_end;
        sel_end = tmp;
    }
    
    const char* text = lv_textarea_get_text(textarea);
    if (!text) return;
    
    std::string new_text;
    new_text.append(text, sel_start);
    new_text.append(text + sel_end);
    
    clear_selection(textarea);
    lv_textarea_set_text(textarea, new_text.c_str());
    lv_textarea_set_cursor_pos(textarea, sel_start);
}

static void handle_clipboard() {
    const uint8_t* kb_state = SDL_GetKeyboardState(nullptr);
    bool ctrl = kb_state[SDL_SCANCODE_LCTRL] || kb_state[SDL_SCANCODE_RCTRL];
    bool cmd = kb_state[SDL_SCANCODE_LGUI] || kb_state[SDL_SCANCODE_RGUI];
    
    if (!ctrl && !cmd) return;
    if (g_sidebar.visible) return;
    
    static uint32_t last_clipboard_time = 0;
    uint32_t now = lv_tick_get();
    bool debounce = (now - last_clipboard_time) > 200;
    if (!debounce) return;
    
    lv_obj_t* focused = lv_group_get_focused(g_input_group);
    if (!focused || !lv_obj_check_type(focused, &lv_textarea_class)) return;
    
    // Copy (Ctrl+C or Cmd+C)
    if (kb_state[SDL_SCANCODE_C]) {
        last_clipboard_time = now;
        std::string selected = get_selected_text(focused);
        if (!selected.empty()) {
            SDL_SetClipboardText(selected.c_str());
        }
    }
    
    // Cut (Ctrl+X or Cmd+X)
    if (kb_state[SDL_SCANCODE_X]) {
        last_clipboard_time = now;
        std::string selected = get_selected_text(focused);
        if (!selected.empty()) {
            SDL_SetClipboardText(selected.c_str());
            delete_selected_text(focused);
            
            if (focused == g_editor.textarea) {
                g_editor.content_pending_save = true;
                g_editor.content_change_time = lv_tick_get();
                update_word_count(g_editor);
            }
        }
    }
    
    // Paste (Ctrl+V or Cmd+V)
    if (kb_state[SDL_SCANCODE_V]) {
        last_clipboard_time = now;
        char* clipboard = SDL_GetClipboardText();
        if (clipboard && strlen(clipboard) > 0) {
            // Delete any selected text first
            std::string selected = get_selected_text(focused);
            if (!selected.empty()) {
                delete_selected_text(focused);
            }
            
            // Insert clipboard text
            for (const char* p = clipboard; *p; p++) {
                lv_textarea_add_char(focused, *p);
            }
            
            if (focused == g_editor.textarea) {
                g_editor.content_pending_save = true;
                g_editor.content_change_time = lv_tick_get();
                update_word_count(g_editor);
            }
        }
        SDL_free(clipboard);
    }
}

static int32_t find_paragraph_start(const char* text, int32_t pos) {
    if (pos <= 0) return 0;
    pos--;
    while (pos > 0 && text[pos] != '\n') pos--;
    if (pos > 0) {
        pos--;
        while (pos > 0 && text[pos] != '\n') pos--;
        if (pos > 0) pos++;
    }
    return pos;
}

static int32_t find_paragraph_end(const char* text, int32_t text_len, int32_t pos) {
    while (pos < text_len && text[pos] != '\n') pos++;
    if (pos < text_len) pos++;
    while (pos < text_len && text[pos] != '\n') pos++;
    return pos;
}

static int32_t find_prev_word_start(const char* text, int32_t pos) {
    if (pos <= 0) return 0;
    pos--;
    while (pos > 0 && (text[pos] == ' ' || text[pos] == '\n' || text[pos] == '\t')) pos--;
    while (pos > 0 && text[pos - 1] != ' ' && text[pos - 1] != '\n' && text[pos - 1] != '\t') pos--;
    return pos;
}

struct KeyRepeatState {
    bool last_pressed = false;
    bool acted_on_press = false;
    uint32_t press_time = 0;
    uint32_t last_action_time = 0;
    
    static constexpr uint32_t INITIAL_DELAY = 800;
    static constexpr uint32_t REPEAT_DELAY = 80;
    
    bool should_act(bool is_pressed, uint32_t now) {
        if (!is_pressed) {
            last_pressed = false;
            acted_on_press = false;
            return false;
        }
        
        bool is_new_press = !last_pressed;
        last_pressed = true;
        
        if (is_new_press) {
            press_time = now;
            last_action_time = now;
            acted_on_press = true;
            return true;
        }
        
        if (acted_on_press && 
            (now - press_time >= INITIAL_DELAY) && 
            (now - last_action_time >= REPEAT_DELAY)) {
            last_action_time = now;
            return true;
        }
        
        return false;
    }
};

static void handle_text_selection() {
    const uint8_t* kb_state = SDL_GetKeyboardState(nullptr);
    bool shift = kb_state[SDL_SCANCODE_LSHIFT] || kb_state[SDL_SCANCODE_RSHIFT];
    bool ctrl = kb_state[SDL_SCANCODE_LCTRL] || kb_state[SDL_SCANCODE_RCTRL];
    bool alt = kb_state[SDL_SCANCODE_LALT] || kb_state[SDL_SCANCODE_RALT];
    
    static int32_t last_cursor_pos = -1;
    static bool last_shift = false;
    static bool last_any_arrow = false;
    static uint32_t last_nav_time = 0;
    
    static KeyRepeatState ctrl_up_state;
    static KeyRepeatState ctrl_down_state;
    static KeyRepeatState ctrl_shift_up_state;
    static KeyRepeatState ctrl_shift_down_state;
    static KeyRepeatState ctrl_backspace_state;
    
    lv_obj_t* focused = lv_group_get_focused(g_input_group);
    if (!focused || !lv_obj_check_type(focused, &lv_textarea_class)) {
        last_cursor_pos = -1;
        last_shift = false;
        last_any_arrow = false;
        return;
    }
    
    lv_obj_t* label = lv_textarea_get_label(focused);
    if (!label) return;
    
    int32_t cur_pos = lv_textarea_get_cursor_pos(focused);
    const char* text = lv_textarea_get_text(focused);
    int32_t text_len = strlen(text);
    
    uint32_t now = lv_tick_get();
    bool nav_debounce = (now - last_nav_time) > 100;
    
    // Handle Ctrl+Backspace (delete previous word)
    bool ctrl_backspace = ctrl && kb_state[SDL_SCANCODE_BACKSPACE] && focused == g_editor.textarea;
    if (ctrl_backspace_state.should_act(ctrl_backspace, now)) {
        int32_t word_start = find_prev_word_start(text, cur_pos);
        if (word_start < cur_pos) {
            std::string new_text;
            new_text.append(text, word_start);
            new_text.append(text + cur_pos);
            lv_textarea_set_text(focused, new_text.c_str());
            lv_textarea_set_cursor_pos(focused, word_start);
            g_editor.content_pending_save = true;
            g_editor.content_change_time = lv_tick_get();
            update_word_count(g_editor);
            text = lv_textarea_get_text(focused);
            text_len = strlen(text);
            cur_pos = word_start;
        }
    }
    if (ctrl_backspace) return;
    
    // Handle Alt+Up/Down (jump to top/bottom of document)
    if (alt && !shift && !ctrl && nav_debounce) {
        if (kb_state[SDL_SCANCODE_UP]) {
            last_nav_time = now;
            lv_textarea_set_cursor_pos(focused, 0);
            g_selection_start = -1;
            return;
        }
        if (kb_state[SDL_SCANCODE_DOWN]) {
            last_nav_time = now;
            lv_textarea_set_cursor_pos(focused, text_len);
            g_selection_start = -1;
            return;
        }
    }
    
    // Handle Ctrl+Up/Down (jump to previous/next paragraph) with key repeat
    bool ctrl_up = ctrl && !shift && !alt && kb_state[SDL_SCANCODE_UP];
    bool ctrl_down = ctrl && !shift && !alt && kb_state[SDL_SCANCODE_DOWN];
    
    if (ctrl_up_state.should_act(ctrl_up, now)) {
        int32_t new_pos = find_paragraph_start(text, cur_pos);
        lv_textarea_set_cursor_pos(focused, new_pos);
        g_selection_start = -1;
    }
    
    if (ctrl_down_state.should_act(ctrl_down, now)) {
        int32_t new_pos = find_paragraph_end(text, text_len, cur_pos);
        lv_textarea_set_cursor_pos(focused, new_pos);
        g_selection_start = -1;
    }
    
    // Handle Ctrl+Shift+Up/Down (extend selection to previous/next paragraph) with key repeat
    bool ctrl_shift_up = ctrl && shift && !alt && kb_state[SDL_SCANCODE_UP];
    bool ctrl_shift_down = ctrl && shift && !alt && kb_state[SDL_SCANCODE_DOWN];
    
    if (ctrl_shift_up_state.should_act(ctrl_shift_up, now)) {
        if (g_selection_start < 0) g_selection_start = cur_pos;
        int32_t new_pos = find_paragraph_start(text, cur_pos);
        lv_textarea_set_cursor_pos(focused, new_pos);
        uint32_t sel_start = (g_selection_start < new_pos) ? g_selection_start : new_pos;
        uint32_t sel_end = (g_selection_start < new_pos) ? new_pos : g_selection_start;
        lv_label_set_text_selection_start(label, sel_start);
        lv_label_set_text_selection_end(label, sel_end);
        lv_obj_invalidate(focused);
        last_cursor_pos = new_pos;
    }
    
    if (ctrl_shift_down_state.should_act(ctrl_shift_down, now)) {
        if (g_selection_start < 0) g_selection_start = cur_pos;
        int32_t new_pos = find_paragraph_end(text, text_len, cur_pos);
        lv_textarea_set_cursor_pos(focused, new_pos);
        uint32_t sel_start = (g_selection_start < new_pos) ? g_selection_start : new_pos;
        uint32_t sel_end = (g_selection_start < new_pos) ? new_pos : g_selection_start;
        lv_label_set_text_selection_start(label, sel_start);
        lv_label_set_text_selection_end(label, sel_end);
        lv_obj_invalidate(focused);
        last_cursor_pos = new_pos;
    }
    
    if (ctrl_up || ctrl_down || ctrl_shift_up || ctrl_shift_down) {
        return;
    }
    
    if (ctrl_up || ctrl_down || ctrl_shift_up || ctrl_shift_down) {
        return;
    }
    
    bool any_arrow = kb_state[SDL_SCANCODE_LEFT] || kb_state[SDL_SCANCODE_RIGHT] ||
                     kb_state[SDL_SCANCODE_UP] || kb_state[SDL_SCANCODE_DOWN] ||
                     kb_state[SDL_SCANCODE_HOME] || kb_state[SDL_SCANCODE_END];
    
    // Only clear selection when moving cursor WITHOUT shift
    if (!shift && any_arrow) {
        if (g_selection_start >= 0) {
            lv_label_set_text_selection_start(label, LV_LABEL_TEXT_SELECTION_OFF);
            lv_label_set_text_selection_end(label, LV_LABEL_TEXT_SELECTION_OFF);
            lv_obj_invalidate(focused);
        }
        g_selection_start = -1;
        last_cursor_pos = cur_pos;
        last_shift = false;
        last_any_arrow = any_arrow;
        return;
    }
    
    // If shift is not pressed and no arrow key, just update tracking
    if (!shift) {
        last_cursor_pos = cur_pos;
        last_shift = false;
        last_any_arrow = any_arrow;
        return;
    }
    
    if (!any_arrow) {
        last_cursor_pos = cur_pos;
        last_shift = shift;
        last_any_arrow = false;
        return;
    }
    
    // Detect new arrow key press while shift is held
    bool new_selection_move = (shift && any_arrow && (!last_any_arrow || !last_shift));
    
    // Initialize selection start from the position BEFORE the key was pressed
    if (g_selection_start < 0 && new_selection_move && last_cursor_pos >= 0) {
        g_selection_start = last_cursor_pos;
    } else if (g_selection_start < 0) {
        g_selection_start = cur_pos;
    }
    
    // Update selection based on current cursor position
    uint32_t sel_start = (g_selection_start < cur_pos) ? g_selection_start : cur_pos;
    uint32_t sel_end = (g_selection_start < cur_pos) ? cur_pos : g_selection_start;
    
    lv_label_set_text_selection_start(label, sel_start);
    lv_label_set_text_selection_end(label, sel_end);
    lv_obj_invalidate(focused);
    
    last_cursor_pos = cur_pos;
    last_shift = shift;
    last_any_arrow = any_arrow;
}

static void keyboard_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_KEY) return;
    
    uint32_t key = lv_event_get_key(e);
    
    if (key == LV_KEY_ESC) {
        if (g_search.active) {
            close_search(g_search);
            lv_obj_add_flag(g_editor.search_container, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_editor.word_count_label, LV_OBJ_FLAG_HIDDEN);
            clear_selection(g_editor.textarea);
            focus_editor_textarea();
        } else if (g_sidebar.visible) {
            hide_sidebar(g_sidebar, g_editor);
            focus_editor_textarea();
        }
    }
}

static void handle_sidebar_keyboard() {
    if (!g_sidebar.visible) return;
    
    const uint8_t* kb_state = SDL_GetKeyboardState(nullptr);
    bool ctrl = kb_state[SDL_SCANCODE_LCTRL] || kb_state[SDL_SCANCODE_RCTRL];
    static uint32_t last_nav_time = 0;
    uint32_t now = lv_tick_get();
    bool nav_debounce = (now - last_nav_time) > 120;
    
    // Handle Ctrl+F to toggle search
    if (ctrl && nav_debounce && kb_state[SDL_SCANCODE_F]) {
        last_nav_time = now;
        toggle_sidebar_search(g_sidebar, g_editor);
        return;
    }
    
    // Handle restore dialog
    if (g_sidebar.confirm_restore) {
        if (nav_debounce) {
            if (kb_state[SDL_SCANCODE_LEFT]) {
                last_nav_time = now;
                g_sidebar.dialog_selection = 0;
                update_restore_dialog_selection(g_sidebar, g_editor);
            }
            
            if (kb_state[SDL_SCANCODE_RIGHT]) {
                last_nav_time = now;
                g_sidebar.dialog_selection = 1;
                update_restore_dialog_selection(g_sidebar, g_editor);
            }
            
            if (kb_state[SDL_SCANCODE_RETURN] || kb_state[SDL_SCANCODE_KP_ENTER]) {
                last_nav_time = now;
                confirm_restore_action(g_sidebar, g_editor);
            }
            
            if (kb_state[SDL_SCANCODE_ESCAPE]) {
                last_nav_time = now;
                hide_restore_dialog(g_sidebar);
            }
        }
        return;
    }
    
    // Handle delete dialog
    if (g_sidebar.confirm_delete) {
        if (nav_debounce) {
            if (kb_state[SDL_SCANCODE_LEFT]) {
                last_nav_time = now;
                g_sidebar.dialog_selection = 0;
                update_delete_dialog_selection(g_sidebar, g_editor);
            }
            
            if (kb_state[SDL_SCANCODE_RIGHT]) {
                last_nav_time = now;
                g_sidebar.dialog_selection = 1;
                update_delete_dialog_selection(g_sidebar, g_editor);
            }
            
            if (kb_state[SDL_SCANCODE_RETURN] || kb_state[SDL_SCANCODE_KP_ENTER]) {
                last_nav_time = now;
                confirm_delete_action(g_sidebar, g_editor);
            }
            
            if (kb_state[SDL_SCANCODE_ESCAPE]) {
                last_nav_time = now;
                hide_delete_dialog(g_sidebar);
            }
        }
        return;
    }
    
    int regular_count = (int)g_sidebar.filtered_file_list.size();
    int deleted_count = g_sidebar.searching ? (int)g_sidebar.filtered_deleted_list.size() : 0;
    int total_count = regular_count + deleted_count;
    
    if (nav_debounce) {
        if (kb_state[SDL_SCANCODE_UP]) {
            last_nav_time = now;
            if (g_sidebar.renaming) {
                // Remove rename textarea from group before cancelling
                if (g_sidebar.rename_textarea) {
                    lv_group_remove_obj(g_sidebar.rename_textarea);
                }
                cancel_rename(g_sidebar, g_editor);
            }
            if (g_sidebar.new_file_selected) {
                // Already at top, do nothing
            } else if (g_sidebar.selected_index == 0) {
                if (!g_sidebar.searching) {
                    g_sidebar.new_file_selected = true;
                }
                refresh_file_list_ui(g_sidebar, g_editor);
            } else if (g_sidebar.selected_index > 0) {
                g_sidebar.selected_index--;
                refresh_file_list_ui(g_sidebar, g_editor);
            }
        }
        
        if (kb_state[SDL_SCANCODE_DOWN]) {
            last_nav_time = now;
            if (g_sidebar.renaming) {
                // Remove rename textarea from group before cancelling
                if (g_sidebar.rename_textarea) {
                    lv_group_remove_obj(g_sidebar.rename_textarea);
                }
                cancel_rename(g_sidebar, g_editor);
            }
            if (g_sidebar.new_file_selected) {
                g_sidebar.new_file_selected = false;
                g_sidebar.selected_index = 0;
                refresh_file_list_ui(g_sidebar, g_editor);
            } else if (g_sidebar.selected_index < total_count - 1) {
                g_sidebar.selected_index++;
                refresh_file_list_ui(g_sidebar, g_editor);
            }
        }
        
        // Right arrow to start rename (only if not searching)
        if (kb_state[SDL_SCANCODE_RIGHT] && !g_sidebar.searching && !g_sidebar.new_file_selected) {
            last_nav_time = now;
            if (!g_sidebar.renaming) {
                start_rename(g_sidebar, g_editor);
                // Add rename textarea to group and focus it
                if (g_sidebar.rename_textarea) {
                    lv_group_add_obj(g_input_group, g_sidebar.rename_textarea);
                    lv_group_focus_obj(g_sidebar.rename_textarea);
                }
            }
        }
        
        if (kb_state[SDL_SCANCODE_RETURN] || kb_state[SDL_SCANCODE_KP_ENTER]) {
            last_nav_time = now;
            if (g_sidebar.renaming) {
                // Remove rename textarea from group before committing
                if (g_sidebar.rename_textarea) {
                    lv_group_remove_obj(g_sidebar.rename_textarea);
                }
                commit_rename(g_sidebar, g_editor);
            } else if (g_sidebar.new_file_selected) {
                create_new_file(g_sidebar, g_editor);
                focus_editor_textarea();
            } else if (g_sidebar.selected_index >= regular_count && g_sidebar.searching) {
                // Selected a deleted file - show restore dialog
                int deleted_index = g_sidebar.selected_index - regular_count;
                show_restore_dialog(g_sidebar, g_editor, deleted_index);
            } else if (g_sidebar.selected_index >= 0 && g_sidebar.selected_index < regular_count) {
                switch_to_file(g_sidebar, g_editor, g_sidebar.filtered_file_list[g_sidebar.selected_index]);
                hide_sidebar(g_sidebar, g_editor);
                focus_editor_textarea();
            }
        }
        
        // Delete key shows delete dialog (only if not searching or renaming)
        if (kb_state[SDL_SCANCODE_DELETE] && !g_sidebar.searching && !g_sidebar.renaming) {
            last_nav_time = now;
            if (!g_sidebar.new_file_selected && g_sidebar.selected_index >= 0 && g_sidebar.selected_index < regular_count) {
                show_delete_dialog(g_sidebar, g_editor, g_sidebar.selected_index);
            }
        }
        
        if (kb_state[SDL_SCANCODE_ESCAPE]) {
            last_nav_time = now;
            if (g_sidebar.renaming) {
                // Remove rename textarea from group before cancelling
                if (g_sidebar.rename_textarea) {
                    lv_group_remove_obj(g_sidebar.rename_textarea);
                }
                cancel_rename(g_sidebar, g_editor);
            } else if (g_sidebar.searching) {
                toggle_sidebar_search(g_sidebar, g_editor);
            } else {
                hide_sidebar(g_sidebar, g_editor);
                focus_editor_textarea();
            }
        }
    }
}

static void handle_search_navigation() {
    if (!g_search.active) return;
    
    const uint8_t* kb_state = SDL_GetKeyboardState(nullptr);
    static uint32_t last_nav_time = 0;
    uint32_t now = lv_tick_get();
    bool debounce = (now - last_nav_time) > 150;
    
    if (!debounce) return;
    
    // Handle ESC to close search
    if (kb_state[SDL_SCANCODE_ESCAPE]) {
        last_nav_time = now;
        close_search(g_search);
        lv_obj_add_flag(g_editor.search_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(g_editor.word_count_label, LV_OBJ_FLAG_HIDDEN);
        lv_textarea_set_text(g_editor.search_input, "");
        clear_selection(g_editor.textarea);
        focus_editor_textarea();
        return;
    }
    
    lv_obj_t* focused = lv_group_get_focused(g_input_group);
    if (focused != g_editor.search_input) return;
    
    bool shift = kb_state[SDL_SCANCODE_LSHIFT] || kb_state[SDL_SCANCODE_RSHIFT];
    
    if (kb_state[SDL_SCANCODE_RETURN] || kb_state[SDL_SCANCODE_KP_ENTER]) {
        last_nav_time = now;
        if (shift) {
            navigate_to_prev_match(g_search);
        } else {
            navigate_to_next_match(g_search);
        }
        update_match_label();
        highlight_current_match();
    }
}

static void handle_shortcuts() {
    static bool last_meta = false;
    static uint32_t last_key_time = 0;
    
    const uint8_t* kb_state = SDL_GetKeyboardState(nullptr);
    bool meta_pressed = kb_state[SDL_SCANCODE_LGUI] || kb_state[SDL_SCANCODE_RGUI];
    bool ctrl = kb_state[SDL_SCANCODE_LCTRL] || kb_state[SDL_SCANCODE_RCTRL];
    
    uint32_t now = lv_tick_get();
    bool debounce = (now - last_key_time) > 150;
    
    if (meta_pressed && !last_meta) {
        toggle_sidebar(g_sidebar, g_editor);
        if (g_sidebar.visible) {
            // Focus sidebar search to remove focus from editor
            focus_sidebar_search();
        } else {
            focus_editor_textarea();
        }
    }
    
    if (g_sidebar.visible) {
        handle_sidebar_keyboard();
    }
    
    if (ctrl && debounce) {
        if (kb_state[SDL_SCANCODE_A] && !g_sidebar.visible) {
            last_key_time = now;
            lv_obj_t* focused = lv_group_get_focused(g_input_group);
            if (focused && lv_obj_check_type(focused, &lv_textarea_class)) {
                const char* text = lv_textarea_get_text(focused);
                uint32_t text_len = strlen(text);
                lv_textarea_set_cursor_pos(focused, 0);
                lv_obj_t* label = lv_textarea_get_label(focused);
                if (label) {
                    lv_label_set_text_selection_start(label, 0);
                    lv_label_set_text_selection_end(label, text_len);
                    lv_obj_invalidate(focused);
                }
            }
        }
        
        if (kb_state[SDL_SCANCODE_F] && !g_sidebar.visible) {
            last_key_time = now;
            if (!g_search.active) {
                g_search.active = true;
                lv_obj_clear_flag(g_editor.search_container, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(g_editor.word_count_label, LV_OBJ_FLAG_HIDDEN);
                focus_editor_search();
            } else {
                close_search(g_search);
                lv_obj_add_flag(g_editor.search_container, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(g_editor.word_count_label, LV_OBJ_FLAG_HIDDEN);
                clear_selection(g_editor.textarea);
                focus_editor_textarea();
            }
        }
        
        if (kb_state[SDL_SCANCODE_T]) {
            last_key_time = now;
            g_editor.dark_theme = !g_editor.dark_theme;
            update_editor_theme(g_editor, g_editor.dark_theme);
            update_sidebar_theme(g_sidebar, g_editor.dark_theme);
            g_editor.state_pending_save = true;
            g_editor.state_change_time = lv_tick_get();
        }
        
        if (kb_state[SDL_SCANCODE_EQUALS] || kb_state[SDL_SCANCODE_KP_PLUS]) {
            last_key_time = now;
            if (g_editor.font_size_index < 9) {
                g_editor.font_size_index++;
                static const lv_font_t* fonts[] = {
                    &lv_font_montserrat_18, &lv_font_montserrat_20, &lv_font_montserrat_22,
                    &lv_font_montserrat_24, &lv_font_montserrat_26, &lv_font_montserrat_28,
                    &lv_font_montserrat_30, &lv_font_montserrat_32, &lv_font_montserrat_36,
                    &lv_font_montserrat_40
                };
                lv_obj_set_style_text_font(g_editor.textarea, fonts[g_editor.font_size_index], 0);
                g_editor.state_pending_save = true;
                g_editor.state_change_time = lv_tick_get();
            }
        }
        
        if (kb_state[SDL_SCANCODE_MINUS] || kb_state[SDL_SCANCODE_KP_MINUS]) {
            last_key_time = now;
            if (g_editor.font_size_index > 0) {
                g_editor.font_size_index--;
                static const lv_font_t* fonts[] = {
                    &lv_font_montserrat_18, &lv_font_montserrat_20, &lv_font_montserrat_22,
                    &lv_font_montserrat_24, &lv_font_montserrat_26, &lv_font_montserrat_28,
                    &lv_font_montserrat_30, &lv_font_montserrat_32, &lv_font_montserrat_36,
                    &lv_font_montserrat_40
                };
                lv_obj_set_style_text_font(g_editor.textarea, fonts[g_editor.font_size_index], 0);
                g_editor.state_pending_save = true;
                g_editor.state_change_time = lv_tick_get();
            }
        }
    }
    
    last_meta = meta_pressed;
}

static void load_editor_state(EditorState& state, const char* state_file_path, 
                              const char* default_file_path, std::string& out_file_path) {
    std::string current_file_path = default_file_path;
    int saved_cursor_pos = -1;
    int saved_font_size_index = 3;
    int saved_dark_theme = 1;

    std::ifstream state_in(state_file_path);
    if (state_in) {
        std::getline(state_in, current_file_path);
        state_in >> saved_cursor_pos;
        if (state_in >> saved_font_size_index) {
            if (saved_font_size_index < 0 || saved_font_size_index > 9) {
                saved_font_size_index = 3;
            }
        }
        state_in >> saved_dark_theme;
        state_in.close();
        if (current_file_path.empty()) {
            current_file_path = default_file_path;
        }
    }

    state.state_file_path = state_file_path;
    state.saved_cursor_pos = saved_cursor_pos;
    state.font_size_index = saved_font_size_index;
    state.dark_theme = (saved_dark_theme != 0);
    out_file_path = current_file_path;
}

bool init_app(App& app) {
    lv_init();
    
    SDL_Init(SDL_INIT_VIDEO);
    SDL_StartTextInput();
    
    app.display = lv_sdl_window_create(app.window_width, app.window_height);
    if (!app.display) {
        fprintf(stderr, "Failed to create SDL window\n");
        return false;
    }
    
    app.keyboard_indev = lv_sdl_keyboard_create();
    
    app.screen = lv_scr_act();
    
    app.editor = &g_editor;
    app.sidebar = &g_sidebar;
    app.search = &g_search;
    
    const char* user_files_dir = "user_files";
    const char* recently_deleted_dir = "user_files/recently_deleted";
    const char* state_file_path = "user_files/.editor_state";
    const char* default_file_path = "user_files/file 1.txt";
    
    ensure_directory(user_files_dir);
    ensure_directory(recently_deleted_dir);
    ensure_shortcuts_file(user_files_dir);
    
    g_editor.user_files_dir = user_files_dir;
    g_editor.recently_deleted_dir = recently_deleted_dir;
    
    std::string file_to_load;
    load_editor_state(g_editor, state_file_path, default_file_path, file_to_load);
    
    create_editor_ui(g_editor, app.screen);
    set_search_input_callback(g_editor, search_input_event_cb, nullptr);
    create_sidebar_ui(g_sidebar, g_editor, app.screen);
    
    struct stat st;
    if (stat(file_to_load.c_str(), &st) != 0) {
        std::ofstream new_file(file_to_load);
        new_file.close();
    }
    
    load_file_into_editor(g_editor, file_to_load.c_str());
    
    cleanup_old_deleted_files(recently_deleted_dir);
    
    return true;
}

void shutdown_app(App& app) {
    if (g_editor.content_pending_save) {
        save_editor_content(g_editor);
    }
    if (g_editor.state_pending_save) {
        save_editor_state(g_editor);
    }
    
    lv_deinit();
}

void run_app(App& app) {
    const uint32_t DEBOUNCE_DELAY = 500;
    const uint32_t RENAME_DEBOUNCE_DELAY = 1000;
    const uint32_t IDLE_WAIT_MS = 50;
    
    g_input_group = lv_group_create();
    app.input_group = g_input_group;
    
    lv_group_add_obj(g_input_group, g_editor.textarea);
    lv_group_add_obj(g_input_group, g_editor.search_input);
    lv_group_add_obj(g_input_group, g_sidebar.search_input);
    
    lv_indev_set_group(app.keyboard_indev, g_input_group);
    
    lv_group_focus_obj(g_editor.textarea);
    
    bool running = true;
    
    while (running) {
        // Check if sidebar is animating
        bool sidebar_animating = false;
        if (g_sidebar.animating) {
            uint32_t elapsed = lv_tick_get() - g_sidebar.anim_start_time;
            if (elapsed < SidebarState::ANIM_DURATION_MS + 50) {
                sidebar_animating = true;
            } else {
                g_sidebar.animating = false;
            }
        }
        
        // Use short wait during animation, longer when idle
        uint32_t wait_time = sidebar_animating ? 1 : IDLE_WAIT_MS;
        
        // Wait for event or timeout
        SDL_Event wait_event;
        bool had_event = SDL_WaitEventTimeout(&wait_event, wait_time);
        
        // Process the event we received
        if (had_event) {
            bool consumed = false;
            
            if (wait_event.type == SDL_QUIT) {
                running = false;
                consumed = true;
            }
            
            if (wait_event.type == SDL_KEYDOWN) {
                // Check for force quit shortcut
                if (wait_event.key.keysym.sym == SDLK_ESCAPE &&
                    (wait_event.key.keysym.mod & KMOD_CTRL) &&
                    (wait_event.key.keysym.mod & KMOD_SHIFT) &&
                    (wait_event.key.keysym.mod & KMOD_ALT)) {
                    running = false;
                    consumed = true;
                }
                // Consume Ctrl+Arrow keys so LVGL doesn't also handle them
                if ((wait_event.key.keysym.mod & KMOD_CTRL) &&
                    (wait_event.key.keysym.sym == SDLK_UP || 
                     wait_event.key.keysym.sym == SDLK_DOWN)) {
                    consumed = true;
                }
                // Consume Ctrl+Backspace so LVGL doesn't also handle it
                if ((wait_event.key.keysym.mod & KMOD_CTRL) &&
                    wait_event.key.keysym.sym == SDLK_BACKSPACE) {
                    consumed = true;
                }
                // When sidebar is visible, handle keyboard input ourselves
                if (g_sidebar.visible) {
                    // Handle backspace for search/rename
                    if (wait_event.key.keysym.sym == SDLK_BACKSPACE) {
                        if (g_sidebar.searching || g_sidebar.renaming) {
                            handle_sidebar_backspace(g_sidebar, g_editor);
                        }
                    }
                    // Handle DELETE key for file deletion (also handle BACKSPACE on Mac when not searching/renaming)
                    if (wait_event.key.keysym.sym == SDLK_DELETE ||
                        (wait_event.key.keysym.sym == SDLK_BACKSPACE && !g_sidebar.searching && !g_sidebar.renaming)) {
                        if (!g_sidebar.searching && !g_sidebar.renaming && 
                            !g_sidebar.confirm_delete && !g_sidebar.confirm_restore) {
                            int regular_count = (int)g_sidebar.filtered_file_list.size();
                            if (!g_sidebar.new_file_selected && 
                                g_sidebar.selected_index >= 0 && 
                                g_sidebar.selected_index < regular_count) {
                                show_delete_dialog(g_sidebar, g_editor, g_sidebar.selected_index);
                            }
                        }
                    }
                    // Consume all keyboard events when sidebar is visible
                    consumed = true;
                }
            }
            
            // Also consume key up events when sidebar is visible
            if (wait_event.type == SDL_KEYUP && g_sidebar.visible) {
                consumed = true;
            }
            
            // Handle text input - intercept when sidebar is visible
            if (wait_event.type == SDL_TEXTINPUT) {
                if (g_sidebar.visible) {
                    if (g_sidebar.searching || g_sidebar.renaming) {
                        handle_sidebar_text_input(g_sidebar, g_editor, wait_event.text.text);
                    }
                    consumed = true;
                }
            }
            
            // Push event back if not consumed, so LVGL can process it
            if (!consumed) {
                SDL_PushEvent(&wait_event);
            }
        }
        
        // Let LVGL process remaining events and render
        lv_timer_handler();
        
        // Ensure keyboard state is updated for SDL_GetKeyboardState
        SDL_PumpEvents();
        
        handle_shortcuts();
        handle_search_navigation();
        handle_text_selection();
        handle_clipboard();
        
        process_pending_saves(g_editor, DEBOUNCE_DELAY);
        process_rename_save(g_sidebar, g_editor, RENAME_DEBOUNCE_DELAY);
    }
}
