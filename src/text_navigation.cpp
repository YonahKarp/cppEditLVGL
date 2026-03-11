#include "text_navigation.h"

int32_t find_paragraph_start(const char* text, int32_t pos) {
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

int32_t find_paragraph_end(const char* text, int32_t text_len, int32_t pos) {
    while (pos < text_len && text[pos] != '\n') pos++;
    if (pos < text_len) pos++;
    while (pos < text_len && text[pos] != '\n') pos++;
    return pos;
}

int32_t find_prev_word_start(const char* text, int32_t pos) {
    if (pos <= 0) return 0;
    pos--;
    while (pos > 0 && (text[pos] == ' ' || text[pos] == '\n' || text[pos] == '\t')) pos--;
    while (pos > 0 && text[pos - 1] != ' ' && text[pos - 1] != '\n' && text[pos - 1] != '\t') pos--;
    return pos;
}

int32_t find_next_word_end(const char* text, int32_t text_len, int32_t pos) {
    if (pos >= text_len) return text_len;
    while (pos < text_len && (text[pos] == ' ' || text[pos] == '\n' || text[pos] == '\t')) pos++;
    while (pos < text_len && text[pos] != ' ' && text[pos] != '\n' && text[pos] != '\t') pos++;
    return pos;
}

int32_t find_line_start(const char* text, int32_t pos) {
    if (pos <= 0) return 0;
    pos--;
    while (pos > 0 && text[pos] != '\n') pos--;
    if (text[pos] == '\n') pos++;
    return pos;
}

int32_t find_line_end(const char* text, int32_t text_len, int32_t pos) {
    while (pos < text_len && text[pos] != '\n') pos++;
    return pos;
}

bool KeyRepeatState::should_act(bool is_pressed, uint32_t now) {
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
