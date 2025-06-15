#pragma once

#include <quart/common.h>
#include <quart/filesystem.h>

namespace quart {

class Span {
public:
    Span() = default;

    Span(size_t start, size_t end, u16 source_code_index) : m_start(start), m_source_code_index(source_code_index), m_end(end) {}
    Span(const Span& start, const Span& end) : m_start(start.start()), m_source_code_index(start.source_code_index()), m_end(end.end()) {}

    size_t start() const { return m_start; }
    size_t end() const { return m_end; }

    u16 source_code_index() const { return m_source_code_index; }

    size_t size() const { return m_end - m_start; }

    void set_start(size_t start) { m_start = start; }
    void set_end(size_t end) { m_end = end; }

private:
    size_t m_start : 48 = 0;
    u16 m_source_code_index = 0;

    size_t m_end = 0;
};

struct Line {
    size_t num;    // The number of the line
    size_t offset; // The offset of the line in the source code
};

class SourceCode {
public:
    enum class MessageType {
        Error,
        Warning,
        Note
    };

    static RefPtr<SourceCode> create(String code, String filename);
    static RefPtr<SourceCode> from_path(fs::Path);

    static RefPtr<SourceCode> lookup(size_t index);

    u16 index() const { return m_index; }
    StringView code() const { return m_code; }
    StringView filename() const { return m_filename; }

    Line line_for(size_t offset) const;
    size_t column_for(size_t offset) const;

    static StringView line(const Span&);

    static String format_note(const Span&, StringView message);
    static String format_warning(const Span&, StringView message);
    static String format_error(class Error&);

    static String format_generic_message(const Span&, StringView message, MessageType);
private:
    SourceCode(String code, String filename, size_t index);

    size_t next_line_offset(size_t line) const {
        return m_line_offsets[line + 1];
    }

    String m_code;
    String m_filename;

    size_t m_index;
    Vector<size_t> m_line_offsets;
};

}