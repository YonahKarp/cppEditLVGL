#pragma once

#include <string>

void ensure_directory(const std::string& path);
void ensure_shortcuts_file(const std::string& user_files_dir);
void cleanup_old_deleted_files(const std::string& recently_deleted_dir);
