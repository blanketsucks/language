#include <quart/source_code.h>
#include <quart/errors.h>
#include <algorithm>

#include <llvm/ADT/StringExtras.h>

namespace quart {

static const HashMap<SourceCode::MessageType, Pair<StringView, StringView>> MESSAGE_TYPES = {
    { SourceCode::MessageType::Error, { "error", "\x1b[1;31m" } },
    { SourceCode::MessageType::Warning, { "warning", "\x1b[1;35m" } },
    { SourceCode::MessageType::Note, { "note", "\x1b[1;36m" } }
};

static Vector<RefPtr<SourceCode>> s_source_codes; // NOLINT

SourceCode::SourceCode(String code, String filename, size_t index) : m_code(move(code)), m_filename(move(filename)), m_index(index) {
    m_line_offsets.push_back(0);

    for (size_t i = 0; i < m_code.size(); i++) {
        if (m_code[i] == '\n') {
            m_line_offsets.push_back(i + 1);
        }
    }
}

RefPtr<SourceCode> SourceCode::lookup(size_t index) {
    if (index >= s_source_codes.size()) {
        return nullptr;
    }

    return s_source_codes[index];
}

RefPtr<SourceCode> SourceCode::create(String code, String filename) {
    auto source_code = RefPtr<SourceCode>(new SourceCode(move(code), move(filename), s_source_codes.size()));
    s_source_codes.push_back(source_code);

    return source_code;
}

RefPtr<SourceCode> SourceCode::from_path(fs::Path path) {
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
    auto source_code = SourceCode::lookup(span.source_code_index());
    auto [line, start] = source_code->line_for(span.start());

    size_t next_line_offset = source_code->next_line_offset(line);
    if (span.end() > next_line_offset) {
        return {}; // FIXME: Support multi-line spans
    }

    StringView code = source_code->code();
    return code.substr(start, next_line_offset - start - 1);
}

String SourceCode::format_generic_message(const Span& span, StringView message, MessageType type) {
    auto& [name, color] = MESSAGE_TYPES.at(type);
    auto source_code = SourceCode::lookup(span.source_code_index());

    auto [lineno, start] = source_code->line_for(span.start());
    auto column = span.start() - start;

    StringView view = source_code->line(span);
    String line = format("{0} | {1}", lineno + 1, view);

    size_t spaces = column + (line.size() - view.size()) - 1;
    size_t size = std::max(span.size(), 1ul);

    line.insert(spaces, color);
    line.insert(spaces + size + color.size(), "\x1b[0m");

    return format(
        "\x1b[1;37m{0}:{1}:{2}: {3}{4}:\x1b[0m {5}\n{6}\n{7}{8}{9}{10}", 
        source_code->filename(), lineno + 1, column + 1,
        color, name, message,
        line,
        String(spaces, ' '),
        color, String(size, '^'), "\x1b[0m"
    );
}

String SourceCode::format_error(Error& error) {
    Vector<String> messages;
    messages.push_back(format_generic_message(error.span(), error.message(), MessageType::Error));

    for (auto& note : error.notes()) {
        messages.push_back(format_generic_message(note.span, note.note, MessageType::Note));
    }

    return llvm::join(messages, "\n");
}

String SourceCode::format_warning(const Span& span, StringView message) {
    return format_generic_message(span, message, MessageType::Warning);
}

String SourceCode::format_note(const Span& span, StringView message) {
    return format_generic_message(span, message, MessageType::Note);
}

}