#pragma once

struct SidebarState;
struct EditorState;

constexpr int FILE_DIALOG_MODE_NEW_FILE = 0;
constexpr int FILE_DIALOG_MODE_MOVE_RENAME_FILE = 1;
constexpr int FILE_DIALOG_MODE_NEW_FOLDER = 2;
constexpr int FILE_DIALOG_MODE_RENAME_FOLDER = 3;

void show_file_dialog(SidebarState& sidebar, EditorState& editor, int mode);
void hide_file_dialog(SidebarState& sidebar);
void handle_file_dialog_keyboard(SidebarState& sidebar, EditorState& editor);
void confirm_file_dialog(SidebarState& sidebar, EditorState& editor);
