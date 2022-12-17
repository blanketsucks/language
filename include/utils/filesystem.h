#ifndef _UTILS_FILESYSTEM_H
#define _UTILS_FILESYSTEM_H

#include <string>
#include <cstring>
#include <vector>
#include <sys/stat.h>
#include <assert.h>
#include <fstream>

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

namespace utils { namespace filesystem {

enum class OpenMode {
    Read,
    Write
};

struct Path {
    std::string name;

    Path(const std::string& name);

    bool operator==(const Path& other) const;
    bool operator==(const std::string& other) const;

    Path operator/(const std::string& other);
    Path operator/(const Path& other);

    static Path empty();
    static Path cwd();

    bool exists() const;
    bool isfile() const;
    bool isdir() const;
    bool isempty() const;

    std::string filename();
    std::string parent();

    std::vector<std::string> parts();

    std::vector<Path> listdir();
    std::vector<Path> listdir(bool recursive);

    std::fstream open(OpenMode mode = OpenMode::Read);

    Path join(const std::string& path);
    Path join(const Path& path);

    std::string extension();
    Path with_extension(const std::string& extension);
    Path with_extension();
};

bool has_extension(const std::string& filename);
std::string remove_extension(const std::string& filename);
std::string replace_extension(const std::string& filename, std::string extension);

} }

#endif