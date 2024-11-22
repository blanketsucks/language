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
};

class Error {
public:
    Error() = default;
    Error(Span span, String error) : m_span(span), m_error(move(error)) {}

    Span span() const { return m_span; }
    StringView message() const { return m_error; }
    Vector<Note> const& notes() const { return m_notes; }

    void add_note(Span span, String note) {
        m_notes.emplace_back(span, move(note));
    }

private:
    Span m_span;
    String m_error;

    Vector<Note> m_notes;
};

template<typename ...Args> requires(std::conjunction_v<has_format_provider<Args>...>)
Error err(Span span, const char* fmt, Args... args) {
    return { span, format(fmt, std::forward<Args>(args)...) };
}

template<typename ...Args> requires(std::conjunction_v<has_format_provider<Args>...>)
Error err(const char* fmt, Args... args) {
    return { Span {}, format(fmt, std::forward<Args>(args)...) };
}

template<typename T, typename E>
class Result {
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
class Result<void, E> {
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