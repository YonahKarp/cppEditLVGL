#include "app.h"
#include "editor.h"
#include "sidebar.h"
#include "sidebar_file_dialog.h"
#include "search.h"
#include "theme.h"
#include "file_manager.h"
#include "textarea_utils.h"
#include "text_navigation.h"
#include "platform.h"
#include "src/libs/qrcode/lv_qrcode.h"
#include <fstream>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <string>
#include <vector>
#include <algorithm>

#include "lvgl.h"

static EditorState g_editor;
static SidebarState g_sidebar;
static SearchState g_search;
static lv_group_t* g_input_group = nullptr;
static int32_t g_selection_start = -1;
static bool g_suppress_close_shortcut_text = false;

struct QrExportState {
    bool active = false;
    bool ascii_error = false;
    bool auto_cycle = true;
    uint32_t last_cycle_ms = 0;
    size_t current_index = 0;
    std::vector<std::string> payloads;
    std::string filename;
    std::string error_message;
    lv_obj_t* overlay = nullptr;
    lv_obj_t* title_label = nullptr;
    lv_obj_t* info_label = nullptr;
    lv_obj_t* hint_label = nullptr;
    lv_obj_t* qr_code = nullptr;
};

static QrExportState g_qr_export;

static std::string base64_encode(const std::string& input) {
    static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string output;
    output.reserve(((input.size() + 2) / 3) * 4);

    size_t i = 0;
    while (i + 2 < input.size()) {
        uint32_t n = (static_cast<uint8_t>(input[i]) << 16) |
                     (static_cast<uint8_t>(input[i + 1]) << 8) |
                     static_cast<uint8_t>(input[i + 2]);
        output.push_back(chars[(n >> 18) & 0x3F]);
        output.push_back(chars[(n >> 12) & 0x3F]);
        output.push_back(chars[(n >> 6) & 0x3F]);
        output.push_back(chars[n & 0x3F]);
        i += 3;
    }

    if (i < input.size()) {
        uint32_t n = static_cast<uint8_t>(input[i]) << 16;
        if (i + 1 < input.size()) {
            n |= static_cast<uint8_t>(input[i + 1]) << 8;
        }
        output.push_back(chars[(n >> 18) & 0x3F]);
        output.push_back(chars[(n >> 12) & 0x3F]);
        if (i + 1 < input.size()) {
            output.push_back(chars[(n >> 6) & 0x3F]);
        } else {
            output.push_back('=');
        }
        output.push_back('=');
    }

    return output;
}

static std::string basename_from_path(const std::string& path) {
    if (path.empty()) return "Untitled.txt";
    size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos) return path;
    if (slash + 1 >= path.size()) return "Untitled.txt";
    return path.substr(slash + 1);
}

static bool find_non_ascii_passage(const std::string& text, size_t& out_index, std::string& out_passage) {
    for (size_t i = 0; i < text.size(); i++) {
        if (static_cast<uint8_t>(text[i]) > 127U) {
            out_index = i;
            size_t start = (i > 12) ? (i - 12) : 0;
            size_t end = std::min(text.size(), i + 13);
            std::string passage;
            for (size_t j = start; j < end; j++) {
                uint8_t c = static_cast<uint8_t>(text[j]);
                if (c >= 32U && c <= 126U) {
                    passage.push_back(static_cast<char>(c));
                } else if (c == '\n') {
                    passage += "\\n";
                } else if (c == '\t') {
                    passage += "\\t";
                } else {
                    char hex_buf[8];
                    std::snprintf(hex_buf, sizeof(hex_buf), "<%02X>", c);
                    passage += hex_buf;
                }
            }
            out_passage = passage;
            return true;
        }
    }
    return false;
}

static void close_qr_export_popup() {
    if (g_qr_export.overlay) {
        lv_obj_delete(g_qr_export.overlay);
    }
    g_qr_export = {};
}

static void update_qr_export_popup_content() {
    if (!g_qr_export.active || !g_qr_export.info_label) {
        return;
    }

    if (g_qr_export.ascii_error) {
        lv_label_set_text(g_qr_export.info_label, g_qr_export.error_message.c_str());
        if (g_qr_export.hint_label) {
            lv_label_set_text(g_qr_export.hint_label, "Press Esc to close");
        }
        return;
    }

    if (!g_qr_export.qr_code || g_qr_export.payloads.empty()) {
        return;
    }

    if (g_qr_export.current_index >= g_qr_export.payloads.size()) {
        g_qr_export.current_index = 0;
    }

    const std::string& payload = g_qr_export.payloads[g_qr_export.current_index];
    lv_result_t result = lv_qrcode_update(g_qr_export.qr_code, payload.data(), static_cast<uint32_t>(payload.size()));
    if (result != LV_RESULT_OK) {
        g_qr_export.ascii_error = true;
        g_qr_export.error_message = "Failed to generate QR code payload for this chunk.";
        if (g_qr_export.qr_code) {
            lv_obj_add_flag(g_qr_export.qr_code, LV_OBJ_FLAG_HIDDEN);
        }
        lv_label_set_text(g_qr_export.info_label, g_qr_export.error_message.c_str());
        if (g_qr_export.hint_label) {
            lv_label_set_text(g_qr_export.hint_label, "Press Esc to close");
        }
        return;
    }

    char info_buf[160];
    std::snprintf(
        info_buf,
        sizeof(info_buf),
        "%s | part %zu/%zu | %s",
        g_qr_export.filename.c_str(),
        g_qr_export.current_index + 1,
        g_qr_export.payloads.size(),
        g_qr_export.auto_cycle ? "auto 0.5s" : "manual"
    );
    lv_label_set_text(g_qr_export.info_label, info_buf);
    if (g_qr_export.hint_label) {
        lv_label_set_text(g_qr_export.hint_label, "Arrows: manual  Enter: resume auto  Esc: close");
    }
}

static void step_qr_export(int delta) {
    if (!g_qr_export.active || g_qr_export.ascii_error || g_qr_export.payloads.empty()) {
        return;
    }
    const int count = static_cast<int>(g_qr_export.payloads.size());
    int next_index = static_cast<int>(g_qr_export.current_index) + delta;
    while (next_index < 0) next_index += count;
    next_index %= count;
    g_qr_export.current_index = static_cast<size_t>(next_index);
    update_qr_export_popup_content();
}

static void open_qr_export_popup() {
    close_qr_export_popup();

    g_qr_export.active = true;
    g_qr_export.auto_cycle = true;
    g_qr_export.last_cycle_ms = lv_tick_get();
    g_qr_export.current_index = 0;
    g_qr_export.filename = basename_from_path(g_editor.current_file_path);

    const Theme& theme = get_theme(g_editor.dark_theme);
    lv_obj_t* parent = lv_scr_act();

    g_qr_export.overlay = lv_obj_create(parent);
    lv_obj_remove_style_all(g_qr_export.overlay);
    lv_obj_set_size(g_qr_export.overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(g_qr_export.overlay, theme.background, 0);
    lv_obj_set_style_bg_opa(g_qr_export.overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(g_qr_export.overlay, 16, 0);
    lv_obj_set_style_pad_row(g_qr_export.overlay, 12, 0);
    lv_obj_set_flex_flow(g_qr_export.overlay, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(
        g_qr_export.overlay,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );
    lv_obj_move_foreground(g_qr_export.overlay);

    g_qr_export.title_label = lv_label_create(g_qr_export.overlay);
    lv_obj_set_style_text_color(g_qr_export.title_label, theme.edit_text, 0);
    lv_obj_set_style_text_font(g_qr_export.title_label, &lv_font_montserrat_24, 0);

    const char* editor_text = lv_textarea_get_text(g_editor.textarea);
    std::string text = editor_text ? std::string(editor_text) : std::string();
    size_t non_ascii_index = 0;
    std::string non_ascii_passage;

    if (find_non_ascii_passage(text, non_ascii_index, non_ascii_passage)) {
        g_qr_export.ascii_error = true;
        lv_label_set_text(g_qr_export.title_label, "QR Export Error");

        g_qr_export.info_label = lv_label_create(g_qr_export.overlay);
        lv_obj_set_width(g_qr_export.info_label, LV_PCT(95));
        lv_label_set_long_mode(g_qr_export.info_label, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_color(g_qr_export.info_label, theme.edit_text, 0);
        lv_obj_set_style_text_font(g_qr_export.info_label, &lv_font_montserrat_16, 0);
        g_qr_export.error_message =
            "Current file contains non-ASCII bytes.\n"
            "First non-ASCII byte at position " + std::to_string(non_ascii_index) +
            ".\nPassage: " + non_ascii_passage;
        lv_label_set_text(g_qr_export.info_label, g_qr_export.error_message.c_str());

        g_qr_export.hint_label = lv_label_create(g_qr_export.overlay);
        lv_obj_set_style_text_color(g_qr_export.hint_label, theme.text_dim, 0);
        lv_obj_set_style_text_font(g_qr_export.hint_label, &lv_font_montserrat_14, 0);
        lv_label_set_text(g_qr_export.hint_label, "Press Esc to close");
        return;
    }

    lv_label_set_text(g_qr_export.title_label, "QR Export");

    constexpr size_t kRawChunkBytes = 700;
    size_t total_chunks = (text.empty() ? 1 : (text.size() + kRawChunkBytes - 1) / kRawChunkBytes);
    std::string filename_b64 = base64_encode(g_qr_export.filename);
    g_qr_export.payloads.reserve(total_chunks);
    for (size_t i = 0; i < total_chunks; i++) {
        size_t start = i * kRawChunkBytes;
        std::string chunk = text.substr(start, std::min(kRawChunkBytes, text.size() - start));
        std::string payload = "JT1|" + std::to_string(i + 1) + "/" + std::to_string(total_chunks) +
                              "|" + filename_b64 + "|" + base64_encode(chunk);
        g_qr_export.payloads.push_back(std::move(payload));
    }

    if (text.empty()) {
        g_qr_export.payloads[0] = "JT1|1/1|" + filename_b64 + "|";
    }

    g_qr_export.qr_code = lv_qrcode_create(g_qr_export.overlay);
    int32_t min_side = std::min(lv_obj_get_width(parent), lv_obj_get_height(parent));
    int32_t qr_size = std::max(220, std::min(480, min_side - 140));
    lv_qrcode_set_size(g_qr_export.qr_code, qr_size);
    lv_qrcode_set_dark_color(g_qr_export.qr_code, theme.edit_text);
    lv_qrcode_set_light_color(g_qr_export.qr_code, theme.edit_bg);

    g_qr_export.info_label = lv_label_create(g_qr_export.overlay);
    lv_obj_set_style_text_color(g_qr_export.info_label, theme.edit_text, 0);
    lv_obj_set_style_text_font(g_qr_export.info_label, &lv_font_montserrat_16, 0);

    g_qr_export.hint_label = lv_label_create(g_qr_export.overlay);
    lv_obj_set_style_text_color(g_qr_export.hint_label, theme.text_dim, 0);
    lv_obj_set_style_text_font(g_qr_export.hint_label, &lv_font_montserrat_14, 0);

    update_qr_export_popup_content();
}

static void update_qr_export_autocycle() {
    if (!g_qr_export.active || g_qr_export.ascii_error || !g_qr_export.auto_cycle || g_qr_export.payloads.size() <= 1) {
        return;
    }

    uint32_t now = lv_tick_get();
    if ((now - g_qr_export.last_cycle_ms) < 500) {
        return;
    }
    g_qr_export.last_cycle_ms = now;
    step_qr_export(1);
}

static bool is_close_shortcut_pressed() {
    platform::KeyModifiers modifiers = platform::get_key_modifiers();
    return platform::is_key_pressed(platform::KeyCode::Escape) ||
           (platform::is_key_pressed(platform::KeyCode::Grave) && !modifiers.shift);
}

static bool is_close_shortcut_event(const platform::PlatformEvent& event) {
    return event.type == platform::PlatformEvent::Type::KeyDown &&
           (event.key == platform::KeyCode::Escape ||
            (event.key == platform::KeyCode::Grave && !event.modifiers.shift));
}

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

static void clear_selection_local(lv_obj_t* textarea) {
    if (!textarea) return;
    lv_obj_t* label = lv_textarea_get_label(textarea);
    if (label) {
        lv_label_set_text_selection_start(label, LV_LABEL_TEXT_SELECTION_OFF);
        lv_label_set_text_selection_end(label, LV_LABEL_TEXT_SELECTION_OFF);
        lv_obj_invalidate(textarea);
    }
    g_selection_start = -1;
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

static void delete_selected_text(lv_obj_t* textarea, uint32_t* out_cursor_pos = nullptr) {
    if (textarea_delete_selected_text(textarea, out_cursor_pos)) {
        g_selection_start = -1;
    }
}

static bool has_selection(lv_obj_t* textarea) {
    return textarea_has_selection(textarea);
}

static void handle_clipboard() {
    platform::KeyModifiers modifiers = platform::get_key_modifiers();
    bool ctrl = modifiers.ctrl;
    bool cmd = modifiers.meta;
    
    if (!ctrl && !cmd) return;
    if (g_sidebar.visible) return;
    
    static uint32_t last_clipboard_time = 0;
    uint32_t now = lv_tick_get();
    bool debounce = (now - last_clipboard_time) > 200;
    if (!debounce) return;
    
    lv_obj_t* focused = lv_group_get_focused(g_input_group);
    if (!focused || !lv_obj_check_type(focused, &lv_textarea_class)) return;
    
    // Copy (Ctrl+C or Cmd+C)
    if (platform::is_key_pressed(platform::KeyCode::C)) {
        last_clipboard_time = now;
        std::string selected = get_selected_text(focused);
        if (!selected.empty()) {
            platform::clipboard_set(selected.c_str());
        }
    }
    
    // Cut (Ctrl+X or Cmd+X)
    if (platform::is_key_pressed(platform::KeyCode::X)) {
        last_clipboard_time = now;
        std::string selected = get_selected_text(focused);
        if (!selected.empty()) {
            platform::clipboard_set(selected.c_str());
            delete_selected_text(focused);
            
            if (focused == g_editor.textarea) {
                g_editor.content_pending_save = true;
                g_editor.content_change_time = lv_tick_get();
                update_word_count(g_editor);
            }
        }
    }
    
    // Paste (Ctrl+V or Cmd+V)
    if (platform::is_key_pressed(platform::KeyCode::V)) {
        last_clipboard_time = now;
        char* clipboard = platform::clipboard_get();
        if (clipboard && strlen(clipboard) > 0) {
            size_t max_bytes = (focused == g_editor.textarea) ? static_cast<size_t>(TEXT_BUFFER_SIZE - 1) : 0;
            if (textarea_add_text_with_limit(focused, clipboard, max_bytes) && focused == g_editor.textarea) {
                g_editor.content_pending_save = true;
                g_editor.content_change_time = lv_tick_get();
                update_word_count(g_editor);
            }
        }
        platform::clipboard_free(clipboard);
    }
}

static void handle_text_selection() {
    platform::KeyModifiers modifiers = platform::get_key_modifiers();
    bool shift = modifiers.shift;
    bool ctrl = modifiers.ctrl;
    bool alt = modifiers.alt;
    
    static int32_t last_cursor_pos = -1;
    static bool last_shift = false;
    static bool last_any_arrow = false;
    static uint32_t last_nav_time = 0;
    
    static KeyRepeatState ctrl_up_state;
    static KeyRepeatState ctrl_down_state;
    static KeyRepeatState ctrl_shift_up_state;
    static KeyRepeatState ctrl_shift_down_state;
    static KeyRepeatState ctrl_shift_left_state;
    static KeyRepeatState ctrl_shift_right_state;
    static KeyRepeatState ctrl_backspace_state;
    static KeyRepeatState ctrl_left_state;
    static KeyRepeatState ctrl_right_state;
    static KeyRepeatState alt_left_state;
    static KeyRepeatState alt_right_state;
    
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
    
    // Handle Ctrl+Backspace (delete previous word, or delete selection if present)
    bool ctrl_backspace = ctrl && platform::is_key_pressed(platform::KeyCode::Backspace) && focused == g_editor.textarea;
    if (ctrl_backspace_state.should_act(ctrl_backspace, now)) {
        if (has_selection(focused)) {
            delete_selected_text(focused);
        } else {
            int32_t word_start = find_prev_word_start(text, cur_pos);
            if (word_start < cur_pos) {
                // Delete characters using native LVGL deletion (faster than set_text)
                int32_t chars_to_delete = cur_pos - word_start;
                for (int32_t i = 0; i < chars_to_delete; i++) {
                    lv_textarea_delete_char(focused);
                }
                cur_pos = word_start;
            }
        }
        g_editor.content_pending_save = true;
        g_editor.content_change_time = lv_tick_get();
        update_word_count(g_editor);
        text = lv_textarea_get_text(focused);
        text_len = strlen(text);
    }
    if (ctrl_backspace) return;
    
    // Handle Ctrl+Left/Right (word navigation) with key repeat
    bool ctrl_left = ctrl && !shift && !alt && platform::is_key_pressed(platform::KeyCode::Left);
    bool ctrl_right = ctrl && !shift && !alt && platform::is_key_pressed(platform::KeyCode::Right);
    
    if (ctrl_left_state.should_act(ctrl_left, now)) {
        int32_t new_pos = find_prev_word_start(text, cur_pos);
        lv_textarea_set_cursor_pos(focused, new_pos);
        clear_selection_local(focused);
    }
    
    if (ctrl_right_state.should_act(ctrl_right, now)) {
        int32_t new_pos = find_next_word_end(text, text_len, cur_pos);
        lv_textarea_set_cursor_pos(focused, new_pos);
        clear_selection_local(focused);
    }
    
    if (ctrl_left || ctrl_right) return;
    
    // Handle Alt+Left/Right (line start/end) with key repeat
    bool alt_left = alt && !shift && !ctrl && platform::is_key_pressed(platform::KeyCode::Left);
    bool alt_right = alt && !shift && !ctrl && platform::is_key_pressed(platform::KeyCode::Right);
    
    if (alt_left_state.should_act(alt_left, now)) {
        int32_t new_pos = find_line_start(text, cur_pos);
        lv_textarea_set_cursor_pos(focused, new_pos);
        g_selection_start = -1;
    }
    
    if (alt_right_state.should_act(alt_right, now)) {
        int32_t new_pos = find_line_end(text, text_len, cur_pos);
        lv_textarea_set_cursor_pos(focused, new_pos);
        g_selection_start = -1;
    }
    
    if (alt_left || alt_right) return;
    
    // Handle Alt+Up/Down (jump to top/bottom of document)
    if (alt && !shift && !ctrl && nav_debounce) {
        if (platform::is_key_pressed(platform::KeyCode::Up)) {
            last_nav_time = now;
            lv_textarea_set_cursor_pos(focused, 0);
            g_selection_start = -1;
            return;
        }
        if (platform::is_key_pressed(platform::KeyCode::Down)) {
            last_nav_time = now;
            lv_textarea_set_cursor_pos(focused, text_len);
            g_selection_start = -1;
            return;
        }
    }
    
    // Handle Ctrl+Up/Down (jump to previous/next paragraph) with key repeat
    bool ctrl_up = ctrl && !shift && !alt && platform::is_key_pressed(platform::KeyCode::Up);
    bool ctrl_down = ctrl && !shift && !alt && platform::is_key_pressed(platform::KeyCode::Down);
    
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
    bool ctrl_shift_up = ctrl && shift && !alt && platform::is_key_pressed(platform::KeyCode::Up);
    bool ctrl_shift_down = ctrl && shift && !alt && platform::is_key_pressed(platform::KeyCode::Down);
    bool ctrl_shift_left = ctrl && shift && !alt && platform::is_key_pressed(platform::KeyCode::Left);
    bool ctrl_shift_right = ctrl && shift && !alt && platform::is_key_pressed(platform::KeyCode::Right);
    
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
    
    // Handle Ctrl+Shift+Left/Right (extend selection by word) with key repeat
    if (ctrl_shift_left_state.should_act(ctrl_shift_left, now)) {
        if (g_selection_start < 0) g_selection_start = cur_pos;
        int32_t new_pos = find_prev_word_start(text, cur_pos);
        lv_textarea_set_cursor_pos(focused, new_pos);
        uint32_t sel_start = (g_selection_start < new_pos) ? g_selection_start : new_pos;
        uint32_t sel_end = (g_selection_start < new_pos) ? new_pos : g_selection_start;
        lv_label_set_text_selection_start(label, sel_start);
        lv_label_set_text_selection_end(label, sel_end);
        lv_obj_invalidate(focused);
        last_cursor_pos = new_pos;
    }
    
    if (ctrl_shift_right_state.should_act(ctrl_shift_right, now)) {
        if (g_selection_start < 0) g_selection_start = cur_pos;
        int32_t new_pos = find_next_word_end(text, text_len, cur_pos);
        lv_textarea_set_cursor_pos(focused, new_pos);
        uint32_t sel_start = (g_selection_start < new_pos) ? g_selection_start : new_pos;
        uint32_t sel_end = (g_selection_start < new_pos) ? new_pos : g_selection_start;
        lv_label_set_text_selection_start(label, sel_start);
        lv_label_set_text_selection_end(label, sel_end);
        lv_obj_invalidate(focused);
        last_cursor_pos = new_pos;
    }
    
    if (ctrl_up || ctrl_down || ctrl_shift_up || ctrl_shift_down || ctrl_shift_left || ctrl_shift_right) {
        return;
    }
    
    bool any_arrow = platform::is_key_pressed(platform::KeyCode::Left) || platform::is_key_pressed(platform::KeyCode::Right) ||
                     platform::is_key_pressed(platform::KeyCode::Up) || platform::is_key_pressed(platform::KeyCode::Down) ||
                     platform::is_key_pressed(platform::KeyCode::Home) || platform::is_key_pressed(platform::KeyCode::End);
    
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
            clear_selection_local(g_editor.textarea);
            focus_editor_textarea();
        } else if (g_sidebar.visible) {
            hide_sidebar(g_sidebar, g_editor);
            focus_editor_textarea();
        }
    }
}

static void handle_sidebar_keyboard() {
    if (!g_sidebar.visible) return;
    
    bool ctrl = platform::get_key_modifiers().ctrl;
    static uint32_t last_nav_time = 0;
    uint32_t now = lv_tick_get();
    bool nav_debounce = (now - last_nav_time) > 120;
    
    if (g_sidebar.file_dialog_active) {
        handle_file_dialog_keyboard(g_sidebar, g_editor);
        return;
    }
    
    // Skip processing for a short time after file dialog closes
    // to prevent Enter key from triggering sidebar actions
    if (g_sidebar.file_dialog_close_time > 0 && 
        (now - g_sidebar.file_dialog_close_time) < 200) {
        return;
    }
    
    // Handle Ctrl+F to toggle search
    if (ctrl && nav_debounce && platform::is_key_pressed(platform::KeyCode::F)) {
        last_nav_time = now;
        toggle_sidebar_search(g_sidebar, g_editor);
        return;
    }
    
    // Handle restore dialog
    if (g_sidebar.confirm_restore) {
        if (nav_debounce) {
            if (platform::is_key_pressed(platform::KeyCode::Left)) {
                last_nav_time = now;
                g_sidebar.dialog_selection = 0;
                update_restore_dialog_selection(g_sidebar, g_editor);
            }
            
            if (platform::is_key_pressed(platform::KeyCode::Right)) {
                last_nav_time = now;
                g_sidebar.dialog_selection = 1;
                update_restore_dialog_selection(g_sidebar, g_editor);
            }
            
            if (platform::is_key_pressed(platform::KeyCode::Enter)) {
                last_nav_time = now;
                confirm_restore_action(g_sidebar, g_editor);
            }
            
            if (is_close_shortcut_pressed()) {
                last_nav_time = now;
                hide_restore_dialog(g_sidebar);
            }
        }
        return;
    }
    
    // Handle delete dialog
    if (g_sidebar.confirm_delete) {
        if (nav_debounce) {
            if (platform::is_key_pressed(platform::KeyCode::Left)) {
                last_nav_time = now;
                g_sidebar.dialog_selection = 0;
                update_delete_dialog_selection(g_sidebar, g_editor);
            }
            
            if (platform::is_key_pressed(platform::KeyCode::Right)) {
                last_nav_time = now;
                g_sidebar.dialog_selection = 1;
                update_delete_dialog_selection(g_sidebar, g_editor);
            }
            
            if (platform::is_key_pressed(platform::KeyCode::Enter)) {
                last_nav_time = now;
                confirm_delete_action(g_sidebar, g_editor);
            }
            
            if (is_close_shortcut_pressed()) {
                last_nav_time = now;
                hide_delete_dialog(g_sidebar);
            }
        }
        return;
    }
    
    // Build display items first to get accurate count
    g_sidebar.folder_data.build_display_items(g_sidebar.searching);
    int display_count = (int)g_sidebar.folder_data.display_items.size();
    int deleted_count = g_sidebar.searching ? (int)g_sidebar.filtered_deleted_list.size() : 0;
    int total_count = display_count + deleted_count;
    
    if (nav_debounce) {
        if (platform::is_key_pressed(platform::KeyCode::Up)) {
            last_nav_time = now;
            if (g_sidebar.renaming) {
                if (g_sidebar.rename_textarea) {
                    lv_group_remove_obj(g_sidebar.rename_textarea);
                }
                cancel_rename(g_sidebar, g_editor);
            }
            if (g_sidebar.new_folder_selected || g_sidebar.new_file_selected) {
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
        
        if (platform::is_key_pressed(platform::KeyCode::Down)) {
            last_nav_time = now;
            if (g_sidebar.renaming) {
                if (g_sidebar.rename_textarea) {
                    lv_group_remove_obj(g_sidebar.rename_textarea);
                }
                cancel_rename(g_sidebar, g_editor);
            }
            if (g_sidebar.new_folder_selected) {
                g_sidebar.new_folder_selected = false;
                g_sidebar.selected_index = 0;
                refresh_file_list_ui(g_sidebar, g_editor);
            } else if (g_sidebar.new_file_selected) {
                g_sidebar.new_file_selected = false;
                g_sidebar.selected_index = 0;
                refresh_file_list_ui(g_sidebar, g_editor);
            } else if (g_sidebar.selected_index < total_count - 1) {
                g_sidebar.selected_index++;
                refresh_file_list_ui(g_sidebar, g_editor);
            }
        }
        
        // Left arrow: navigate from new_file to new_folder
        if (platform::is_key_pressed(platform::KeyCode::Left) && !g_sidebar.searching && !g_sidebar.renaming) {
            if (g_sidebar.new_file_selected) {
                last_nav_time = now;
                g_sidebar.new_file_selected = false;
                g_sidebar.new_folder_selected = true;
                refresh_file_list_ui(g_sidebar, g_editor);
            }
        }
        
        // Right arrow: navigate from new_folder to new_file, or start rename on file/folder
        if (platform::is_key_pressed(platform::KeyCode::Right) && !g_sidebar.searching) {
            if (g_sidebar.new_folder_selected) {
                last_nav_time = now;
                g_sidebar.new_folder_selected = false;
                g_sidebar.new_file_selected = true;
                refresh_file_list_ui(g_sidebar, g_editor);
            } else if (!g_sidebar.new_file_selected && !g_sidebar.renaming && 
                       g_sidebar.selected_index >= 0 && g_sidebar.selected_index < display_count) {
                last_nav_time = now;
                const SidebarItem& item = g_sidebar.folder_data.display_items[g_sidebar.selected_index];
                if (item.is_folder()) {
                    show_file_dialog(g_sidebar, g_editor, FILE_DIALOG_MODE_RENAME_FOLDER);
                    if (g_sidebar.file_dialog_name_input) {
                        lv_group_add_obj(g_input_group, g_sidebar.file_dialog_name_input);
                        lv_group_focus_obj(g_sidebar.file_dialog_name_input);
                    }
                } else {
                    start_rename(g_sidebar, g_editor);
                    if (g_sidebar.rename_textarea) {
                        lv_group_add_obj(g_input_group, g_sidebar.rename_textarea);
                        lv_group_focus_obj(g_sidebar.rename_textarea);
                    }
                }
            }
        }
        
        // Tab key: rename selected file/folder
        if (platform::is_key_pressed(platform::KeyCode::Tab) && !g_sidebar.searching && !g_sidebar.renaming && 
            !g_sidebar.new_file_selected && !g_sidebar.new_folder_selected) {
            last_nav_time = now;
            if (g_sidebar.selected_index >= 0 && g_sidebar.selected_index < display_count) {
                const SidebarItem& item = g_sidebar.folder_data.display_items[g_sidebar.selected_index];
                if (item.is_folder()) {
                    show_file_dialog(g_sidebar, g_editor, FILE_DIALOG_MODE_RENAME_FOLDER);
                    if (g_sidebar.file_dialog_name_input) {
                        lv_group_add_obj(g_input_group, g_sidebar.file_dialog_name_input);
                        lv_group_focus_obj(g_sidebar.file_dialog_name_input);
                    }
                } else {
                    show_file_dialog(g_sidebar, g_editor, FILE_DIALOG_MODE_MOVE_RENAME_FILE);
                    // Add the name input to the input group and focus it
                    if (g_sidebar.file_dialog_name_input) {
                        lv_group_add_obj(g_input_group, g_sidebar.file_dialog_name_input);
                        lv_group_focus_obj(g_sidebar.file_dialog_name_input);
                    }
                }
            }
            return;
        }
        
        if (platform::is_key_pressed(platform::KeyCode::Enter)) {
            last_nav_time = now;
            if (g_sidebar.renaming) {
                if (g_sidebar.rename_textarea) {
                    lv_group_remove_obj(g_sidebar.rename_textarea);
                }
                commit_rename(g_sidebar, g_editor);
            } else if (g_sidebar.new_folder_selected) {
                show_file_dialog(g_sidebar, g_editor, FILE_DIALOG_MODE_NEW_FOLDER);
                if (g_sidebar.file_dialog_name_input) {
                    lv_group_add_obj(g_input_group, g_sidebar.file_dialog_name_input);
                    lv_group_focus_obj(g_sidebar.file_dialog_name_input);
                }
            } else if (g_sidebar.new_file_selected) {
                show_file_dialog(g_sidebar, g_editor, FILE_DIALOG_MODE_NEW_FILE);
                // Add the name input to the input group and focus it
                if (g_sidebar.file_dialog_name_input) {
                    lv_group_add_obj(g_input_group, g_sidebar.file_dialog_name_input);
                    lv_group_focus_obj(g_sidebar.file_dialog_name_input);
                }
            } else if (g_sidebar.selected_index >= display_count && g_sidebar.searching) {
                int deleted_index = g_sidebar.selected_index - display_count;
                show_restore_dialog(g_sidebar, g_editor, deleted_index);
            } else if (g_sidebar.selected_index >= 0 && g_sidebar.selected_index < display_count) {
                const SidebarItem& item = g_sidebar.folder_data.display_items[g_sidebar.selected_index];
                if (item.is_file()) {
                    SidebarFileEntry entry;
                    entry.filename = item.name;
                    entry.folder = item.folder;
                    switch_to_file_entry(g_sidebar, g_editor, entry);
                    hide_sidebar(g_sidebar, g_editor);
                    focus_editor_textarea();
                } else {
                    // Toggle folder expansion on Enter (collapsed_folders tracks closed folders)
                    if (g_sidebar.folder_data.collapsed_folders.find(item.name) != 
                        g_sidebar.folder_data.collapsed_folders.end()) {
                        g_sidebar.folder_data.collapsed_folders.erase(item.name);
                    } else {
                        g_sidebar.folder_data.collapsed_folders.insert(item.name);
                    }
                    g_editor.state_pending_save = true;
                    g_editor.state_change_time = lv_tick_get();
                    refresh_file_list_ui(g_sidebar, g_editor);
                }
            }
        }
        
        // Delete key shows delete dialog (for files and folders)
        if (platform::is_key_pressed(platform::KeyCode::Delete) && !g_sidebar.searching && !g_sidebar.renaming) {
            last_nav_time = now;
            if (!g_sidebar.new_file_selected && !g_sidebar.new_folder_selected &&
                g_sidebar.selected_index >= 0 && g_sidebar.selected_index < display_count) {
                show_delete_dialog(g_sidebar, g_editor, g_sidebar.selected_index);
            }
        }
        
        if (is_close_shortcut_pressed()) {
            last_nav_time = now;
            if (g_sidebar.renaming) {
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
    
    static uint32_t last_nav_time = 0;
    uint32_t now = lv_tick_get();
    bool debounce = (now - last_nav_time) > 150;
    
    if (!debounce) return;
    
    // Handle ESC to close search
    if (is_close_shortcut_pressed()) {
        last_nav_time = now;
        close_search(g_search);
        lv_obj_add_flag(g_editor.search_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(g_editor.word_count_label, LV_OBJ_FLAG_HIDDEN);
        lv_textarea_set_text(g_editor.search_input, "");
        clear_selection_local(g_editor.textarea);
        focus_editor_textarea();
        return;
    }
    
    lv_obj_t* focused = lv_group_get_focused(g_input_group);
    if (focused != g_editor.search_input) return;
    
    bool shift = platform::get_key_modifiers().shift;
    
    if (platform::is_key_pressed(platform::KeyCode::Enter)) {
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

    if (g_qr_export.active) {
        return;
    }
    
    platform::KeyModifiers modifiers = platform::get_key_modifiers();
    bool meta_pressed = modifiers.meta;
    bool ctrl = modifiers.ctrl;
    
    uint32_t now = lv_tick_get();
    bool debounce = (now - last_key_time) > 150;
    
    if (meta_pressed && !last_meta) {
        if (g_sidebar.file_dialog_active) {
            hide_file_dialog(g_sidebar);
            hide_sidebar(g_sidebar, g_editor);
            focus_editor_textarea();
        } else {
            toggle_sidebar(g_sidebar, g_editor);
            if (g_sidebar.visible) {
                focus_sidebar_search();
            } else {
                focus_editor_textarea();
            }
        }
    }
    
    if (g_sidebar.visible) {
        handle_sidebar_keyboard();
    }
    
    if (ctrl && debounce) {
        if (platform::is_key_pressed(platform::KeyCode::P)) {
            last_key_time = now;
            open_qr_export_popup();
            return;
        }

        if (platform::is_key_pressed(platform::KeyCode::A) && !g_sidebar.visible) {
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
        
        if (platform::is_key_pressed(platform::KeyCode::F) && !g_sidebar.visible) {
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
                clear_selection_local(g_editor.textarea);
                focus_editor_textarea();
            }
        }
        
        if (platform::is_key_pressed(platform::KeyCode::T)) {
            last_key_time = now;
            g_editor.dark_theme = !g_editor.dark_theme;
            update_editor_theme(g_editor, g_editor.dark_theme);
            update_sidebar_theme(g_sidebar, g_editor.dark_theme);
            g_editor.state_pending_save = true;
            g_editor.state_change_time = lv_tick_get();
        }
        
        if (platform::is_key_pressed(platform::KeyCode::Equals)) {
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
        
        if (platform::is_key_pressed(platform::KeyCode::Minus)) {
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

    if (!platform::init(app.window_width, app.window_height)) {
        fprintf(stderr, "Failed to initialize platform backend\n");
        lv_deinit();
        return false;
    }

    app.display = platform::get_display();
    app.keyboard_indev = platform::get_keyboard_indev();
    if (!app.display) {
        fprintf(stderr, "Failed to create platform display\n");
        platform::shutdown();
        lv_deinit();
        return false;
    }
    
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
    load_collapsed_folders(state_file_path, g_sidebar.folder_data.collapsed_folders);
    
    create_editor_ui(g_editor, app.screen);
    start_editor_background_tasks(g_editor);
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
    (void)app;

    if (g_editor.content_pending_save) {
        save_editor_content(g_editor);
    }
    if (g_editor.state_pending_save) {
        save_editor_state(g_editor, g_sidebar.folder_data.collapsed_folders);
    }
    stop_editor_background_tasks(g_editor);
    
    if (g_input_group) {
        lv_group_delete(g_input_group);
        g_input_group = nullptr;
    }

    close_qr_export_popup();
    
    lv_deinit();
    platform::shutdown();
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
    
    if (app.keyboard_indev) {
        lv_indev_set_group(app.keyboard_indev, g_input_group);
    }
    
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
        platform::PlatformEvent wait_event {};
        bool had_event = platform::poll_event(wait_event, wait_time);
        
        // Process the event we received
        if (had_event) {
            bool consumed = false;
            
            if (wait_event.type == platform::PlatformEvent::Type::Quit) {
                running = false;
                consumed = true;
            }

            if (!consumed && g_qr_export.active) {
                if (wait_event.type == platform::PlatformEvent::Type::KeyDown) {
                    if (wait_event.key == platform::KeyCode::Escape) {
                        close_qr_export_popup();
                    } else if (wait_event.key == platform::KeyCode::Left ||
                               wait_event.key == platform::KeyCode::Up) {
                        g_qr_export.auto_cycle = false;
                        step_qr_export(-1);
                    } else if (wait_event.key == platform::KeyCode::Right ||
                               wait_event.key == platform::KeyCode::Down) {
                        g_qr_export.auto_cycle = false;
                        step_qr_export(1);
                    } else if (wait_event.key == platform::KeyCode::Enter) {
                        g_qr_export.auto_cycle = true;
                        g_qr_export.last_cycle_ms = lv_tick_get();
                        update_qr_export_popup_content();
                    }
                    consumed = true;
                } else if (wait_event.type == platform::PlatformEvent::Type::TextInput ||
                           wait_event.type == platform::PlatformEvent::Type::KeyUp) {
                    consumed = true;
                }
            }

            if (!consumed && !g_qr_export.active && wait_event.type == platform::PlatformEvent::Type::KeyDown) {
                // Check for force quit shortcut
                if (wait_event.key == platform::KeyCode::Escape &&
                    wait_event.modifiers.meta &&
                    wait_event.modifiers.shift &&
                    wait_event.modifiers.alt) {
                    running = false;
                    consumed = true;
                }
                if (is_close_shortcut_event(wait_event) &&
                    wait_event.key == platform::KeyCode::Grave &&
                    (g_search.active || g_sidebar.visible)) {
                    g_suppress_close_shortcut_text = true;
                }
                // Consume Ctrl+Arrow keys so LVGL doesn't also handle them
                if (wait_event.modifiers.ctrl &&
                    (wait_event.key == platform::KeyCode::Up ||
                     wait_event.key == platform::KeyCode::Down ||
                     wait_event.key == platform::KeyCode::Left ||
                     wait_event.key == platform::KeyCode::Right)) {
                    consumed = true;
                }
                // Consume Alt+Arrow keys so LVGL doesn't also handle them
                if (wait_event.modifiers.alt &&
                    (wait_event.key == platform::KeyCode::Left ||
                     wait_event.key == platform::KeyCode::Right)) {
                    consumed = true;
                }
                // Consume Ctrl+Backspace so LVGL doesn't also handle it
                if (wait_event.modifiers.ctrl &&
                    wait_event.key == platform::KeyCode::Backspace) {
                    consumed = true;
                }
                // Handle backspace/delete with selection in editor (delete selected text)
                if (!g_sidebar.visible && 
                    (wait_event.key == platform::KeyCode::Backspace || wait_event.key == platform::KeyCode::Delete) &&
                    !wait_event.modifiers.ctrl) {
                    lv_obj_t* focused = lv_group_get_focused(g_input_group);
                    if (focused == g_editor.textarea && has_selection(focused)) {
                        delete_selected_text(focused);
                        g_editor.content_pending_save = true;
                        g_editor.content_change_time = lv_tick_get();
                        update_word_count(g_editor);
                        consumed = true;
                    }
                }
                // When sidebar is visible, handle keyboard input ourselves
                if (g_sidebar.visible) {
                    // If file dialog is active, handle input appropriately
                    if (g_sidebar.file_dialog_active) {
                        // When text field is selected (selection == 0), let LVGL handle text input
                        // but consume Enter and Delete keys to prevent sidebar actions
                        // Note: Backspace is NOT consumed so LVGL textarea can delete characters
                        if (g_sidebar.file_dialog_selection == 0) {
                            if (wait_event.key == platform::KeyCode::Enter ||
                                wait_event.key == platform::KeyCode::Delete) {
                                consumed = true;
                            }
                            // Don't consume other keys - let LVGL textarea handle input
                        } else {
                            // For dropdown and buttons, consume input
                            consumed = true;
                        }
                    } else {
                        // Handle backspace for search (renaming uses LVGL textarea)
                        if (wait_event.key == platform::KeyCode::Backspace) {
                            if (g_sidebar.searching) {
                                handle_sidebar_backspace(g_sidebar, g_editor);
                            }
                        }
                        // Handle DELETE key for file deletion (also handle BACKSPACE on Mac when not searching/renaming)
                        if (wait_event.key == platform::KeyCode::Delete ||
                            (wait_event.key == platform::KeyCode::Backspace && !g_sidebar.searching && !g_sidebar.renaming)) {
                            if (!g_sidebar.searching && !g_sidebar.renaming && 
                                !g_sidebar.confirm_delete && !g_sidebar.confirm_restore) {
                                g_sidebar.folder_data.build_display_items(g_sidebar.searching);
                                int display_count = (int)g_sidebar.folder_data.display_items.size();
                                if (!g_sidebar.new_file_selected && !g_sidebar.new_folder_selected &&
                                    g_sidebar.selected_index >= 0 && 
                                    g_sidebar.selected_index < display_count) {
                                    show_delete_dialog(g_sidebar, g_editor, g_sidebar.selected_index);
                                }
                            }
                        }
                        // Consume keyboard events when sidebar is visible, except when renaming
                        // (rename textarea needs to receive input from LVGL)
                        if (!g_sidebar.renaming) {
                            consumed = true;
                        }
                    }
                }
            }
            
            // Also consume key up events when sidebar is visible
            if (!consumed && !g_qr_export.active &&
                wait_event.type == platform::PlatformEvent::Type::KeyUp &&
                g_sidebar.visible) {
                consumed = true;
            }
            
            // Handle text input - intercept when sidebar is visible
            if (!consumed && !g_qr_export.active && wait_event.type == platform::PlatformEvent::Type::TextInput) {
                if (g_suppress_close_shortcut_text && strcmp(wait_event.text, "`") == 0) {
                    g_suppress_close_shortcut_text = false;
                    consumed = true;
                } else if (g_sidebar.visible && (g_sidebar.searching || g_sidebar.renaming)) {
                    handle_sidebar_text_input(g_sidebar, g_editor, wait_event.text);
                    consumed = true;
                } else {
                    lv_obj_t* focused = lv_group_get_focused(g_input_group);
                    if (focused && lv_obj_check_type(focused, &lv_textarea_class) && wait_event.text[0] != '\0') {
                        size_t max_bytes = (focused == g_editor.textarea) ? static_cast<size_t>(TEXT_BUFFER_SIZE - 1) : 0;
                        if (textarea_add_text_with_limit(focused, wait_event.text, max_bytes) &&
                            focused == g_editor.textarea) {
                            g_editor.content_pending_save = true;
                            g_editor.content_change_time = lv_tick_get();
                            update_word_count(g_editor);
                        }
                    }
                    consumed = true;
                }
            } else if (!consumed && !g_qr_export.active &&
                       wait_event.type == platform::PlatformEvent::Type::KeyUp &&
                       wait_event.key == platform::KeyCode::Grave) {
                g_suppress_close_shortcut_text = false;
            }
            
            // Push event back if not consumed, so LVGL can process it
            if (!consumed) {
                platform::requeue_last_event();
            }
        }
        
        // Let LVGL process remaining events and render
        lv_timer_handler();
        
        platform::pump_events();

        update_qr_export_autocycle();
        
        if (!g_qr_export.active) {
            handle_shortcuts();
            handle_search_navigation();
            handle_text_selection();
            handle_clipboard();
        }
        process_pending_word_count(g_editor, 120);
        
        process_pending_saves(g_editor, DEBOUNCE_DELAY, g_sidebar.folder_data.collapsed_folders);
        process_rename_save(g_sidebar, g_editor, RENAME_DEBOUNCE_DELAY);
    }
}
