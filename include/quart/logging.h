#pragma once

#include <quart/lexer/lexer.h>

#include <llvm/Support/FormatVariadic.h>

#define FORMAT(fmt, ...) llvm::formatv(fmt, __VA_ARGS__).str()

#define RED     logging::Color::Red
#define WHITE   logging::Color::White
#define MAGENTA logging::Color::Magenta
#define RESET   logging::Color::Reset

#define COLOR(color, s) logging::color(color, s)

#define ERROR(span, ...) do { logging::error(span, llvm::formatv(__VA_ARGS__)); exit(1); } while (0)
#define NOTE(span, ...) logging::note(span, llvm::formatv(__VA_ARGS__))

#define TODO(x) \
    std::cout << FORMAT("{0}:{1} in {2}: '{3}'\n", __FILE__, __LINE__, __PRETTY_FUNCTION__, x); \
    exit(1)

namespace quart {

namespace logging {

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

void error(const Span& span, const std::string& message, bool fatal = true);
void note(const Span& span, const std::string& message);

}

}