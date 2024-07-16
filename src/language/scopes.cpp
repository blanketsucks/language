#include <quart/language/scopes.h>

namespace quart {

Scope* Scope::create(String name, ScopeType type, Scope* parent) {
    auto* scope = new Scope(move(name), type, parent);
    if (parent) {
        parent->m_children.push_back(scope);
    }

    return scope;
}

Symbol* Scope::resolve(const String& name) {
    auto iterator = m_symbols.find(name);
    if (iterator == m_symbols.end()) {
        if (!m_parent) {
            return nullptr;
        }

        return m_parent->resolve(name);
    }

    return iterator->second.get();
}

void Scope::finalize(bool) {}

}