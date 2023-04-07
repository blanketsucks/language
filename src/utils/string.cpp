#include <quart/utils/string.h>

std::string utils::join(const std::string& sep, std::vector<std::string> strings) {
    std::string result;
    for (size_t i = 0; i < strings.size(); i++) {
        result += strings[i];
        if (i < strings.size() - 1) {
            result += sep;
        }
    }

    return result;
}

std::vector<std::string> utils::split(std::string str, const std::string& delim) {
    std::vector<std::string> result;
    size_t pos = 0;
    while ((pos = str.find(delim)) != std::string::npos) {
        result.push_back(str.substr(0, pos));
        str.erase(0, pos + delim.length());
    }

    result.push_back(str);
    return result;
}

std::string utils::replace(std::string str, const std::string& from, const std::string& to) {
    size_t start = 0;
    while ((start = str.find(from, start)) != std::string::npos) {
        str.replace(start, from.length(), to);
        start += to.length();
    }

    return str;
}

bool utils::startswith(const std::string& str, const std::string& prefix) {
    return str.rfind(prefix, 0) == 0;
}