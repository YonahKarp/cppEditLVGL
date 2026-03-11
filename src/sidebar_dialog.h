#pragma once

#include "lvgl.h"

struct SidebarState;
struct EditorState;

void show_delete_dialog(SidebarState& sidebar, EditorState& editor, int index);
void hide_delete_dialog(SidebarState& sidebar);
void update_delete_dialog_selection(SidebarState& sidebar, EditorState& editor);
void confirm_delete_action(SidebarState& sidebar, EditorState& editor);

void show_restore_dialog(SidebarState& sidebar, EditorState& editor, int deleted_index);
void hide_restore_dialog(SidebarState& sidebar);
void update_restore_dialog_selection(SidebarState& sidebar, EditorState& editor);
void confirm_restore_action(SidebarState& sidebar, EditorState& editor);
