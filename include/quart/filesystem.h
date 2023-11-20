#pragma once

#include <string>
#include <cstring>
#include <vector>
#include <sys/stat.h>
#include <cassert>
#include <fstream>
#include <sstream>

#include <llvm/ADT/StringRef.h>

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

namespace fs {

enum class OpenMode {
    Read,
    Write
};

struct Path {
    std::string name;

    Path(std::string name = "");

    bool operator==(const Path& other) const;
    bool operator==(const std::string& other) const;

    Path operator/(const std::string& other) const;
    Path operator/(const Path& other) const;

    operator std::string() const;
    operator llvm::StringRef() const;

    static Path cwd();
    static Path home();

    static Path from_parts(const std::vector<std::string>& parts);
    static Path from_env(const std::string& env);

    struct stat stat();
    struct stat stat(int& err);

    [[nodiscard]] bool exists() const;
    [[nodiscard]] bool isfile() const;
    [[nodiscard]] bool isdir() const;
    [[nodiscard]] bool isempty() const;

    [[nodiscard]] bool is_part_of(const Path& other) const;

    fs::Path remove_prefix(const fs::Path& prefix) const;

    std::string filename() const;
    fs::Path parent() const;

    fs::Path resolve() const;

    std::vector<std::string> parts() const;

    std::vector<Path> listdir() const;
    std::vector<Path> listdir(bool recursive) const;

    static std::vector<Path> glob(const std::string& pattern, int flags = 0);

    std::fstream open(OpenMode mode = OpenMode::Read);
    std::stringstream read(bool binary = false);

    Path join(const std::string& path) const;
    Path join(const Path& path) const;

    std::string extension();

    Path with_extension(const std::string& extension);
    Path with_extension();
};

bool exists(const std::string& path);
bool isdir(const std::string& path);

bool has_extension(const std::string& filename);
std::string remove_extension(const std::string& filename);
std::string replace_extension(const std::string& filename, std::string extension);

} 