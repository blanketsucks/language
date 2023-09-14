#include <quart/lexer/location.h>

using namespace quart;

Span::Span(Location start, Location end, const std::string& filename, const std::string& line) {
    this->start = start;
    this->end = end;

    this->filename = filename;
    this->line = line;
}

Span Span::merge(const Span& start, const Span& end) {
    return Span { start.start, end.end, start.filename, start.line };
}

size_t Span::length() const {
    return this->end.index - this->start.index;
}