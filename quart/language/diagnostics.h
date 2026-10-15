#pragma once

#include <quart/errors.h>

namespace quart {

class Diagnostics {
public:
    static OwnPtr<Diagnostics> create() {
        return OwnPtr<Diagnostics>(new Diagnostics());
    }

    void clear() {
        m_errors.clear();
        m_notes.clear();
    }

    bool empty() {
        return m_errors.empty() && m_notes.empty();
    }

    bool has_errors() const { return !m_errors.empty(); }

    Vector<Error> const& errors() const { return m_errors; }
    Vector<Note> const& notes() const { return m_notes; }

    void add_error(const Error& err) { m_errors.push_back(err); }
    void add_note(const Note& note) { m_notes.push_back(note); }

private:
    Diagnostics() = default;

    Vector<Error> m_errors;
    Vector<Note>  m_notes;
};

}