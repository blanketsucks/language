#include "utils.h"

#include <sys/stat.h>
#include <fstream>
#include <cstring>

#if _WIN32 || _WIN64
    #include <Windows.h>
#else
    #include <dirent.h>
#endif

#ifndef S_ISREG 
    #define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif

#ifndef S_ISDIR 
    #define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

using namespace utils;

filesystem::Path::Path(const std::string& name) {
    this->name = name;
}

bool filesystem::Path::exists() const {
    struct stat buffer;
    return (stat(this->name.c_str(), &buffer) == 0);
}

bool filesystem::Path::isfile() const {
    struct stat buffer;
    stat(this->name.c_str(), &buffer);

    return S_ISREG(buffer.st_mode);
}

bool filesystem::Path::isdir() const {
    struct stat buffer;
    stat(this->name.c_str(), &buffer);

    return S_ISDIR(buffer.st_mode);
}

std::string filesystem::Path::filename() {
    if (this->isdir()) {
        return this->name;
    }

    auto pos = this->name.find_last_of("/\\");
    if (pos == std::string::npos) {
        return this->name;
    }

    return this->name.substr(pos + 1);
}

std::vector<filesystem::Path> filesystem::Path::listdir() {
    std::vector<filesystem::Path> paths;

#if _WIN32 || _WIN64
    std::string search = this->name + "/*.*";
    WIN32_FIND_DATA fd;

    std::wstring wsearch = std::wstring(search.begin(), search.end());
    HANDLE handle = FindFirstFile(wsearch.c_str(), &fd);

    if (handle != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                std::wstring wfilename = std::wstring(fd.cFileName);

                std::string filename = std::string(wfilename.begin(), wfilename.end());
                paths.push_back(this->join(filename));
            }
        } while (FindNextFile(handle, &fd));

        FindClose(handle);
    }
#else
    DIR* dir = opendir(this->name.c_str());
    struct dirent* entry;

    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        paths.push_back(this->join(entry->d_name));
    }

    closedir(dir);
#endif

    return paths;
}

std::fstream filesystem::Path::open(filesystem::OpenMode mode) {
    assert(this->isfile() && "Path is not a file");

    if (mode == filesystem::OpenMode::Read) {
        return std::fstream(this->name, std::fstream::in);
    } else {
        return std::fstream(this->name, std::fstream::out);
    }
}

filesystem::Path filesystem::Path::join(const std::string& path) {
    return filesystem::Path(this->name + "/" + path);
}

filesystem::Path filesystem::Path::join(const filesystem::Path& path) {
    return filesystem::Path(this->name + "/" + path.name);
}

filesystem::Path filesystem::Path::with_extension(const std::string& extension) {
    return filesystem::Path(filesystem::replace_extension(this->name, extension));
}

filesystem::Path filesystem::Path::with_extension() {
    return filesystem::Path(filesystem::remove_extension(this->name));
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
    if (extension[0] == '.') {
        extension = extension.substr(1);
    }

    return filesystem::remove_extension(filename) + "." + extension;
}