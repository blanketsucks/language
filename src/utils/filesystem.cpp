#include <quart/utils/filesystem.h>

#include <cerrno>

using namespace utils;

fs::Path::Path(const std::string& name) {
    this->name = name;
}

fs::Path::Path() {
    this->name = "";
}

fs::Path fs::Path::empty() {
    return Path("");
}

std::string fs::Path::str() {
    return this->name;
}

fs::Path fs::Path::cwd() {
#if _WIN32 || _WIN64
    char buffer[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, buffer);

    return Path(buffer);
#else
    char buffer[FILENAME_MAX];
    assert(getcwd(buffer, FILENAME_MAX) && "getcwd() error");

    return Path(buffer);
#endif
}

fs::Path fs::Path::home() {
#if _WIN32 || _WIN64
    char buffer[MAX_PATH];
    GetEnvironmentVariableA("USERPROFILE", buffer, MAX_PATH);

    return Path(buffer);
#else
    char* buffer = getenv("HOME");
    assert(buffer && "getenv() error");

    return Path(buffer);
#endif
}

bool fs::Path::operator==(const Path& other) const {
    return this->name == other.name;
}

bool fs::Path::operator==(const std::string& other) const {
    return this->name == other;
}

fs::Path fs::Path::operator/(const std::string& other) {
    return this->join(other);
}

fs::Path fs::Path::operator/(const Path& other) {
    return this->join(other.name);
}

fs::Path::operator std::string() const {
    return this->name;
}

struct stat fs::Path::stat() {
    struct stat buffer;
    ::stat(this->name.c_str(), &buffer);

    return buffer;
}

struct stat fs::Path::stat(int& err) {
    struct stat buffer;
    err = ::stat(this->name.c_str(), &buffer);

    return buffer;
}

bool fs::Path::exists() const {
    struct stat buffer;
    return ::stat(this->name.c_str(), &buffer) == 0;
}

bool fs::Path::isfile() const {
    struct stat buffer;
    ::stat(this->name.c_str(), &buffer);

    return S_ISREG(buffer.st_mode);
}

bool fs::Path::isdir() const {
    struct stat buffer;
    ::stat(this->name.c_str(), &buffer);

    return S_ISDIR(buffer.st_mode);
}

bool fs::Path::isempty() const {
    return this->name.empty();
}

std::string fs::Path::filename() {
    if (this->isdir()) {
        return this->name;
    }

    auto pos = this->name.find_last_of("/\\");
    if (pos == std::string::npos) {
        return this->name;
    }

    return this->name.substr(pos + 1);
}

fs::Path fs::Path::parent() {
    if (this->isdir()) {
        return this->name;
    }

    auto pos = this->name.find_last_of("/\\");
    if (pos == std::string::npos) {
        return this->name;
    }

    return this->name.substr(0, pos);
}

std::vector<std::string> fs::Path::parts() {
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

std::vector<fs::Path> fs::Path::listdir() {
    std::vector<fs::Path> paths;

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
    if (!dir) {
        return paths;
    }

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

std::vector<fs::Path> fs::Path::listdir(bool recursive) {
    std::vector<fs::Path> paths;

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

std::fstream fs::Path::open(fs::OpenMode mode) {
    assert(this->isfile() && "Path is not a file");

    if (mode == fs::OpenMode::Read) {
        return std::fstream(this->name, std::fstream::in);
    } else {
        return std::fstream(this->name, std::fstream::out);
    }
}

std::stringstream fs::Path::read(bool binary) {
    assert(this->isfile() && "Path is not a file");

    std::fstream::openmode mode = std::fstream::in;
    if (binary) {
        mode |= std::fstream::binary;
    }

    std::fstream file(this->name, mode);
    std::stringstream buffer;

    buffer << file.rdbuf();
    file.close();

    return buffer;
}

fs::Path fs::Path::join(const std::string& path) {
    std::string name;
    if (path[0] == '/' || this->name.back() == '/') {
        name = this->name + path;
    } else {
        name = this->name + "/" + path;
    }

    return fs::Path(name);
}

fs::Path fs::Path::join(const fs::Path& path) {
    return this->join(path.name);
}

std::string fs::Path::extension() {
    auto pos = this->name.find_last_of(".");
    if (pos == std::string::npos) {
        return "";
    }

    return this->name.substr(pos + 1);
}

fs::Path fs::Path::with_extension(const std::string& extension) {
    return fs::Path(fs::replace_extension(this->name, extension));
}

fs::Path fs::Path::with_extension() {
    return fs::Path(fs::remove_extension(this->name));
}

bool fs::has_extension(const std::string& filename) {
    return filename.find_last_of('.') != std::string::npos;
}

std::string fs::remove_extension(const std::string& filename) {
    if (fs::has_extension(filename)) {
        return filename.substr(0, filename.find_last_of('.'));
    } else {
        return filename;
    }
}

std::string fs::replace_extension(const std::string& filename, std::string extension) {
    if (extension[0] == '.') {
        extension = extension.substr(1);
    }

    return fs::remove_extension(filename) + "." + extension;
}