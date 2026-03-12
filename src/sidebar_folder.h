#pragma once

#include <string>
#include <vector>
#include <set>
#include <ctime>

struct SidebarFileEntry {
    std::string filename;
    std::string folder;
    std::string snippet;
    std::string date_str;
    time_t mod_time = 0;
};

struct SidebarItem {
    enum Type { FOLDER, FILE };
    Type type;
    std::string name;
    std::string folder;  // For files: parent folder. For folders: empty
    std::string snippet;
    std::string date_str;
    
    bool is_folder() const { return type == FOLDER; }
    bool is_file() const { return type == FILE; }
};

struct SidebarFolderData {
    std::vector<std::string> folders;
    std::vector<SidebarFileEntry> all_entries;
    std::vector<SidebarFileEntry> filtered_entries;
    
    // Display items (folders + files combined for selection)
    std::vector<SidebarItem> display_items;
    
    // Track which folders are collapsed (default is open)
    std::set<std::string> collapsed_folders;
    
    std::string get_relative_path(const SidebarFileEntry& entry) const;
    void build_display_items(bool searching);
};

void scan_folders_and_files(SidebarFolderData& data, const std::string& user_files_dir);
std::string create_folder(const std::string& user_files_dir, const std::string& base_name = "New Folder");
bool move_file(const std::string& user_files_dir, const SidebarFileEntry& entry, 
               const std::string& new_name, const std::string& new_folder);
bool rename_folder(const std::string& user_files_dir, const std::string& old_name, const std::string& new_name);
bool delete_folder(const std::string& user_files_dir, const std::string& folder_name, 
                   const std::string& recently_deleted_dir);
std::string get_current_folder(const std::string& current_file_path, const std::string& user_files_dir);
void filter_folder_entries(SidebarFolderData& data, const std::string& query);
