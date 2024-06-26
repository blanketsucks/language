#pragma once

#include <quart/common.h>
#include <quart/filesystem.h>

namespace quart {

class Span {
public:
    Span() = default;

    Span(size_t start, size_t end) : m_start(start), m_end(end) {}
    Span(const Span& start, const Span& end) : m_start(start.start()), m_end(end.end()) {}

    size_t start() const { return m_start; }
    size_t end() const { return m_end; }

    size_t size() const { return m_end - m_start; }

    void set_start(size_t start) { m_start = start; }
    void set_end(size_t end) { m_end = end; }

private:
    size_t m_start = 0;
    size_t m_end = 0;
};

struct Line {
    size_t num;    // The number of the line
    size_t offset; // The offset of the line in the source code
};

class SourceCode {
public:
    SourceCode(String code, String filename);

    static SourceCode from_path(fs::Path);

    StringView code() const { return m_code; }
    StringView filename() const { return m_filename; }

    Line line_for(size_t offset) const;
    size_t column_for(size_t offset) const;

    StringView line(const Span&) const;

    String format_error(class Error&) const;

private:
    size_t next_line_offset(size_t line) const {
        return m_line_offsets[line + 1];
    }

    String m_code;
    String m_filename;

    Vector<size_t> m_line_offsets;
};

}