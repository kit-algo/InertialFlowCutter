#ifndef FILE_UTILITY_H
#define FILE_UTILITY_H

#include <string>
#include <ctime>

std::string concat_file_path_and_file_name(std::string path, const std::string&name);
std::string make_absolute_file_name(const std::string&file_name);
std::string get_temp_directory_path();
std::string uniquely_hash_file_name(const std::string&file_name);
bool file_exists(const std::string&file_name);
std::time_t file_last_modified(const std::string&file_name);

#endif
