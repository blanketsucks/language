#include "utils.h"

#include <sys/stat.h>

using namespace utils;

bool filesystem::exists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

bool filesystem::has_extension(const std::string& filename) {
    return filename.find_last_of('.') != std::string::npos;
}

std::string filesystem::remove_extension(const std::string& filename) {
    if (filesystem::has_extension(filename)) {
        return filename.substr(0, filename.find_last_of('.'));
    } else {
        return filename;
    }
}

std::string filesystem::replace_extension(const std::string& filename, std::string extension) {
    return filesystem::remove_extension(filename) + "." + extension;
}