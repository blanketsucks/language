#include <quart/filesystem.h>
#include <quart/assert.h>

#include <iostream>
#include <cerrno>
#include <glob.h>

namespace quart::fs {

Path::Path(String name) : m_name(move(name)) {}

Path Path::cwd() {
#if _WIN32 || _WIN64
    char buffer[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, buffer);

    return Path(buffer);
#else
    Array<char, FILENAME_MAX> buffer = {};
    ASSERT(getcwd(buffer.data(), FILENAME_MAX) && "getcwd() error");

    return { buffer.data() };
#endif
}

Path Path::home() {
#if _WIN32 || _WIN64
    char buffer[MAX_PATH];
    GetEnvironmentVariableA("USERPROFILE", buffer, MAX_PATH);

    return Path(buffer);
#else
    char* buffer = getenv("HOME");
    ASSERT(buffer && "getenv() error");

    return { buffer };
#endif
}

Path Path::resolve() const {
    char* result = realpath(m_name.c_str(), nullptr);
    if (!result) {
        return {};
    }
    
    Path path(result);
    free(result); 

    return path;
}

bool Path::operator==(const Path& other) const {
    return m_name == other.m_name;
}

bool Path::operator==(const String& other) const {
    return m_name == other;
}

Path Path::operator/(const String& other) const {
    return this->join(other);
}

Path Path::operator/(const Path& other) const {
    return this->join(other.m_name);
}

Path::operator String() const {
    return m_name;
}

Path::operator StringView() const {
    return m_name;
}

Path Path::from_parts(const Vector<String>& parts) {
    String name;
    for (size_t i = 0; i < parts.size(); i++) {
        name.append(parts[i]);
        if (i != parts.size() - 1) {
            name.push_back('/');
        }
    }

    return name;
}

Path Path::from_env(const String& env) {
    char* buffer = getenv(env.c_str());
    if (!buffer) {
        return {};
    }

    return { buffer };
}

struct stat Path::stat() const {
    struct stat buffer = {};
    ::stat(m_name.c_str(), &buffer);

    return buffer;
}

struct stat Path::stat(int& err) const {
    struct stat buffer = {};
    err = ::stat(m_name.c_str(), &buffer);

    return buffer;
}

bool Path::exists() const {
    struct stat buffer = {};
    return ::stat(m_name.c_str(), &buffer) == 0;
}

bool Path::is_regular_file() const {
    struct stat buffer = this->stat();
    return S_ISREG(buffer.st_mode);
}

bool Path::is_dir() const {
    struct stat buffer = this->stat();
    return S_ISDIR(buffer.st_mode);
}

bool Path::empty() const {
    return m_name.empty();
}

bool Path::is_part_of(const Path& other) const {
    return m_name.find(other.m_name) == 0;
}

Path Path::remove_prefix(const Path& prefix) const {
    if (!this->is_part_of(prefix)) {
        return m_name;
    }

    return m_name.substr(prefix.m_name.size());
}

String Path::filename() const {
    if (this->is_dir()) {
        return m_name;
    }

    auto pos = m_name.find_last_of("/\\");
    if (pos == String::npos) {
        return m_name;
    }

    return m_name.substr(pos + 1);
}

Path Path::parent() const {
    if (this->is_dir()) {
        return m_name;
    }

    auto pos = m_name.find_last_of("/\\");
    if (pos == String::npos) {
        return m_name;
    }

    return m_name.substr(0, pos);
}

Vector<String> Path::parts() const {
    Vector<String> parts;
    String part;

    for (auto& c : m_name) {
        if (c == '/' || c == '\\') {
            parts.push_back(part);
            part.clear();
        } else {
            part.push_back(c);
        }
    }

    if (!part.empty()) {
        parts.push_back(part);
    }

    return parts;
}

Vector<Path> Path::listdir() const {
    Vector<Path> paths;

#if _WIN32 || _WIN64
    String search = m_name + "/*.*";
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
    DIR* dir = opendir(m_name.c_str());
    if (!dir) {
        return paths;
    }

    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    struct dirent* entry = nullptr;
    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        paths.push_back(this->join(entry->d_name));
    }

    // NOLINTEND(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    closedir(dir);
#endif

    return paths;
}

Vector<Path> Path::listdir(bool recursive) const {
    Vector<Path> paths;

    for (auto& path : this->listdir()) {
        if (path.is_dir() && recursive) {
            auto subpaths = path.listdir(true);
            paths.insert(paths.end(), subpaths.begin(), subpaths.end());

            continue;
        }

        paths.push_back(path);
    }

    return paths;
}

Vector<Path> Path::glob(const String& pattern, int flags) {
    Vector<Path> paths;

#if _WIN32 || _WIN64
    WIN32_FIND_DATA fd;

    std::wstring wpattern = std::wstring(pattern.begin(), pattern.end());
    HANDLE handle = FindFirstFile(wpattern.c_str(), &fd);

    if (handle != INVALID_HANDLE_VALUE) {
        do {
            paths.push_back(Path(fd.cFileName));
        } while (FindNextFile(handle, &fd));

        FindClose(handle);
    }
#else
    glob_t result;
    ::glob(pattern.c_str(), flags, nullptr, &result);

    paths.reserve(result.gl_pathc);
    for (size_t i = 0; i < result.gl_pathc; i++) {
        paths.emplace_back(result.gl_pathv[i]);
    }

    globfree(&result);
#endif

    return paths;
}

std::fstream Path::open(OpenMode mode) {
    // FIXME: Remove this assert
    ASSERT(this->is_regular_file() && "Path is not a file");

    if (mode == OpenMode::Read) {
        return std::fstream(m_name, std::fstream::in);
    } else {
        return std::fstream(m_name, std::fstream::out);
    }
}

std::stringstream Path::read(bool binary) {
    // FIXME: Remove this assert
    ASSERT(this->is_regular_file() && "Path is not a file");

    std::fstream::openmode mode = std::fstream::in;
    if (binary) {
        mode |= std::fstream::binary;
    }

    std::fstream file(m_name, mode);
    std::stringstream buffer;

    buffer << file.rdbuf();
    file.close();

    return buffer;
}

Path Path::join(const String& path) const {
    String name;
    if (path[0] == '/' || m_name.back() == '/') {
        name = m_name + path;
    } else {
        name = m_name + "/" + path;
    }

    return name;
}

Path Path::join(const Path& path) const {
    return this->join(path.m_name);
}

String Path::extension() const {
    auto pos = m_name.find_last_of('.');
    if (pos == String::npos) {
        return "";
    }

    return m_name.substr(pos + 1);
}

Path Path::with_extension(const String& extension) const {
    if (extension.empty()) {
        return remove_extension(m_name);
    }

    return replace_extension(m_name, extension);
}

Path Path::with_extension() const {
    return remove_extension(m_name);
}

bool exists(const String& path) {
    struct stat buffer = {};
    return ::stat(path.c_str(), &buffer) == 0;
}

bool isdir(const String& path) {
    struct stat buffer = {};
    ::stat(path.c_str(), &buffer);

    return S_ISDIR(buffer.st_mode);
}

bool has_extension(const String& filename) {
    return filename.find_last_of('.') != String::npos;
}

String remove_extension(const String& filename) {
    if (has_extension(filename)) {
        return filename.substr(0, filename.find_last_of('.'));
    } else {
        return filename;
    }
}

String replace_extension(const String& filename, String extension) {
    if (extension[0] == '.') {
        extension = extension.substr(1);
    }

    return remove_extension(filename) + "." + extension;
}

}