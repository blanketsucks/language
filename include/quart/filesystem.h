#pragma once

#include <string>
#include <cstring>
#include <vector>
#include <sys/stat.h>
#include <assert.h>
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

    Path(const std::string& name);
    Path();

    bool operator==(const Path& other) const;
    bool operator==(const std::string& other) const;

    Path operator/(const std::string& other);
    Path operator/(const Path& other);

    operator std::string() const;
    operator llvm::StringRef() const;

    static Path empty();
    static Path cwd();
    static Path home();

    static Path from_parts(const std::vector<std::string>& parts);
    static Path from_env(const std::string& env);

    struct stat stat();
    struct stat stat(int& err);

    bool exists() const;
    bool isfile() const;
    bool isdir() const;
    bool isempty() const;

    bool is_part_of(const Path& other) const;

    fs::Path remove_prefix(const fs::Path& prefix);

    std::string filename();
    fs::Path parent();

    fs::Path resolve();

    std::vector<std::string> parts();

    std::vector<Path> listdir();
    std::vector<Path> listdir(bool recursive);

    static std::vector<Path> glob(const std::string& pattern, int flags = 0);

    std::fstream open(OpenMode mode = OpenMode::Read);
    std::stringstream read(bool binary = false);

    Path join(const std::string& path);
    Path join(const Path& path);

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