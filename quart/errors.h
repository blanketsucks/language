#pragma once

#include <quart/common.h>
#include <quart/source_code.h>
#include <quart/format.h>

#define TRY(expr)                       \
    ({                                  \
        auto result = (expr);           \
        if (result.is_err()) {          \
            return result.error();      \
        }                               \
        result.release_value();         \
    })


namespace quart {

struct Note {
    Span span;
    String note;

    Note(Span span, String note) : span(span), note(move(note)) {}
};

enum class ErrorType : u8 {
    Generic,

    EndOfString,

    UnknownIdentifier,
    MutabilityMismatch,
};

class Error {
public:
    Error() = default;
    Error(Span span, String error) : m_span(span), m_error(move(error)) {}
    Error(Span span, ErrorType type, String error) : m_span(span), m_error(move(error)), m_type(type) {}

    Span span() const { return m_span; }
    StringView message() const { return m_error; }

    Vector<Note> const& notes() const { return m_notes; }

    ErrorType type() const { return m_type; }

    void add_note(Span span, String note) {
        m_notes.emplace_back(span, move(note));
    }

private:
    Span m_span;
    String m_error;

    Vector<Note> m_notes;
    ErrorType m_type = ErrorType::Generic;
};

template<typename ...Args>
Error err(Span span, std::format_string<Args...> fmt, Args... args) {
    return { span, format(fmt, std::forward<Args>(args)...) };
}

template<typename ...Args>
Error err(ErrorType type, Span span, std::format_string<Args...> fmt, Args... args) {
    return { span, type, format(fmt, std::forward<Args>(args)...) };
}

template<typename ...Args>
Error err(std::format_string<Args...> fmt, Args... args) {
    return { Span {}, format(fmt, std::forward<Args>(args)...) };
}

template<typename ...Args>
void warn(Span span, std::format_string<Args...> fmt, Args... args) {
    String message = format(fmt, std::forward<Args>(args)...);
    outln(SourceCode::format_warning(span, message));
}

template<typename ...Args>
void note(Span span, std::format_string<Args...> fmt, Args... args) {
    String message = format(fmt, std::forward<Args>(args)...);
    outln(SourceCode::format_note(span, message));
}

template<typename T, typename E>
class [[nodiscard]] Result {
public:
    Result() : m_value(), m_error(), has_value(true) {}

    Result(const T& value) : m_value(value), m_error(), has_value(true) {}
    Result(T&& value) : m_value(move(value)), m_error(), has_value(true) {}

    Result(const E& error) : m_value(), m_error(error), has_value(false) {}
    Result(E&& error) : m_value(), m_error(move(error)), has_value(false) {}

    bool is_ok() const { return has_value; }
    bool is_err() const { return !is_ok(); }

    T& value() { return m_value; }
    E& error() { return m_error; }

    T release_value() { return move(m_value); }
    E release_error() { return move(m_error); }

protected:
    void set_value(T&& value) {
        m_value = move(value);
        has_value = true;
    }

    void set_error(E&& error) {
        m_error = move(error);
        has_value = false;
    }

private:
    T m_value;
    E m_error;

    bool has_value;
};

template<typename E>
class [[nodiscard]] Result<void, E> {
public:
    Result() : m_error(), has_err(false) {}
    Result(const E& error) : m_error(error), has_err(true) {}

    bool is_err() const { return has_err; }
    bool is_ok() const { return !is_err(); }

    void value() const { return; } // Added here to make it possible to use the TRY() macro

    const E& error() const { return m_error; }
    E& error() { return m_error; }

    void release_value() { return; }
private:
    E m_error;
    bool has_err;
};

template<typename T>
using ErrorOr = Result<T, Error>;

template<typename T>
ErrorOr<T> Ok(const T& value) {
    return { value };
}

template<typename T>
ErrorOr<T> Ok(T&& value) {
    return { std::forward(value) };
}

}