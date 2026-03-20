#pragma once

#include <cstdint>
#include <string>

struct EditorState;

int count_words(const char* text, int text_len);
void set_word_count(EditorState& state, int count);
void show_pending_word_count(EditorState& state);
void request_word_count(EditorState& state, std::string text);
