#include <quart/language/scopes.h>

namespace quart {

RefPtr<Scope> Scope::create(String name, ScopeType type, RefPtr<Scope> parent) {
    auto scope = RefPtr<Scope>(new Scope(move(name), type, move(parent)));
    if (scope->parent()) {
        scope->parent()->m_children.push_back(scope);
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