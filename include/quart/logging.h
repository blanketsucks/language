#pragma once

#include <quart/lexer/lexer.h>

#include <llvm/Support/FormatVariadic.h>

#define FORMAT(fmt, ...) llvm::formatv(fmt, __VA_ARGS__).str()

#define COLOR_RED     logging::Color::Red
#define COLOR_WHITE   logging::Color::White
#define COLOR_MAGENTA logging::Color::Magenta
#define COLOR_RESET   logging::Color::Reset

#define COLOR(color, s) logging::color(color, s)

#define ERROR(span, ...) do { logging::error(span, llvm::formatv(__VA_ARGS__)); exit(1); } while (0)
#define NOTE(span, ...) logging::note(span, llvm::formatv(__VA_ARGS__))
#define ERROR_WITH_NOTE(error_span, error, note_span, note) logging::error_with_note(error_span, error, note_span, note)

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
    u32 line_number, u32 start_index,
    u32 end_index, u32 start_column
);

void error(const Span& span, const std::string& message, bool fatal = true);
void note(const Span& span, const std::string& message);

void error_with_note(
    const Span& error_span,
    const std::string& error,
    const Span& note_span,
    const std::string& note
);


}

}