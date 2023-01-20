#ifndef _LEXER_LOCATION_H
#define _LEXER_LOCATION_H

#include <stdint.h>
#include <string>
#include <sstream>

struct Location {
    uint32_t line;
    uint32_t column;
    uint32_t index;
};

struct Span {
    Location start;
    Location end;

    const char* filename;
    std::string line;

    static Span from_span(const Span& start, const Span& end) {
        return Span { start.start, end.end, start.filename, start.line };
    }

    uint32_t length() const {
        return end.index - start.index;
    }
};

#endif