#include <quart/source_code.h>
#include <quart/errors.h>
#include <algorithm>

namespace quart {

SourceCode::SourceCode(String code, String filename) : m_code(move(code)), m_filename(move(filename)) {
    m_line_offsets.push_back(0);

    for (size_t i = 0; i < m_code.size(); i++) {
        if (m_code[i] == '\n') {
            m_line_offsets.push_back(i + 1);
        }
    }
}

SourceCode SourceCode::from_path(fs::Path path) {
    auto ss = path.read();
    return { ss.str(), String(path) };
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

StringView SourceCode::line(const Span& span) const {
    auto [line, start] = this->line_for(span.start());

    size_t next_line_offset = this->next_line_offset(line);
    if (span.end() > next_line_offset) {
        return {}; // FIXME: Support multi-line spans
    }

    StringView code { m_code };
    return code.substr(start, next_line_offset - start - 1);
}

String SourceCode::format_error(Error& error) const {
    Span span = error.span();

    auto [lineno, start] = this->line_for(span.start());
    auto column = span.start() - start;

    // FIXME: Do not format with ANSI escape codes if not outputting to a terminal

    StringView view = this->line(span);
    String line = format("{0} | {1}", lineno + 1, view);

    return format(
        "\x1b[1;37m{0}:{1}:{2}: \x1b[1;31merror:\x1b[0m {3}\n{4}\n{5}{6}", 
        m_filename, lineno + 1, column + 1,
        error.message(),
        line,
        String(column + (line.size() - view.size()) - 1, ' '),
        String(span.size(), '^')
    );
}

}