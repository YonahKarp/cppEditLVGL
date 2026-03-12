#include "sidebar_folder.h"
#include "file_manager.h"
#include <algorithm>
#include <dirent.h>
#include <sys/stat.h>
#include <fstream>
#include <cctype>
#include <cstdio>
#include <unistd.h>

static bool case_insensitive_less(const std::string& a, const std::string& b) {
    size_t min_len = a.size() < b.size() ? a.size() : b.size();
    for (size_t i = 0; i < min_len; ++i) {
        char ca = std::tolower(static_cast<unsigned char>(a[i]));
        char cb = std::tolower(static_cast<unsigned char>(b[i]));
        if (ca < cb) return true;
        if (ca > cb) return false;
    }
    return a.size() < b.size();
}

static bool entry_less(const SidebarFileEntry& a, const SidebarFileEntry& b) {
    if (a.folder != b.folder) {
        if (a.folder.empty()) return true;
        if (b.folder.empty()) return false;
        return case_insensitive_less(a.folder, b.folder);
    }
    return case_insensitive_less(a.filename, b.filename);
}

static bool case_insensitive_contains(const std::string& str, const std::string& substr) {
    if (substr.empty()) return true;
    std::string str_lower = str;
    std::string substr_lower = substr;
    for (auto& c : str_lower) c = std::tolower(static_cast<unsigned char>(c));
    for (auto& c : substr_lower) c = std::tolower(static_cast<unsigned char>(c));
    return str_lower.find(substr_lower) != std::string::npos;
}

std::string SidebarFolderData::get_relative_path(const SidebarFileEntry& entry) const {
    if (entry.folder.empty()) {
        return entry.filename;
    }
    return entry.folder + "/" + entry.filename;
}

void SidebarFolderData::build_display_items(bool searching) {
    display_items.clear();
    
    if (searching) {
        for (const auto& entry : filtered_entries) {
            SidebarItem item;
            item.type = SidebarItem::FILE;
            item.name = entry.filename;
            item.folder = entry.folder;
            item.snippet = entry.snippet;
            item.date_str = entry.date_str;
            display_items.push_back(item);
        }
        return;
    }
    
    // First add folders and their contents (folders on top)
    for (const auto& folder : folders) {
        SidebarItem folder_item;
        folder_item.type = SidebarItem::FOLDER;
        folder_item.name = folder;
        folder_item.folder = "";
        display_items.push_back(folder_item);
        
        // Default is open (expanded), so check if NOT in collapsed_folders
        bool is_expanded = collapsed_folders.find(folder) == collapsed_folders.end();
        if (is_expanded) {
            for (const auto& entry : filtered_entries) {
                if (entry.folder == folder) {
                    SidebarItem item;
                    item.type = SidebarItem::FILE;
                    item.name = entry.filename;
                    item.folder = folder;
                    item.snippet = entry.snippet;
                    item.date_str = entry.date_str;
                    display_items.push_back(item);
                }
            }
        }
    }
    
    // Then add root-level files (after folders)
    for (const auto& entry : filtered_entries) {
        if (entry.folder.empty()) {
            SidebarItem item;
            item.type = SidebarItem::FILE;
            item.name = entry.filename;
            item.folder = "";
            item.snippet = entry.snippet;
            item.date_str = entry.date_str;
            display_items.push_back(item);
        }
    }
}

void scan_folders_and_files(SidebarFolderData& data, const std::string& user_files_dir) {
    data.folders.clear();
    data.all_entries.clear();
    data.filtered_entries.clear();
    
    DIR* dir = opendir(user_files_dir.c_str());
    if (!dir) return;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == ".." || name == "recently_deleted" || name[0] == '.') {
            continue;
        }
        
        std::string full_path = user_files_dir + "/" + name;
        struct stat st;
        if (stat(full_path.c_str(), &st) != 0) continue;
        
        if (S_ISDIR(st.st_mode)) {
            data.folders.push_back(name);
            
            DIR* subdir = opendir(full_path.c_str());
            if (subdir) {
                struct dirent* subentry;
                while ((subentry = readdir(subdir)) != nullptr) {
                    std::string subname = subentry->d_name;
                    if (subname.size() > 4 && subname.substr(subname.size() - 4) == ".txt") {
                        SidebarFileEntry file_entry;
                        file_entry.filename = subname;
                        file_entry.folder = name;
                        
                        std::string file_path = full_path + "/" + subname;
                        struct stat file_stat;
                        if (stat(file_path.c_str(), &file_stat) == 0) {
                            file_entry.mod_time = file_stat.st_mtime;
                            struct tm* time_info = localtime(&file_entry.mod_time);
                            char date_buf[32];
                            strftime(date_buf, sizeof(date_buf), "%m/%d/%y", time_info);
                            file_entry.date_str = date_buf;
                        }
                        
                        std::ifstream file(file_path);
                        if (file) {
                            std::getline(file, file_entry.snippet);
                        }
                        
                        data.all_entries.push_back(file_entry);
                    }
                }
                closedir(subdir);
            }
        } else if (name.size() > 4 && name.substr(name.size() - 4) == ".txt") {
            SidebarFileEntry file_entry;
            file_entry.filename = name;
            file_entry.folder = "";
            
            if (stat(full_path.c_str(), &st) == 0) {
                file_entry.mod_time = st.st_mtime;
                struct tm* time_info = localtime(&file_entry.mod_time);
                char date_buf[32];
                strftime(date_buf, sizeof(date_buf), "%m/%d/%y", time_info);
                file_entry.date_str = date_buf;
            }
            
            std::ifstream file(full_path);
            if (file) {
                std::getline(file, file_entry.snippet);
            }
            
            data.all_entries.push_back(file_entry);
        }
    }
    closedir(dir);
    
    std::sort(data.folders.begin(), data.folders.end(), case_insensitive_less);
    std::sort(data.all_entries.begin(), data.all_entries.end(), entry_less);
}

std::string create_folder(const std::string& user_files_dir, const std::string& base_name) {
    int counter = 1;
    std::string folder_name;
    std::string folder_path;
    
    while (true) {
        if (counter == 1) {
            folder_name = base_name;
        } else {
            folder_name = base_name + " " + std::to_string(counter);
        }
        folder_path = user_files_dir + "/" + folder_name;
        
        struct stat st;
        if (stat(folder_path.c_str(), &st) != 0) {
            break;
        }
        counter++;
    }
    
    ensure_directory(folder_path);
    return folder_name;
}

bool rename_folder(const std::string& user_files_dir, const std::string& old_name, const std::string& new_name) {
    if (old_name == new_name) return true;
    if (new_name.empty()) return false;
    
    std::string old_path = user_files_dir + "/" + old_name;
    std::string new_path = user_files_dir + "/" + new_name;
    
    struct stat st;
    if (stat(new_path.c_str(), &st) == 0) {
        return false;
    }
    
    return std::rename(old_path.c_str(), new_path.c_str()) == 0;
}

static void remove_directory_recursive(const std::string& path) {
    DIR* dir = opendir(path.c_str());
    if (!dir) return;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        
        std::string full_path = path + "/" + name;
        struct stat st;
        if (stat(full_path.c_str(), &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                remove_directory_recursive(full_path);
            } else {
                std::remove(full_path.c_str());
            }
        }
    }
    closedir(dir);
    rmdir(path.c_str());
}

bool delete_folder(const std::string& user_files_dir, const std::string& folder_name,
                   const std::string& recently_deleted_dir) {
    std::string folder_path = user_files_dir + "/" + folder_name;
    
    struct stat st;
    if (stat(folder_path.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
        return false;
    }
    
    DIR* dir = opendir(folder_path.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string name = entry->d_name;
            if (name == "." || name == "..") continue;
            
            std::string file_path = folder_path + "/" + name;
            if (name.size() > 4 && name.substr(name.size() - 4) == ".txt") {
                std::string deleted_name = folder_name + "__" + name;
                std::string deleted_path = recently_deleted_dir + "/" + deleted_name;
                std::rename(file_path.c_str(), deleted_path.c_str());
            } else {
                std::remove(file_path.c_str());
            }
        }
        closedir(dir);
    }
    
    rmdir(folder_path.c_str());
    return true;
}

bool move_file(const std::string& user_files_dir, const SidebarFileEntry& entry,
               const std::string& new_name, const std::string& new_folder) {
    std::string old_path;
    if (entry.folder.empty()) {
        old_path = user_files_dir + "/" + entry.filename;
    } else {
        old_path = user_files_dir + "/" + entry.folder + "/" + entry.filename;
    }
    
    std::string new_filename = new_name;
    if (new_filename.size() < 4 || new_filename.substr(new_filename.size() - 4) != ".txt") {
        new_filename += ".txt";
    }
    
    std::string new_path;
    if (new_folder.empty()) {
        new_path = user_files_dir + "/" + new_filename;
    } else {
        new_path = user_files_dir + "/" + new_folder + "/" + new_filename;
    }
    
    if (old_path == new_path) {
        return true;
    }
    
    std::ifstream test(new_path);
    if (test.good()) {
        test.close();
        return false;
    }
    
    return std::rename(old_path.c_str(), new_path.c_str()) == 0;
}

std::string get_current_folder(const std::string& current_file_path, const std::string& user_files_dir) {
    if (current_file_path.find(user_files_dir) != 0) {
        return "";
    }
    
    std::string relative = current_file_path.substr(user_files_dir.size());
    if (!relative.empty() && relative[0] == '/') {
        relative = relative.substr(1);
    }
    
    size_t slash_pos = relative.find('/');
    if (slash_pos == std::string::npos) {
        return "";
    }
    
    return relative.substr(0, slash_pos);
}

void filter_folder_entries(SidebarFolderData& data, const std::string& query) {
    data.filtered_entries.clear();
    
    for (const auto& entry : data.all_entries) {
        if (case_insensitive_contains(entry.filename, query)) {
            data.filtered_entries.push_back(entry);
        }
    }
}
