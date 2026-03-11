#pragma once

#include <cstdint>

// Pure text navigation functions - no LVGL dependencies

int32_t find_paragraph_start(const char* text, int32_t pos);
int32_t find_paragraph_end(const char* text, int32_t text_len, int32_t pos);
int32_t find_prev_word_start(const char* text, int32_t pos);
int32_t find_next_word_end(const char* text, int32_t text_len, int32_t pos);
int32_t find_line_start(const char* text, int32_t pos);
int32_t find_line_end(const char* text, int32_t text_len, int32_t pos);

struct KeyRepeatState {
    bool last_pressed = false;
    bool acted_on_press = false;
    uint32_t press_time = 0;
    uint32_t last_action_time = 0;
    
    static constexpr uint32_t INITIAL_DELAY = 800;
    static constexpr uint32_t REPEAT_DELAY = 80;
    
    bool should_act(bool is_pressed, uint32_t now);
};
