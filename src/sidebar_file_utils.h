#pragma once

#include "sidebar_folder.h"
#include <string>

struct SidebarState;
struct EditorState;

void switch_to_file(SidebarState& sidebar, EditorState& editor, const std::string& filename);
void switch_to_file_entry(SidebarState& sidebar, EditorState& editor, const SidebarFileEntry& entry);
void create_new_file(SidebarState& sidebar, EditorState& editor, const std::string& folder = "");
void delete_file(SidebarState& sidebar, EditorState& editor, int index);
void restore_file(SidebarState& sidebar, EditorState& editor, int deleted_index);

void start_rename(SidebarState& sidebar, EditorState& editor);
void commit_rename(SidebarState& sidebar, EditorState& editor);
void cancel_rename(SidebarState& sidebar, EditorState& editor);
