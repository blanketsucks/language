#include <quart/source_code.h>
#include <quart/errors.h>
#include <algorithm>

namespace quart {

static Vector<SourceCode> s_source_codes;

SourceCode::SourceCode(String code, String filename, size_t index) : m_code(move(code)), m_filename(move(filename)), m_index(index) {
    m_line_offsets.push_back(0);

    for (size_t i = 0; i < m_code.size(); i++) {
        if (m_code[i] == '\n') {
            m_line_offsets.push_back(i + 1);
        }
    }
}

SourceCode const& SourceCode::lookup(size_t index) {
    // We start SourceCode indicies from 1 so we can reserve 0 to mean a Span without a source code.
    // FIXME: Bounds checking
    return s_source_codes[index - 1];
}

SourceCode const& SourceCode::create(String code, String filename) {
    s_source_codes.push_back({ move(code), move(filename), s_source_codes.size() + 1 });
    return s_source_codes[s_source_codes.size() - 1];
}

SourceCode const& SourceCode::from_path(fs::Path path) {
    auto ss = path.read();
    return SourceCode::create(ss.str(), path);
}

Line SourceCode::line_for(size_t offset) const {
    auto iterator = std::upper_bound(m_line_offsets.begin(), m_line_offsets.end(), offset);

    size_t line = (iterator--) - m_line_offsets.begin() - 1;
    return { line, *iterator };
}

size_t SourceCode::column_for(size_t offset) const {
    auto [_, start] = this->line_for(offset);
    return offset - start;
}

StringView SourceCode::line(const Span& span) {
    // FIXME: Check if the source code index is 0
    SourceCode const& source_code = SourceCode::lookup(span.source_code_index());
    auto [line, start] = source_code.line_for(span.start());

    size_t next_line_offset = source_code.next_line_offset(line);
    if (span.end() > next_line_offset) {
        return {}; // FIXME: Support multi-line spans
    }

    StringView code = source_code.code();
    return code.substr(start, next_line_offset - start - 1);
}

String SourceCode::format_error(Error& error) {
    static const StringView RED = "\x1b[1;31m";

    Span span = error.span();
    SourceCode const& source_code = SourceCode::lookup(span.source_code_index());

    auto [lineno, start] = source_code.line_for(span.start());
    auto column = span.start() - start;

    // FIXME: Do not format with ANSI escape codes if not outputting to a terminal

    StringView view = source_code.line(span);
    String line = format("{0} | {1}", lineno + 1, view);

    size_t spaces = column + (line.size() - view.size()) - 1;
    size_t size = std::max(span.size(), 1ul);

    // Maybe?
if constexpr (false) {
    line.insert(spaces, RED);
    line.insert(spaces + size + RED.size(), "\x1b[0m");
}

    return format(
        "\x1b[1;37m{0}:{1}:{2}: \x1b[1;31merror:\x1b[0m {3}\n{4}\n{5}{6}", 
        source_code.filename(), lineno + 1, column + 1,
        error.message(),
        line,
        String(spaces, ' '),
        String(size, '^')
    );
}

}