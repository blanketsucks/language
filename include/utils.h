#ifndef _UTILS_H
#define _UTILS_H

#include "lexer/tokens.h"

#include "llvm/Support/FormatVariadic.h"
#include "llvm/ADT/Any.h"
#include "llvm/IR/Type.h"

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

#define FORMAT(fmt, ...) llvm::formatv(fmt, __VA_ARGS__).str()

#define RED     "\033[1;31m"
#define WHITE   "\033[1;37m"
#define MAGENTA "\033[1;35m"
#define RESET   "\033[0;0m"

#define COLOR(color, s) utils::color(color, s)

#define ERROR(loc, ...) utils::error(loc, llvm::formatv(__VA_ARGS__)); exit(1)
#define NOTE(loc, fmt, ...) utils::note(loc, FORMAT(fmt, __VA_ARGS__))

#define TODO(x) \
    std::cout << FORMAT("{0}:{1} in {2}: '{3}'\n", __FILE__, __LINE__, __PRETTY_FUNCTION__, x); \
    exit(1)


#define UNUSED(x) (void)x

namespace utils {

std::string color(const std::string& color, const std::string& s);

namespace filesystem {

    enum class OpenMode {
        Read,
        Write
    };

    struct Path {
        std::string name;

        Path(const std::string& name);

        static Path empty();
        static Path cwd();

        bool exists() const;
        bool isfile() const;
        bool isdir() const;
        bool isempty() const;

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

        template<typename... Ts> void error(const std::string& message, Ts&&... values) {
            std::cout << utils::color(WHITE, this->name + ": ") << utils::color(RED, "error: ");
            std::cout << llvm::formatv(message.c_str(), std::forward<Ts>(values)...).str() << '\n';

            if (this->exit_on_error) {
                exit(1);
            }
        }

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

        bool has(std::string name);
        bool has(Argument arg);

        template<typename T> T get(std::string name) {
            llvm::Any value = this->values[name];
            assert(value.hasValue() && "Value is not set.");

            return llvm::any_cast<T>(value);
        }

        template<typename T> T get(std::string name, T default_value) {
            if (!this->has(name)) {
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

bool has_color_support();

std::string join(std::string sep, std::vector<std::string> strings);
std::vector<std::string> split(std::string str, char delimiter);
std::string replace(std::string str, std::string from, std::string to);

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

template<typename K, typename V> std::vector<K> keys(std::map<K, V> map) {
    std::vector<K> result;
    result.reserve(map.size());
    for (auto& pair : map) {
        result.push_back(pair.first);
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