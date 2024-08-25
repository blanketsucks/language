#include <quart/assert.h>

#include <sstream>

namespace quart {

bool has_color_support() {
    return isatty(fileno(stdout));
}

void assertion_failed(const char* message, const char* file, u32 line, const char* function) {
    std::stringstream stream;

    stream << format("\x1b[1;37m{0}:{1}:{2}: \x1b[1;31massertion failed: \x1b[0m", file, line, function) << ' ';
    stream << message << '\n';

    std::cout << stream.str() << std::endl;
}

}