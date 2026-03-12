#include "sidebar_file_utils.h"
#include "sidebar.h"
#include "editor.h"
#include "file_manager.h"
#include <fstream>
#include <cstring>

void switch_to_file(SidebarState& sidebar, EditorState& editor, const std::string& filename) {
    save_editor_content(editor);
    
    std::string new_path = editor.user_files_dir + "/" + filename;
    load_file_into_editor(editor, new_path.c_str());
    
    editor.state_pending_save = true;
    editor.state_change_time = lv_tick_get();
}

void switch_to_file_entry(SidebarState& sidebar, EditorState& editor, const SidebarFileEntry& entry) {
    save_editor_content(editor);
    
    std::string relative_path = sidebar.folder_data.get_relative_path(entry);
    std::string new_path = editor.user_files_dir + "/" + relative_path;
    load_file_into_editor(editor, new_path.c_str());
    
    editor.state_pending_save = true;
    editor.state_change_time = lv_tick_get();
}

void create_new_file(SidebarState& sidebar, EditorState& editor, const std::string& folder) {
    save_editor_content(editor);
    
    int counter = 1;
    std::string new_filename;
    std::string new_path;
    
    while (true) {
        if (counter == 1) {
            new_filename = "Untitled.txt";
        } else {
            new_filename = "Untitled " + std::to_string(counter) + ".txt";
        }
        
        if (folder.empty()) {
            new_path = editor.user_files_dir + "/" + new_filename;
        } else {
            new_path = editor.user_files_dir + "/" + folder + "/" + new_filename;
        }
        
        std::ifstream test(new_path);
        if (!test.good()) {
            break;
        }
        counter++;
    }
    
    std::ofstream new_file(new_path);
    new_file.close();
    
    scan_txt_files(sidebar, editor.user_files_dir);
    filter_sidebar_files(sidebar);
    refresh_file_list_ui(sidebar, editor);
    
    load_file_into_editor(editor, new_path.c_str());
    hide_sidebar(sidebar, editor);
}

void delete_file(SidebarState& sidebar, EditorState& editor, int index) {
    // Use the stored item identity from show_delete_dialog instead of looking up by index
    // This ensures we delete the correct item even if the display list has changed
    const std::string& item_name = sidebar.delete_item_name;
    const std::string& item_folder = sidebar.delete_item_folder;
    bool is_folder = sidebar.delete_item_is_folder;
    
    if (item_name.empty()) return;
    
    if (is_folder) {
        delete_folder(editor.user_files_dir, item_name, editor.recently_deleted_dir);
        sidebar.folder_data.collapsed_folders.erase(item_name);
    } else {
        std::string file_path;
        if (item_folder.empty()) {
            file_path = editor.user_files_dir + "/" + item_name;
        } else {
            file_path = editor.user_files_dir + "/" + item_folder + "/" + item_name;
        }
        
        std::string deleted_name = item_folder.empty() ? item_name : (item_folder + "__" + item_name);
        std::string deleted_path = editor.recently_deleted_dir + "/" + deleted_name;
        
        std::rename(file_path.c_str(), deleted_path.c_str());
        
        if (file_path == editor.current_file_path) {
            // Clear current file path to prevent save_editor_content from recreating the deleted file
            editor.current_file_path.clear();
            editor.temp_file_path.clear();
            
            scan_folders_and_files(sidebar.folder_data, editor.user_files_dir);
            filter_folder_entries(sidebar.folder_data, std::string(sidebar.search_buffer, sidebar.search_len));
            sidebar.folder_data.build_display_items(sidebar.searching);
            
            bool found_file = false;
            for (const auto& display_item : sidebar.folder_data.display_items) {
                if (display_item.is_file()) {
                    SidebarFileEntry entry;
                    entry.filename = display_item.name;
                    entry.folder = display_item.folder;
                    switch_to_file_entry(sidebar, editor, entry);
                    found_file = true;
                    break;
                }
            }
            if (!found_file) {
                create_new_file(sidebar, editor);
                return;
            }
        }
    }
    
    scan_folders_and_files(sidebar.folder_data, editor.user_files_dir);
    scan_deleted_files(sidebar, editor.recently_deleted_dir);
    filter_folder_entries(sidebar.folder_data, std::string(sidebar.search_buffer, sidebar.search_len));
    refresh_file_list_ui(sidebar, editor);
}

void restore_file(SidebarState& sidebar, EditorState& editor, int deleted_index) {
    if (deleted_index < 0 || deleted_index >= (int)sidebar.filtered_deleted_list.size()) return;
    
    const std::string& filename = sidebar.filtered_deleted_list[deleted_index];
    std::string src_path = editor.recently_deleted_dir + "/" + filename;
    
    size_t folder_sep = filename.find("__");
    std::string dest_folder = "";
    std::string actual_filename = filename;
    
    if (folder_sep != std::string::npos) {
        dest_folder = filename.substr(0, folder_sep);
        actual_filename = filename.substr(folder_sep + 2);
        
        std::string folder_path = editor.user_files_dir + "/" + dest_folder;
        ensure_directory(folder_path);
    }
    
    std::string dest_path;
    if (dest_folder.empty()) {
        dest_path = editor.user_files_dir + "/" + actual_filename;
    } else {
        dest_path = editor.user_files_dir + "/" + dest_folder + "/" + actual_filename;
    }
    
    std::ifstream test(dest_path);
    if (test.good()) {
        test.close();
        std::string base = actual_filename.substr(0, actual_filename.size() - 4);
        int counter = 1;
        while (true) {
            if (dest_folder.empty()) {
                dest_path = editor.user_files_dir + "/" + base + " (" + std::to_string(counter) + ").txt";
            } else {
                dest_path = editor.user_files_dir + "/" + dest_folder + "/" + base + " (" + std::to_string(counter) + ").txt";
            }
            std::ifstream check(dest_path);
            if (!check.good()) {
                break;
            }
            check.close();
            counter++;
        }
    }
    
    std::rename(src_path.c_str(), dest_path.c_str());
    
    scan_folders_and_files(sidebar.folder_data, editor.user_files_dir);
    scan_deleted_files(sidebar, editor.recently_deleted_dir);
    filter_folder_entries(sidebar.folder_data, std::string(sidebar.search_buffer, sidebar.search_len));
    refresh_file_list_ui(sidebar, editor);
    
    SidebarFileEntry entry;
    entry.filename = actual_filename;
    entry.folder = dest_folder;
    switch_to_file_entry(sidebar, editor, entry);
}

void start_rename(SidebarState& sidebar, EditorState& editor) {
    if (sidebar.searching) return;
    if (sidebar.new_file_selected || sidebar.new_folder_selected) return;
    
    sidebar.folder_data.build_display_items(sidebar.searching);
    if (sidebar.selected_index < 0 || sidebar.selected_index >= (int)sidebar.folder_data.display_items.size()) return;
    
    sidebar.renaming = true;
    sidebar.rename_file_index = sidebar.selected_index;
    sidebar.rename_textarea = nullptr;
    
    const SidebarItem& item = sidebar.folder_data.display_items[sidebar.selected_index];
    std::string name = item.name;
    if (item.is_file() && name.size() > 4 && name.substr(name.size() - 4) == ".txt") {
        name = name.substr(0, name.size() - 4);
    }
    strncpy(sidebar.rename_buffer, name.c_str(), sizeof(sidebar.rename_buffer) - 1);
    sidebar.rename_buffer[sizeof(sidebar.rename_buffer) - 1] = '\0';
    sidebar.rename_len = strlen(sidebar.rename_buffer);
    
    refresh_file_list_ui(sidebar, editor);
}

void commit_rename(SidebarState& sidebar, EditorState& editor) {
    if (!sidebar.renaming || !sidebar.rename_textarea) return;
    
    const char* new_name = lv_textarea_get_text(sidebar.rename_textarea);
    if (!new_name || strlen(new_name) == 0) {
        cancel_rename(sidebar, editor);
        return;
    }
    
    sidebar.folder_data.build_display_items(sidebar.searching);
    if (sidebar.rename_file_index < 0 || 
        sidebar.rename_file_index >= (int)sidebar.folder_data.display_items.size()) {
        cancel_rename(sidebar, editor);
        return;
    }
    
    const SidebarItem& item = sidebar.folder_data.display_items[sidebar.rename_file_index];
    
    if (item.is_folder()) {
        std::string old_name = item.name;
        std::string new_folder_name = new_name;
        
        if (old_name != new_folder_name) {
            if (rename_folder(editor.user_files_dir, old_name, new_folder_name)) {
                if (sidebar.folder_data.collapsed_folders.find(old_name) != 
                    sidebar.folder_data.collapsed_folders.end()) {
                    sidebar.folder_data.collapsed_folders.erase(old_name);
                    sidebar.folder_data.collapsed_folders.insert(new_folder_name);
                    editor.state_pending_save = true;
                    editor.state_change_time = lv_tick_get();
                }
            }
        }
    } else {
        std::string old_path;
        if (item.folder.empty()) {
            old_path = editor.user_files_dir + "/" + item.name;
        } else {
            old_path = editor.user_files_dir + "/" + item.folder + "/" + item.name;
        }
        
        std::string new_filename = new_name;
        if (new_filename.size() < 4 || new_filename.substr(new_filename.size() - 4) != ".txt") {
            new_filename += ".txt";
        }
        
        std::string new_path;
        if (item.folder.empty()) {
            new_path = editor.user_files_dir + "/" + new_filename;
        } else {
            new_path = editor.user_files_dir + "/" + item.folder + "/" + new_filename;
        }
        
        if (old_path != new_path) {
            std::ifstream test(new_path);
            if (!test.good()) {
                std::rename(old_path.c_str(), new_path.c_str());
                
                bool was_current_file = (editor.current_file_path == old_path);
                
                if (was_current_file) {
                    editor.current_file_path = new_path;
                    editor.temp_file_path = new_path + ".tmp";
                    update_filename_display(editor);
                }
            }
        }
    }
    
    sidebar.renaming = false;
    sidebar.rename_pending = false;
    sidebar.rename_textarea = nullptr;
    
    scan_folders_and_files(sidebar.folder_data, editor.user_files_dir);
    filter_folder_entries(sidebar.folder_data, std::string(sidebar.search_buffer, sidebar.search_len));
    refresh_file_list_ui(sidebar, editor);
}

void cancel_rename(SidebarState& sidebar, EditorState& editor) {
    sidebar.renaming = false;
    sidebar.rename_pending = false;
    sidebar.rename_textarea = nullptr;
    refresh_file_list_ui(sidebar, editor);
}
