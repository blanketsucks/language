#pragma once

#include <string>
#include <cstring>
#include <vector>
#include <sys/stat.h>
#include <cassert>
#include <fstream>
#include <sstream>
#include <format>

#include <quart/common.h>


#if _WIN32 || _WIN64
    #include <io.h>
    #include <Windows.h>

    #define popen _popen
    #define pclose _pclose
    #define isatty _isatty
    #define fileno _fileno
#else
    #include <unistd.h>
    #include <dirent.h>
#endif

#ifndef S_ISREG 
    #define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif

#ifndef S_ISDIR 
    #define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

namespace quart::fs {

enum class OpenMode : u8 {
    Read,
    Write
};

class Path {
public:
    Path() = default;
    Path(String name);

    bool operator==(const Path& other) const;
    bool operator==(const String& other) const;

    Path operator/(const String& other) const;
    Path operator/(const Path& other) const;

    operator String() const;
    operator StringView() const;

    static Path cwd();
    static Path home();

    static Path from_parts(const Vector<String>& parts);
    static Path from_env(const String& env);

    struct stat stat() const;
    struct stat stat(int& err) const;

    [[nodiscard]] bool exists() const;
    [[nodiscard]] bool is_regular_file() const;
    [[nodiscard]] bool is_dir() const;
    [[nodiscard]] bool empty() const;

    [[nodiscard]] bool is_part_of(const Path& other) const;

    fs::Path remove_prefix(const fs::Path& prefix) const;

    String filename() const;
    fs::Path parent() const;

    fs::Path resolve() const;

    Vector<String> parts() const;

    Vector<Path> listdir() const;
    Vector<Path> listdir(bool recursive) const;

    static Vector<Path> glob(const String& pattern, int flags = 0);

    std::fstream open(OpenMode mode = OpenMode::Read);
    std::stringstream read(bool binary = false);

    Path join(const String& path) const;
    Path join(const Path& path) const;

    String extension() const;

    Path with_extension(const String& extension) const;
    Path with_extension() const;

private:
    String m_name;
};

bool exists(const String& path);
bool isdir(const String& path);

bool has_extension(const String& filename);
String remove_extension(const String& filename);
String replace_extension(const String& filename, String extension);

}

template<>
struct std::formatter<quart::fs::Path> {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }

    template<typename FormatContext>
    auto format(const quart::fs::Path& path, FormatContext& ctx) const {
        return std::format_to(ctx.out(), "{}", std::string(path));
    }
};