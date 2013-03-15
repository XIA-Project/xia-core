#pragma once

#include <string>
#include <sstream>
#include <vector>

std::vector<std::string> split(const std::string &s, char delim);
std::string trim(const std::string& str);
int stoi(const std::string& str, int pos, int base);
