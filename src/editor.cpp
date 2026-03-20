#include "editor.h"
#include "editor_word_count.h"
#include "theme.h"
#include "search.h"
#include <utility>
#include <cstring>
#include <fstream>

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
    if (state->suppress_change_tracking) return;
    
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
    
    create_magnifying_glass_icon(state.search_icon, state.search_container, 16, theme.text_dim);
    
    state.search_input = lv_textarea_create(state.search_container);
    lv_obj_set_size(state.search_input, 160, 22);
    lv_textarea_set_one_line(state.search_input, true);
    lv_textarea_set_placeholder_text(state.search_input, "Find...");
    lv_textarea_set_text_selection(state.search_input, true);
    lv_obj_set_style_bg_opa(state.search_input, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(state.search_input, 0, 0);
    lv_obj_set_style_border_width(state.search_input, 0, LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(state.search_input, 0, LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_width(state.search_input, 0, 0);
    lv_obj_set_style_outline_width(state.search_input, 0, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(state.search_input, 0, LV_STATE_FOCUS_KEY);
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
    
    lv_color_t fill_color = state.battery.percent > 20 ? theme.battery_good : theme.battery_low;
    create_battery_icon(state.battery, state.status_bar, state.battery.percent, theme.battery_border, fill_color);
    
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
    lv_obj_set_style_border_width(state.textarea, 0, LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(state.textarea, 0, LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_width(state.textarea, 0, 0);
    lv_obj_set_style_outline_width(state.textarea, 0, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(state.textarea, 0, LV_STATE_FOCUS_KEY);
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
    state.suppress_change_tracking = true;
    
    std::ifstream infile(file_path);
    if (infile) {
        std::string content((std::istreambuf_iterator<char>(infile)),
                            std::istreambuf_iterator<char>());
        infile.close();
        
        if (content.size() >= TEXT_BUFFER_SIZE - 1) {
            content = content.substr(0, TEXT_BUFFER_SIZE - 1);
        }
        
        lv_textarea_set_text(state.textarea, content.c_str());
        show_pending_word_count(state);
        request_word_count(state, std::move(content));
    } else {
        lv_textarea_set_text(state.textarea, "");
        show_pending_word_count(state);
        request_word_count(state, std::string());
    }
    
    update_filename_display(state);
    
    if (state.saved_cursor_pos >= 0) {
        lv_textarea_set_cursor_pos(state.textarea, state.saved_cursor_pos);
        state.saved_cursor_pos = -1;
    }

    state.suppress_change_tracking = false;
    state.content_pending_save = false;
    state.content_change_time = 0;
}

void save_editor_content(EditorState& state) {
    // Don't save if no file is open (e.g., after deleting the current file)
    if (state.current_file_path.empty() || state.temp_file_path.empty()) {
        return;
    }
    
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

void flush_editor_content(EditorState& state) {
    if (!state.content_pending_save) {
        return;
    }

    save_editor_content(state);
    state.content_pending_save = false;
}

void save_editor_state(EditorState& state, const std::set<std::string>& collapsed_folders) {
    std::string temp_state_path = state.state_file_path + ".tmp";
    std::ofstream state_out(temp_state_path);
    if (state_out) {
        state_out << state.current_file_path << "\n";
        state_out << lv_textarea_get_cursor_pos(state.textarea) << "\n";
        state_out << state.font_size_index << "\n";
        state_out << (state.dark_theme ? 1 : 0) << "\n";
        state_out << collapsed_folders.size() << "\n";
        for (const auto& folder : collapsed_folders) {
            state_out << folder << "\n";
        }
        bool write_success = state_out.good();
        state_out.close();
        if (write_success) {
            std::rename(temp_state_path.c_str(), state.state_file_path.c_str());
        }
    }
}

void load_collapsed_folders(const std::string& state_file_path, std::set<std::string>& collapsed_folders) {
    collapsed_folders.clear();
    std::ifstream state_in(state_file_path);
    if (!state_in) return;
    
    std::string line;
    std::getline(state_in, line);  // current_file_path
    std::getline(state_in, line);  // cursor_pos
    std::getline(state_in, line);  // font_size_index
    std::getline(state_in, line);  // dark_theme
    
    int folder_count = 0;
    if (state_in >> folder_count) {
        state_in.ignore();  // skip newline
        for (int i = 0; i < folder_count; i++) {
            if (std::getline(state_in, line) && !line.empty()) {
                collapsed_folders.insert(line);
            }
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
    
    if (state.search_icon.canvas) {
        update_magnifying_glass_icon_color(state.search_icon, theme.text_dim);
    }
    
    if (state.battery.body) {
        lv_color_t fill_color = state.battery.percent > 20 ? theme.battery_good : theme.battery_low;
        update_battery_icon_theme(state.battery, theme.battery_border, fill_color);
    }
}

void update_filename_display(EditorState& state) {
    std::string display_name;
    
    std::string user_files_dir = state.user_files_dir;
    if (state.current_file_path.find(user_files_dir) == 0) {
        std::string relative = state.current_file_path.substr(user_files_dir.size());
        if (!relative.empty() && relative[0] == '/') {
            relative = relative.substr(1);
        }
        
        size_t slash_pos = relative.find('/');
        if (slash_pos != std::string::npos) {
            std::string folder = relative.substr(0, slash_pos);
            std::string filename = relative.substr(slash_pos + 1);
            
            if (filename.size() > 4 && filename.substr(filename.size() - 4) == ".txt") {
                filename = filename.substr(0, filename.size() - 4);
            }
            
            display_name = folder + " / " + filename;
        } else {
            display_name = relative;
            if (display_name.size() > 4 && display_name.substr(display_name.size() - 4) == ".txt") {
                display_name = display_name.substr(0, display_name.size() - 4);
            }
        }
    } else {
        size_t last_slash = state.current_file_path.find_last_of('/');
        if (last_slash != std::string::npos) {
            display_name = state.current_file_path.substr(last_slash + 1);
        } else {
            display_name = state.current_file_path;
        }
        
        if (display_name.size() > 4 && display_name.substr(display_name.size() - 4) == ".txt") {
            display_name = display_name.substr(0, display_name.size() - 4);
        }
    }
    
    lv_label_set_text(state.filename_label, display_name.c_str());
}

void process_pending_saves(EditorState& state, uint32_t debounce_delay, const std::set<std::string>& collapsed_folders) {
    uint32_t now = lv_tick_get();
    if (state.content_pending_save && (now - state.content_change_time >= debounce_delay)) {
        save_editor_content(state);
        state.content_pending_save = false;
    }
    if (state.state_pending_save && (now - state.state_change_time >= debounce_delay)) {
        save_editor_state(state, collapsed_folders);
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
    
    lv_color_t fill_color = state.battery.percent > 20 ? theme.battery_good : theme.battery_low;
    update_battery_icon(state.battery, state.battery.percent, fill_color);
}
