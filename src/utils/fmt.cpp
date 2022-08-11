#include "utils.h"

#include "types/type.h"

#include <assert.h>

std::string utils::fmt::join(std::string sep, std::vector<std::string>& strings) {
    std::string result;
    for (auto& str : strings) {
        result += str + sep;
    }

    if (!result.empty()) {
        result.pop_back();
    }

    return result;
}

std::vector<std::string> utils::fmt::split(const std::string& str, char delimiter) {
    std::string result;
    std::vector<std::string> tokens;

    for (char c : str) {
        if (c == delimiter) {
            tokens.push_back(result);
            result.clear();
        } else {
            result += c;
        }
    }

    if (result.size() > 0) {
        tokens.push_back(result);
    }

    return tokens;
}

utils::fmt::AnsiFormat utils::fmt::get_format(std::string str) {
    assert(FORMATS.find(str) != FORMATS.end() && "Invalid format");
    return FORMATS[str];
}

utils::fmt::AnsiForeground utils::fmt::get_foreground(std::string str) {
    assert(FOREGROUNDS.find(str) != FOREGROUNDS.end() && "Invalid foreground");
    return FOREGROUNDS[str];
}

utils::fmt::AnsiBackground utils::fmt::get_background(std::string str) {
    assert(BACKGROUNDS.find(str) != BACKGROUNDS.end() && "Invalid background");
    return BACKGROUNDS[str];
}

const char* utils::fmt::create_ansi_code() {
    return "\x1b[0m";
}

const char* utils::fmt::create_ansi_code(AnsiFormat format, AnsiForeground foreground) {
    return create_ansi_code(format, foreground, AnsiBackground::None);
}

const char* utils::fmt::create_ansi_code(AnsiFormat format, AnsiForeground foreground, AnsiBackground background) {
    static char buffer[32];

    int fmt = static_cast<int>(format);
    int fg = static_cast<int>(foreground);
    int bg = static_cast<int>(background);

    if (background != AnsiBackground::None) {
        snprintf(buffer, sizeof(buffer), "\x1b[%d;%d;%dm", fmt, fg, bg);
    } else {
        snprintf(buffer, sizeof(buffer), "\x1b[%d;%dm", fmt, fg);
    }
    
    return buffer;
}

std::string utils::fmt::format(const std::string& str, va_list args) {
    std::string result = str;
    size_t i = 0;
    
    while (i < result.size()) {
        if (result[i] == '{') {
            size_t loc = result.find('}', i);
            if (loc == std::string::npos) {
                break;
            }

            std::string fmt = result.substr(i + 1, loc - i - 1);
            std::string value;

            if (fmt.find("|") != std::string::npos) {
                std::vector<std::string> tokens = fmt::split(fmt, '|');

                const char* code;
                if (tokens.size() == 2) {
                    std::string format = tokens[0];
                    std::string foreground = tokens[1];

                    code = fmt::create_ansi_code(fmt::get_format(format), fmt::get_foreground(foreground));
                } else if (tokens.size() == 3) {
                    std::string format = tokens[0];
                    std::string foreground = tokens[1];
                    std::string background = tokens[2];

                    code = fmt::create_ansi_code(
                        fmt::get_format(format), 
                        fmt::get_foreground(foreground), 
                        fmt::get_background(background)
                    );
                } else {
                    assert(false && "Invalid format");
                }

                char* val = va_arg(args, char*);
                value = std::string(code) + val + fmt::create_ansi_code();
            } else if (fmt == "i") {
                value = std::to_string(va_arg(args, int));
            } else if (fmt == "s") {
                value = va_arg(args, std::string);
            } else if (fmt == "f" || fmt == "d") {
                value = std::to_string(va_arg(args, double));
            } else if (fmt == "c") {
                value = std::string(1, va_arg(args, int));
            } else if (fmt == "t") {
                value = va_arg(args, Type*)->str();
            } else {
                value = va_arg(args, char*);
            }

            result.replace(i, loc - i + 1, value);
        } else {
            i++;
        }
    }

    return result;
}

std::string utils::fmt::format(const std::string& str, ...) {
    va_list args;
    va_start(args, str);

    std::string result = utils::fmt::format(str, args);
    va_end(args);

    return result; 
}