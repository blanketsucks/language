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
    #include <io.h>

    #define popen _popen
    #define pclose _pclose
    #define isatty _isatty
    #define fileno _fileno
#else
    #include <unistd.h>
#endif

#define EMPTY ""

#define ERROR(loc, ...) utils::error(loc, utils::fmt::format(__VA_ARGS__)); exit(1)
#define NOTE(loc, ...) utils::note(loc, utils::fmt::format(__VA_ARGS__))

#define TODO(x) std::cout << "TODO: " << "(" << __FILE__ << ":" << __LINE__ << ") " << x << '\n'; exit(1)
#define UNUSED(x) (void)x

namespace utils {

namespace fmt {
    bool has_ansi_support();

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

    enum class OpenMode {
        Read,
        Write
    };

    struct Path {
        std::string name;

        Path(const std::string& name);

        bool exists() const;
        bool isfile() const;
        bool isdir() const;

        std::string filename();

        std::vector<Path> listdir();

        std::fstream open(OpenMode mode = OpenMode::Read);

        Path join(const std::string& path);
        Path join(const Path& path);

        Path with_extension(const std::string& extension);
        Path with_extension();
    };

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

template<typename F, typename S> 
std::vector<std::pair<F, S>> zip(std::vector<F> first, std::vector<S> second) {
    assert(first.size() == second.size() && "first and second must have the same size");

    std::vector<std::pair<F, S>> result;
    result.reserve(first.size());

    for (size_t i = 0; i < first.size(); i++) {
        result.push_back(std::make_pair(first[i], second[i]));
    }

    return result;
}

template<typename T> using Ref = std::unique_ptr<T>;
template<typename T, typename ...Args> Ref<T> make_ref(Args&& ...args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

template<typename K, typename V> std::vector<V> values(std::map<K, V> map) {
    std::vector<V> result;

    result.reserve(map.size());
    for (auto& pair : map) {
        result.push_back(pair.second);
    }

    return result;
}

template<typename T> bool in(std::vector<T> values, T value) {
    return std::find(values.begin(), values.end(), value) != values.end();
}

template<typename L, typename R> L evaluate_integral_expression(
    TokenKind op, Location start, const char* type, L left, R right
) {
    switch (op) {
        case TokenKind::Add:
            return left + right;
        case TokenKind::Minus:
            return left - right;
        case TokenKind::Mul:
            return left * right;
        case TokenKind::Div:
            return left / right;
        case TokenKind::Mod:
            return (int)left % (int)right;
        case TokenKind::And:
            return left && right;
        case TokenKind::Or:
            return left || right;
        case TokenKind::BinaryAnd:
            return (int)left | (int)right;
        case TokenKind::BinaryOr:
            return (int)left & (int)right;
        case TokenKind::Xor:
            return (int)left ^ (int)right;
        case TokenKind::Lsh:
            return (int)left << (int)right;
        case TokenKind::Rsh:
            return (int)left >> (int)right;
        case TokenKind::Eq:
            return left == right;
        case TokenKind::Neq:
            return left != right;
        case TokenKind::Gt:
            return left > right;
        case TokenKind::Lt:
            return left < right;
        case TokenKind::Gte:
            return left >= right;
        case TokenKind::Lte:
            return left <= right;
        default:
            ERROR(
                start, 
                "Unsupported binary operator '{s}' for types '{}' and '{}'", 
                Token::getTokenTypeValue(op), type, type
            );
    }
}

};

#endif