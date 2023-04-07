#ifndef _UTILS_STRING_H
#define _UTILS_STRING_H

#include <string>
#include <vector>

namespace utils {

std::string join(const std::string& sep, std::vector<std::string> strings);
std::string replace(std::string str, const std::string& from, const std::string& to);

std::vector<std::string> split(std::string str, const std::string& delim);

bool startswith(const std::string& str, const std::string& prefix);

}

#endif