#include <quart/logging.h>

#include <algorithm>
#include <sstream>

using namespace quart;

std::string format_location_span(const Span& span) {
    return FORMAT("{0}:{1}:{2}:", span.filename, span.start.line, span.start.column);
}

bool logging::has_color_support() {
    return isatty(fileno(stdout));
}

std::string logging::color_to_str(Color color) {
    return FORMAT("\033[1;{0}m", int(color));
}

std::string logging::color(Color color, const std::string& str) {
    if (logging::has_color_support()) {
        return FORMAT("\033[1;{0}m{1}\033[0;0m", int(color), str);
    } else {
        return str;
    }
}

void logging::error(const Span& span, const std::string& message, bool fatal) {
    std::stringstream stream;

    stream << logging::color(COLOR_WHITE, format_location_span(span)) << ' ';
    stream << logging::color(COLOR_RED, "error:") << ' ';

    stream << message << '\n';

    std::string fmt = FORMAT("{0} | ", span.start.line);
    stream << logging::color(COLOR_WHITE, fmt);

    stream.write(span.line.data(), static_cast<long>(span.line.size()));
    stream << '\n';

    for (u32 i = 0; i < span.start.column - 1 + fmt.length(); i++) stream << ' ';
    for (u32 i = span.start.column; i < span.end.column; i++) stream << '^';

    std::cout << stream.str() << std::endl;
    if (fatal) exit(1);
}

void logging::note(const Span& span, const std::string& message) {
    std::stringstream stream;

    stream << logging::color(COLOR_WHITE, format_location_span(span)) << ' ';
    stream << logging::color(COLOR_MAGENTA, "note:") << ' ';

    stream << message << '\n';

    std::string fmt = FORMAT("{0} | ", span.start.line);
    stream << logging::color(COLOR_WHITE, fmt);
    
    stream.write(span.line.data(), static_cast<long>(span.line.size()));
    stream << '\n';

    for (u32 i = 0; i < span.start.column - 1 + fmt.length(); i++) stream << ' ';
    for (u32 i = span.start.column; i < span.end.column; i++) stream << '^';

    std::cout << stream.str() << std::endl;
}

void logging::error_with_note(
    const Span& error_span,
    const std::string& error,
    const Span& note_span,
    const std::string& note
) {
    logging::error(error_span, error, false);
    logging::note(note_span, note);

    exit(1);
}
