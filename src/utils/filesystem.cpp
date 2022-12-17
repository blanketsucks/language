#include "utils/filesystem.h"

using namespace utils;

filesystem::Path::Path(const std::string& name) {
    this->name = name;
}

filesystem::Path filesystem::Path::empty() {
    return Path("");
}

filesystem::Path filesystem::Path::cwd() {
#if _WIN32 || _WIN64
    char buffer[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, buffer);

    return Path(buffer);
#else
    char buffer[FILENAME_MAX];
    getcwd(buffer, FILENAME_MAX);

    return Path(buffer);
#endif
}

bool filesystem::Path::operator==(const Path& other) const {
    return this->name == other.name;
}

bool filesystem::Path::operator==(const std::string& other) const {
    return this->name == other;
}

filesystem::Path filesystem::Path::operator/(const std::string& other) {
    return this->join(other);
}

filesystem::Path filesystem::Path::operator/(const Path& other) {
    return this->join(other.name);
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

bool filesystem::Path::isempty() const {
    return this->name.empty();
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

std::string filesystem::Path::parent() {
    if (this->isdir()) {
        return this->name;
    }

    auto pos = this->name.find_last_of("/\\");
    if (pos == std::string::npos) {
        return this->name;
    }

    return this->name.substr(0, pos);
}

std::vector<std::string> filesystem::Path::parts() {
    std::vector<std::string> parts;
    std::string part;

    for (auto& c : this->name) {
        if (c == '/' || c == '\\') {
            parts.push_back(part);
            part.clear();
        } else {
            part += c;
        }
    }

    if (!part.empty()) {
        parts.push_back(part);
    }

    return parts;
}

std::vector<filesystem::Path> filesystem::Path::listdir() {
    std::vector<filesystem::Path> paths;

#if _WIN32 || _WIN64
    std::string search = this->name + "/*.*";
    WIN32_FIND_DATA fd;

    std::wstring wsearch = std::wstring(search.begin(), search.end());
    HANDLE handle = FindFirstFile(search.c_str(), &fd);

    if (handle != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                paths.push_back(this->join(fd.cFileName));
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

std::vector<filesystem::Path> filesystem::Path::listdir(bool recursive) {
    std::vector<filesystem::Path> paths;

    for (auto& path : this->listdir()) {
        if (path.isdir()) {
            if (recursive) {
                auto subpaths = path.listdir(true);
                paths.insert(paths.end(), subpaths.begin(), subpaths.end());
            } else {
                paths.push_back(path);
            }
        } else {
            paths.push_back(path);
        }
    }

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
    std::string name;
    if (path[0] == '/' || this->name.back() == '/') {
        name = this->name + path;
    } else {
        name = this->name + "/" + path;
    }

    return filesystem::Path(name);
}

filesystem::Path filesystem::Path::join(const filesystem::Path& path) {
    return this->join(path.name);
}

std::string filesystem::Path::extension() {
    auto pos = this->name.find_last_of(".");
    if (pos == std::string::npos) {
        return "";
    }

    return this->name.substr(pos + 1);
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