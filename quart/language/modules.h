#pragma once

#include <quart/filesystem.h>
#include <quart/language/symbol.h>

#include <memory>

namespace quart {

class Scope;

class Module : public Symbol {
public:
    static bool classof(Symbol const* symbol) { return symbol->type() == Symbol::Module; }

    enum State {
        Ready,
        Importing
    };

    static RefPtr<Module> create(String name, String qualified_name, fs::Path path, Scope* scope) {
        return RefPtr<Module>(new Module(move(name), move(qualified_name), move(path), scope));
    }

    String const& qualified_name() const { return m_qualified_name; }

    fs::Path const& path() const { return m_path; }
    Scope* scope() const { return m_scope; }

    bool is_ready() const { return m_state == State::Ready; }
    bool is_importing() const { return m_state == State::Importing; }

    void set_state(State state) { m_state = state; }
private:
    Module(
        String name, String qualified_name, fs::Path path, Scope* scope
    ) : Symbol(move(name), Symbol::Module), m_qualified_name(move(qualified_name)), m_path(move(path)), m_scope(scope) {}

    String m_qualified_name;

    fs::Path m_path;
    Scope* m_scope;

    State m_state = State::Importing;
};

}