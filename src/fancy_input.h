#ifndef FANCY_INPUT_H
#define FANCY_INPUT_H

#include <string>
#include <vector>

void set_autocomplete_command_list(std::vector<std::string>cmd_list);
bool get_command_line(std::string&line);

#endif

