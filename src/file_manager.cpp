#include "file_manager.h"
#include <sys/stat.h>
#include <dirent.h>
#include <fstream>
#include <ctime>
#include <cstdio>

void ensure_directory(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        #ifdef _WIN32
        mkdir(path.c_str());
        #else
        mkdir(path.c_str(), 0755);
        #endif
    }
}

void ensure_shortcuts_file(const std::string& user_files_dir) {
    std::string shortcuts_path = user_files_dir + "/Shortcuts.txt";
    
    std::ifstream test(shortcuts_path);
    if (test.good()) {
        test.close();
        return;
    }
    
    std::ofstream file(shortcuts_path);
    if (file) {
        file << "JustType Keyboard Shortcuts\n";
        file << "===========================\n\n";
        file << "Ctrl + F          Search in document\n";
        file << "Ctrl + T          Toggle dark/light theme\n";
        file << "Ctrl + P          Export as QR codes\n";
        file << "Ctrl + +          Increase font size\n";
        file << "Ctrl + -          Decrease font size\n";
        file << "Ctrl + Up         Jump to previous paragraph\n";
        file << "Ctrl + Down       Jump to next paragraph\n";
        file << "Alt + Up          Jump to top of document\n";
        file << "Alt + Down        Jump to bottom of document\n";
        file << "Ctrl + A          Select all\n";
        file << "Ctrl + Z          Undo\n";
        file << "Ctrl + Backspace  Delete word\n";
        file << "Meta (GUI key)    Toggle sidebar\n";
        file << "\n";
        file << "Search Mode:\n";
        file << "  Enter           Next match\n";
        file << "  Shift + Enter   Previous match\n";
        file << "  Escape          Close search\n";
        file.close();
    }
}

void cleanup_old_deleted_files(const std::string& recently_deleted_dir) {
    constexpr int MAX_AGE_DAYS = 30;
    
    DIR* dir = opendir(recently_deleted_dir.c_str());
    if (!dir) return;
    
    time_t now = time(nullptr);
    time_t max_age = MAX_AGE_DAYS * 24 * 60 * 60;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        
        std::string file_path = recently_deleted_dir + "/" + name;
        
        struct stat st;
        if (stat(file_path.c_str(), &st) == 0) {
            time_t age = now - st.st_mtime;
            if (age > max_age) {
                std::remove(file_path.c_str());
            }
        }
    }
    
    closedir(dir);
}
