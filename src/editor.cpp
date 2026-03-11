#include "editor.h"
#include "theme.h"
#include "search.h"
#include <cstring>
#include <fstream>

int count_words(const char* text, int text_len) {
    int count = 0;
    bool in_word = false;
    for (int i = 0; i < text_len; i++) {
        char c = text[i];
        bool is_whitespace = (c == ' ' || c == '\n' || c == '\t' || c == '\r');
        if (is_whitespace) {
            in_word = false;
        } else if (!in_word) {
            in_word = true;
            count++;
        }
    }
    return count;
}

static const lv_font_t* get_font_for_size_index(int index) {
    static const lv_font_t* fonts[] = {
        &lv_font_montserrat_18,
        &lv_font_montserrat_20,
        &lv_font_montserrat_22,
        &lv_font_montserrat_24,
        &lv_font_montserrat_26,
        &lv_font_montserrat_28,
        &lv_font_montserrat_30,
        &lv_font_montserrat_32,
        &lv_font_montserrat_36,
        &lv_font_montserrat_40,
    };
    constexpr int num_fonts = sizeof(fonts) / sizeof(fonts[0]);
    if (index < 0) index = 0;
    if (index >= num_fonts) index = num_fonts - 1;
    return fonts[index];
}

static void textarea_event_cb(lv_event_t* e) {
    EditorState* state = static_cast<EditorState*>(lv_event_get_user_data(e));
    if (!state) return;
    
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        state->content_pending_save = true;
        state->content_change_time = lv_tick_get();
        update_word_count(*state);
    }
}

void create_editor_ui(EditorState& state, lv_obj_t* parent) {
    const Theme& theme = get_theme(state.dark_theme);
    
    lv_obj_set_style_bg_color(parent, theme.background, 0);
    
    lv_obj_t* main_container = lv_obj_create(parent);
    lv_obj_remove_style_all(main_container);
    lv_obj_set_size(main_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(main_container, theme.background, 0);
    lv_obj_set_style_bg_opa(main_container, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(main_container, 10, 0);
    lv_obj_set_flex_flow(main_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(main_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    state.status_bar = lv_obj_create(main_container);
    lv_obj_remove_style_all(state.status_bar);
    lv_obj_set_size(state.status_bar, LV_PCT(100), 30);
    lv_obj_set_style_bg_color(state.status_bar, theme.background, 0);
    lv_obj_set_style_bg_opa(state.status_bar, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(state.status_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(state.status_bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(state.status_bar, 5, 0);
    
    state.filename_label = lv_label_create(state.status_bar);
    lv_label_set_text(state.filename_label, "Untitled");
    lv_obj_set_style_text_color(state.filename_label, theme.text_dim, 0);
    lv_obj_set_style_text_font(state.filename_label, &lv_font_montserrat_18, 0);
    
    state.search_container = lv_obj_create(state.status_bar);
    lv_obj_remove_style_all(state.search_container);
    lv_obj_set_size(state.search_container, 250, 26);
    lv_obj_set_style_bg_color(state.search_container, theme.search_input_bg, 0);
    lv_obj_set_style_bg_opa(state.search_container, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(state.search_container, 3, 0);
    lv_obj_set_flex_flow(state.search_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(state.search_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(state.search_container, 8, 0);
    lv_obj_add_flag(state.search_container, LV_OBJ_FLAG_HIDDEN);
    
    lv_obj_t* search_icon = lv_label_create(state.search_container);
    lv_label_set_text(search_icon, LV_SYMBOL_EYE_OPEN);
    lv_obj_set_style_text_color(search_icon, theme.text_dim, 0);
    lv_obj_set_style_text_font(search_icon, &lv_font_montserrat_14, 0);
    
    state.search_input = lv_textarea_create(state.search_container);
    lv_obj_set_size(state.search_input, 160, 22);
    lv_textarea_set_one_line(state.search_input, true);
    lv_textarea_set_placeholder_text(state.search_input, "Find...");
    lv_textarea_set_text_selection(state.search_input, true);
    lv_obj_set_style_bg_opa(state.search_input, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(state.search_input, 0, 0);
    lv_obj_set_style_text_color(state.search_input, theme.search_input_text, 0);
    lv_obj_set_style_text_font(state.search_input, &lv_font_montserrat_16, 0);
    lv_obj_set_style_border_color(state.search_input, theme.search_input_text, LV_PART_CURSOR);
    lv_obj_set_style_border_color(state.search_input, theme.search_input_text, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_border_opa(state.search_input, LV_OPA_COVER, LV_PART_CURSOR);
    lv_obj_set_style_border_opa(state.search_input, LV_OPA_COVER, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(state.search_input, 2, LV_PART_CURSOR);
    lv_obj_set_style_border_side(state.search_input, LV_BORDER_SIDE_LEFT, LV_PART_CURSOR);
    lv_obj_set_style_bg_opa(state.search_input, LV_OPA_TRANSP, LV_PART_CURSOR);
    lv_obj_set_style_bg_opa(state.search_input, LV_OPA_TRANSP, LV_PART_CURSOR | LV_STATE_FOCUSED);
    
    state.match_label = lv_label_create(state.search_container);
    lv_label_set_text(state.match_label, "");
    lv_obj_set_style_text_color(state.match_label, theme.text_dim, 0);
    lv_obj_set_style_text_font(state.match_label, &lv_font_montserrat_14, 0);
    
    state.word_count_label = lv_label_create(state.status_bar);
    lv_label_set_text(state.word_count_label, "0 words");
    lv_obj_set_style_text_color(state.word_count_label, theme.text_dim, 0);
    lv_obj_set_style_text_font(state.word_count_label, &lv_font_montserrat_18, 0);
    
    // Battery icon (30x14 body + 3x6 tip)
    const int batt_width = 30;
    const int batt_height = 14;
    const int batt_tip_width = 3;
    const int batt_tip_height = 6;
    
    state.battery_container = lv_obj_create(state.status_bar);
    lv_obj_remove_style_all(state.battery_container);
    lv_obj_set_size(state.battery_container, batt_width + batt_tip_width + 2, batt_height);
    lv_obj_clear_flag(state.battery_container, LV_OBJ_FLAG_SCROLLABLE);
    
    // Battery body (outline)
    state.battery_body = lv_obj_create(state.battery_container);
    lv_obj_remove_style_all(state.battery_body);
    lv_obj_set_size(state.battery_body, batt_width, batt_height);
    lv_obj_set_pos(state.battery_body, 0, 0);
    lv_obj_set_style_border_width(state.battery_body, 1, 0);
    lv_obj_set_style_border_color(state.battery_body, theme.battery_border, 0);
    lv_obj_set_style_radius(state.battery_body, 2, 0);
    lv_obj_set_style_bg_opa(state.battery_body, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(state.battery_body, LV_OBJ_FLAG_SCROLLABLE);
    
    // Battery fill (inside body)
    state.battery_fill = lv_obj_create(state.battery_body);
    lv_obj_remove_style_all(state.battery_fill);
    int fill_width = (batt_width - 4) * state.battery_percent / 100;
    lv_obj_set_size(state.battery_fill, fill_width, batt_height - 4);
    lv_obj_set_pos(state.battery_fill, 2, 2);
    lv_obj_set_style_bg_color(state.battery_fill, 
        state.battery_percent > 20 ? theme.battery_good : theme.battery_low, 0);
    lv_obj_set_style_bg_opa(state.battery_fill, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(state.battery_fill, 0, 0);
    lv_obj_clear_flag(state.battery_fill, LV_OBJ_FLAG_SCROLLABLE);
    
    // Battery tip (right side nub)
    state.battery_tip = lv_obj_create(state.battery_container);
    lv_obj_remove_style_all(state.battery_tip);
    lv_obj_set_size(state.battery_tip, batt_tip_width, batt_tip_height);
    lv_obj_set_pos(state.battery_tip, batt_width, (batt_height - batt_tip_height) / 2);
    lv_obj_set_style_bg_color(state.battery_tip, theme.battery_border, 0);
    lv_obj_set_style_bg_opa(state.battery_tip, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(state.battery_tip, 0, 0);
    lv_obj_clear_flag(state.battery_tip, LV_OBJ_FLAG_SCROLLABLE);
    
    state.textarea = lv_textarea_create(main_container);
    lv_obj_set_size(state.textarea, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_grow(state.textarea, 1);
    lv_textarea_set_placeholder_text(state.textarea, "Start typing...");
    lv_textarea_set_accepted_chars(state.textarea, NULL);
    lv_textarea_set_max_length(state.textarea, TEXT_BUFFER_SIZE - 1);
    lv_textarea_set_text_selection(state.textarea, true);
    
    lv_obj_set_style_bg_color(state.textarea, theme.edit_bg, 0);
    lv_obj_set_style_text_color(state.textarea, theme.edit_text, 0);
    lv_obj_set_style_text_font(state.textarea, get_font_for_size_index(state.font_size_index), 0);
    lv_obj_set_style_border_width(state.textarea, 0, 0);
    lv_obj_set_style_radius(state.textarea, 0, 0);
    lv_obj_set_style_pad_all(state.textarea, 10, 0);
    
    lv_obj_set_style_anim_duration(state.textarea, 500, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(state.textarea, theme.edit_text, LV_PART_CURSOR);
    lv_obj_set_style_border_color(state.textarea, theme.edit_text, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_border_opa(state.textarea, LV_OPA_COVER, LV_PART_CURSOR);
    lv_obj_set_style_border_opa(state.textarea, LV_OPA_COVER, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(state.textarea, 2, LV_PART_CURSOR);
    lv_obj_set_style_border_side(state.textarea, LV_BORDER_SIDE_LEFT, LV_PART_CURSOR);
    lv_obj_set_style_bg_opa(state.textarea, LV_OPA_TRANSP, LV_PART_CURSOR);
    lv_obj_set_style_bg_opa(state.textarea, LV_OPA_TRANSP, LV_PART_CURSOR | LV_STATE_FOCUSED);
    
    lv_obj_set_style_bg_color(state.textarea, theme.scrollbar_bg, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(state.textarea, theme.scrollbar_cursor, LV_PART_SCROLLBAR | LV_STATE_SCROLLED);
    
    lv_obj_t* label = lv_textarea_get_label(state.textarea);
    if (label) {
        lv_obj_set_style_bg_color(label, theme.selection, LV_PART_SELECTED);
        lv_obj_set_style_text_color(label, theme.edit_text, LV_PART_SELECTED);
    }
    
    lv_obj_add_event_cb(state.textarea, textarea_event_cb, LV_EVENT_VALUE_CHANGED, &state);
    
    lv_obj_remove_flag(state.textarea, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_flag(state.textarea, LV_OBJ_FLAG_CLICK_FOCUSABLE);
}

void load_file_into_editor(EditorState& state, const char* file_path) {
    state.current_file_path = file_path;
    state.temp_file_path = std::string(file_path) + ".tmp";
    
    std::ifstream infile(file_path);
    if (infile) {
        std::string content((std::istreambuf_iterator<char>(infile)),
                            std::istreambuf_iterator<char>());
        infile.close();
        
        if (content.size() >= TEXT_BUFFER_SIZE - 1) {
            content = content.substr(0, TEXT_BUFFER_SIZE - 1);
        }
        
        lv_textarea_set_text(state.textarea, content.c_str());
        state.cached_word_count = count_words(content.c_str(), content.size());
    } else {
        lv_textarea_set_text(state.textarea, "");
        state.cached_word_count = 0;
    }
    
    update_filename_display(state);
    update_word_count(state);
    
    if (state.saved_cursor_pos >= 0) {
        lv_textarea_set_cursor_pos(state.textarea, state.saved_cursor_pos);
    }
}

void save_editor_content(EditorState& state) {
    const char* text = lv_textarea_get_text(state.textarea);
    
    std::ofstream outfile(state.temp_file_path, std::ios::binary | std::ios::trunc);
    if (outfile) {
        outfile.write(text, strlen(text));
        bool write_success = outfile.good();
        outfile.close();
        if (write_success) {
            std::rename(state.temp_file_path.c_str(), state.current_file_path.c_str());
        }
    }
}

void save_editor_state(EditorState& state) {
    std::string temp_state_path = state.state_file_path + ".tmp";
    std::ofstream state_out(temp_state_path);
    if (state_out) {
        state_out << state.current_file_path << "\n";
        state_out << lv_textarea_get_cursor_pos(state.textarea) << "\n";
        state_out << state.font_size_index << "\n";
        state_out << (state.dark_theme ? 1 : 0) << "\n";
        bool write_success = state_out.good();
        state_out.close();
        if (write_success) {
            std::rename(temp_state_path.c_str(), state.state_file_path.c_str());
        }
    }
}

void update_editor_theme(EditorState& state, bool dark) {
    state.dark_theme = dark;
    const Theme& theme = get_theme(dark);
    
    lv_obj_t* parent = lv_obj_get_parent(state.textarea);
    if (parent) {
        lv_obj_t* main_container = lv_obj_get_parent(parent);
        if (main_container) {
            lv_obj_set_style_bg_color(main_container, theme.background, 0);
        }
    }
    
    if (state.status_bar) {
        lv_obj_set_style_bg_color(state.status_bar, theme.background, 0);
    }
    
    if (state.filename_label) {
        lv_obj_set_style_text_color(state.filename_label, theme.text_dim, 0);
    }
    
    if (state.word_count_label) {
        lv_obj_set_style_text_color(state.word_count_label, theme.text_dim, 0);
    }
    
    if (state.textarea) {
        lv_obj_set_style_bg_color(state.textarea, theme.edit_bg, 0);
        lv_obj_set_style_text_color(state.textarea, theme.edit_text, 0);
        lv_obj_set_style_border_color(state.textarea, theme.edit_text, LV_PART_CURSOR);
        lv_obj_set_style_border_color(state.textarea, theme.edit_text, LV_PART_CURSOR | LV_STATE_FOCUSED);
        lv_obj_set_style_border_opa(state.textarea, LV_OPA_COVER, LV_PART_CURSOR);
        lv_obj_set_style_border_opa(state.textarea, LV_OPA_COVER, LV_PART_CURSOR | LV_STATE_FOCUSED);
        lv_obj_set_style_bg_opa(state.textarea, LV_OPA_TRANSP, LV_PART_CURSOR);
        lv_obj_set_style_bg_opa(state.textarea, LV_OPA_TRANSP, LV_PART_CURSOR | LV_STATE_FOCUSED);
        lv_obj_set_style_bg_color(state.textarea, theme.scrollbar_bg, LV_PART_SCROLLBAR);
        lv_obj_set_style_bg_color(state.textarea, theme.scrollbar_cursor, LV_PART_SCROLLBAR | LV_STATE_SCROLLED);
        
        lv_obj_t* label = lv_textarea_get_label(state.textarea);
        if (label) {
            lv_obj_set_style_bg_color(label, theme.selection, LV_PART_SELECTED);
            lv_obj_set_style_text_color(label, theme.edit_text, LV_PART_SELECTED);
        }
    }
    
    if (state.search_container) {
        lv_obj_set_style_bg_color(state.search_container, theme.search_input_bg, 0);
    }
    
    if (state.search_input) {
        lv_obj_set_style_text_color(state.search_input, theme.search_input_text, 0);
        lv_obj_set_style_border_color(state.search_input, theme.search_input_text, LV_PART_CURSOR);
        lv_obj_set_style_border_color(state.search_input, theme.search_input_text, LV_PART_CURSOR | LV_STATE_FOCUSED);
        lv_obj_set_style_border_opa(state.search_input, LV_OPA_COVER, LV_PART_CURSOR);
        lv_obj_set_style_border_opa(state.search_input, LV_OPA_COVER, LV_PART_CURSOR | LV_STATE_FOCUSED);
        lv_obj_set_style_bg_opa(state.search_input, LV_OPA_TRANSP, LV_PART_CURSOR);
        lv_obj_set_style_bg_opa(state.search_input, LV_OPA_TRANSP, LV_PART_CURSOR | LV_STATE_FOCUSED);
    }
    
    if (state.battery_body) {
        lv_obj_set_style_border_color(state.battery_body, theme.battery_border, 0);
    }
    if (state.battery_tip) {
        lv_obj_set_style_bg_color(state.battery_tip, theme.battery_border, 0);
    }
    if (state.battery_fill) {
        lv_obj_set_style_bg_color(state.battery_fill, 
            state.battery_percent > 20 ? theme.battery_good : theme.battery_low, 0);
    }
}

void update_word_count(EditorState& state) {
    const char* text = lv_textarea_get_text(state.textarea);
    state.cached_word_count = count_words(text, strlen(text));
    
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d words", state.cached_word_count);
    lv_label_set_text(state.word_count_label, buffer);
}

void update_filename_display(EditorState& state) {
    std::string display_name;
    size_t last_slash = state.current_file_path.find_last_of('/');
    if (last_slash != std::string::npos) {
        display_name = state.current_file_path.substr(last_slash + 1);
    } else {
        display_name = state.current_file_path;
    }
    
    if (display_name.size() > 4 && display_name.substr(display_name.size() - 4) == ".txt") {
        display_name = display_name.substr(0, display_name.size() - 4);
    }
    
    lv_label_set_text(state.filename_label, display_name.c_str());
}

void process_pending_saves(EditorState& state, uint32_t debounce_delay) {
    uint32_t now = lv_tick_get();
    if (state.content_pending_save && (now - state.content_change_time >= debounce_delay)) {
        save_editor_content(state);
        state.content_pending_save = false;
    }
    if (state.state_pending_save && (now - state.state_change_time >= debounce_delay)) {
        save_editor_state(state);
        state.state_pending_save = false;
    }
}

void set_search_input_callback(EditorState& state, lv_event_cb_t cb, void* user_data) {
    if (state.search_input) {
        lv_obj_add_event_cb(state.search_input, cb, LV_EVENT_VALUE_CHANGED, user_data);
    }
}

void update_battery_display(EditorState& state) {
    const Theme& theme = get_theme(state.dark_theme);
    
    const int batt_width = 30;
    
    int fill_width = (batt_width - 4) * state.battery_percent / 100;
    if (fill_width < 0) fill_width = 0;
    
    lv_obj_set_width(state.battery_fill, fill_width);
    lv_obj_set_style_bg_color(state.battery_fill, 
        state.battery_percent > 20 ? theme.battery_good : theme.battery_low, 0);
}
