#ifndef _UTILS_H
#define _UTILS_H

#include <iostream>
#include <cstdio>
#include <array>
#include <cstdarg>

#if _WIN32 || _WIN64
    #define popen _popen
    #define pclose _pclose
#endif

#include "tokens.h"

#define ERROR(loc, ...) utils::error(loc, utils::fmt::format(__VA_ARGS__))
#define NOTE(loc, ...) utils::note(loc, utils::fmt::format(__VA_ARGS__))

#define TODO(x) std::cout << "TODO: " << "(" << __FILE__ << ":" << __LINE__ << ") " << x << '\n'; exit(1)
#define UNUSED(x) (void)x

namespace utils {
    namespace fmt {
        std::string join(std::string sep, std::vector<std::string>& strings);
        std::vector<std::string> split(const std::string& str, char delimiter);

        enum class AnsiFormat {
            Normal = 0,
            Bold = 1,
            Underline = 4,
        };

        static std::map<std::string, AnsiFormat> FORMATS {
            {"normal", AnsiFormat::Normal},
            {"bold", AnsiFormat::Bold},
            {"underline", AnsiFormat::Underline},
        };

        enum class AnsiForeground {
            Gray = 30,
            Red = 31,
            Green = 32,
            Yellow = 33,
            Blue = 34,
            Magenta = 35,
            Cyan = 36,
            White = 37,
        };

        static std::map<std::string, AnsiForeground> FOREGROUNDS {
            {"gray", AnsiForeground::Gray},
            {"red", AnsiForeground::Red},
            {"green", AnsiForeground::Green},
            {"yellow", AnsiForeground::Yellow},
            {"blue", AnsiForeground::Blue},
            {"magenta", AnsiForeground::Magenta},
            {"cyan", AnsiForeground::Cyan},
            {"white", AnsiForeground::White},
        };

        enum class AnsiBackground {
            None = -1,

            FireFlyDarkBlue = 40,
            Orange = 41,
            MarbleBlue = 42,
            GreyishTurquoise = 43,
            Gray = 44,
            Indigo = 45,
            LightGray = 46,
            White = 47,
        };

        static std::map<std::string, AnsiBackground> BACKGROUNDS {
            {"firefly-dark-blue", AnsiBackground::FireFlyDarkBlue},
            {"orange", AnsiBackground::Orange},
            {"marble-blue", AnsiBackground::MarbleBlue},
            {"greyish-turquoise", AnsiBackground::GreyishTurquoise},
            {"gray", AnsiBackground::Gray},
            {"indigo", AnsiBackground::Indigo},
            {"light-gray", AnsiBackground::LightGray},
            {"white", AnsiBackground::White},
        };

        AnsiFormat get_format(std::string str);
        AnsiForeground get_foreground(std::string str);
        AnsiBackground get_background(std::string str);

        const char* create_ansi_code();
        const char* create_ansi_code(AnsiFormat format, AnsiForeground foreground);
        const char* create_ansi_code(AnsiFormat format, AnsiForeground foreground, AnsiBackground background);

        std::string format(const std::string& str, va_list args);
        std::string format(const std::string& str, ...);
    }

    bool has_extension(const std::string& filename);
    std::string remove_extension(const std::string& filename);
    std::string replace_extension(const std::string& filename, std::string extension);

    void error(Location location, const std::string& message, bool fatal = true);
    void note(Location location, const std::string& message);

    std::string exec(const std::string& command);
}

#endif