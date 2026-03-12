#pragma once

struct SidebarState;
struct EditorState;

void show_file_dialog(SidebarState& sidebar, EditorState& editor, int mode);
void hide_file_dialog(SidebarState& sidebar);
void handle_file_dialog_keyboard(SidebarState& sidebar, EditorState& editor);
void confirm_file_dialog(SidebarState& sidebar, EditorState& editor);
