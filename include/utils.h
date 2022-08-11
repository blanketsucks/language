#ifndef _UTILS_H
#define _UTILS_H

#include "lexer/tokens.h"
#include "llvm/ADT/Any.h"

#include <iostream>
#include <cstdio>
#include <array>
#include <cstdarg>
#include <functional>

#if _WIN32 || _WIN64
    #define popen _popen
    #define pclose _pclose
#endif

#define EMPTY ""

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

namespace filesystem {
    bool exists(const std::string& path);

    bool has_extension(const std::string& filename);
    std::string remove_extension(const std::string& filename);
    std::string replace_extension(const std::string& filename, std::string extension);
}

namespace argparse {

    using ArgumentValueMap = std::map<std::string, llvm::Any>;
    using CallbackFunction = std::function<void(llvm::Any)>;

    enum ArgumentValueType {
        Required,
        NoArguments,
        Optional,
        Many
    };

    struct Argument {
        std::string name;
        std::string short_name;
        std::string description;
        std::string dest;
        ArgumentValueType type;
        bool required;
        CallbackFunction callback;

        Argument();
        Argument(
            std::string name, 
            ArgumentValueType type, 
            std::string short_name, 
            std::string description, 
            std::string dest,
            bool required,
            CallbackFunction callback
        );

        std::string get_clean_name();
    };

    class ArgumentParser {
    public:
        std::string name;
        std::string description;
        std::string usage;
        std::string epilogue;

        ArgumentParser(
            std::string name, 
            std::string description = EMPTY, 
            std::string usage = EMPTY,
            std::string epilogue = EMPTY, 
            bool exit_on_error = true, 
            bool add_help = true
        );

        void display_help();

        void error(const std::string& message, ...);

        Argument add_argument(
            std::string name, 
            ArgumentValueType type = ArgumentValueType::NoArguments, 
            std::string short_name = EMPTY, 
            std::string description = EMPTY, 
            std::string dest = "arg",
            bool required = false,
            CallbackFunction callback = nullptr
        );

        Argument add_argument(Argument arg);

        std::vector<std::string> parse(int argc, char** argv);

        bool has_value(std::string name);
        bool has_value(Argument arg);

        template<typename T> T get(std::string name) {
            llvm::Any value = this->values[name];
            assert(value.hasValue() && "Value is not set.");

            return llvm::any_cast<T>(value);
        }

        template<typename T> T get(std::string name, T default_value) {
            if (!this->has_value(name)) {
                return default_value;
            }

            return llvm::any_cast<T>(this->values[name]);
        }

    private:
        void set_value(Argument arg, llvm::Any value);

        bool exit_on_error;
    
        std::map<std::string, Argument> arguments;
        std::vector<Argument> positionals;
        ArgumentValueMap values;
    };
}

void message(std::string type, fmt::AnsiForeground color, Location location, const std::string& message);

void error(Location location, const std::string& message, bool fatal = true);
void note(Location location, const std::string& message);

std::string exec(const std::string& command);

}

#endif