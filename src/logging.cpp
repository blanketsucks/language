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

    stream << logging::color(WHITE, format_location_span(span)) << ' ';
    stream << logging::color(RED, "error:") << ' ';

    stream << message << '\n';

    std::string fmt = FORMAT("{0} | ", span.start.line);
    stream << logging::color(WHITE, fmt) << span.line << '\n';

    size_t len = span.line.length();
    size_t offset = std::min(span.start.column - 1 + fmt.length(), len);

    uint32_t padding = span.length() <= 0 ? 1 : span.length();
    for (size_t i = 0; i < offset; i++) stream << ' ';
    for (size_t i = 0; i < padding; i++) stream << '^';

    stream << '\n';

    std::cout << stream.str() << std::endl;
    if (fatal) {
        exit(1);
    }
}

void logging::note(const Span& span, const std::string& message) {
    std::stringstream stream;

    stream << logging::color(WHITE, format_location_span(span)) << ' ';
    stream << logging::color(MAGENTA, "note:") << ' ';

    stream << message << '\n';

    std::string fmt = FORMAT("{0} | ", span.start.line);
    stream << logging::color(WHITE, fmt) << span.line << '\n';

    size_t len = span.line.length();
    size_t offset = std::min(span.start.column - 1 + fmt.length(), len);

    for (size_t i = 0; i < offset; i++) stream << ' ';
    for (size_t i = 0; i < span.length(); i++) stream << '^';

    stream << '\n';
    std::cout << stream.str() << std::endl;
}
