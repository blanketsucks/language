#include <quart/scopes.h>

namespace quart {

RefPtr<Scope> Scope::create(String name, ScopeType type, Scope* parent) {
    auto scope = RefPtr<Scope>(new Scope(move(name), type, parent));
    if (parent) {
        parent->add_child(scope);
    }

    return scope;
}

RefPtr<Scope> Scope::clone(String name) {
    auto scope = Scope::create(move(name), m_type, m_parent);
    for (auto& [symbol_name, symbol] : m_symbols) {
        scope->m_symbols[symbol_name] = symbol;
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