#ifndef _UTILS_LOG_H
#define _UTILS_LOG_H

#include <quart/lexer/lexer.h>
#include <quart/utils/filesystem.h>

#include "llvm/Support/FormatVariadic.h"

#define FORMAT(fmt, ...) llvm::formatv(fmt, __VA_ARGS__).str()

#define RED     utils::Color::Red
#define WHITE   utils::Color::White
#define MAGENTA utils::Color::Magenta
#define RESET   utils::Color::Reset

#define COLOR(color, s) utils::color(color, s)

#define ERROR(loc, ...) utils::error(loc, llvm::formatv(__VA_ARGS__)); exit(1)
#define NOTE(loc, ...) utils::note(loc, llvm::formatv(__VA_ARGS__))

#define TODO(x) \
    std::cout << FORMAT("{0}:{1} in {2}: '{3}'\n", __FILE__, __LINE__, __PRETTY_FUNCTION__, x); \
    exit(1)

namespace utils {

enum class Color {
    Reset = 0,
    Red = 31,
    White = 37,
    Magenta = 35
};

bool has_color_support();

std::string color_to_str(Color color);
std::string color(Color color, const std::string& str);

void underline_error(
    std::stringstream& stream,
    const std::string& line,
    uint32_t line_number,
    uint32_t start_index,
    uint32_t end_index,
    uint32_t start_column
);

void error(Span span, const std::string& message, bool fatal = true);
void note(Span span, const std::string& message);

}

#endif