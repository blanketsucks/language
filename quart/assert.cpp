#include <quart/assert.h>
#include <quart/format.h>

namespace quart {

bool has_color_support() {
    return isatty(fileno(stdout));
}

void assertion_failed(StringView message, StringView condition, StringView file, u32 line, StringView function) {
    String fmt = format(
        "{}:{} in `{}`: Assertion failed ({})",
        file, line, function, condition
    );

    auto& err = std::cerr;

    err << fmt;
    if (!message.empty()) {
        err << ": " << message;
    }

    err << std::endl;
}

}