#pragma once

#include <stdint.h>
#include <string>
#include <sstream>

namespace quart {

struct Location {
    uint32_t line;
    uint32_t column;
    size_t index;
};

struct Span {
    Location start;
    Location end;

    std::string filename;
    std::string line;

    Span() = default;
    Span(Location start, Location end, const std::string& filename, const std::string& line);

    static Span merge(const Span& start, const Span& end);

    size_t length() const;
};

}
