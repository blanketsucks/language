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

template<typename T> T* Scope::as() const {
    if (!m_data || !is<T>()) {
        return nullptr;
    }

    return static_cast<T*>(m_data);
}

template<typename T> bool Scope::is() const {
    switch (m_type) {
        case ScopeType::Function:
            return std::is_same_v<T, Function>;
        case ScopeType::Struct:
            return std::is_same_v<T, Struct>;
        case ScopeType::Module:
            return std::is_same_v<T, Module>;
        default:
            return false;
    }
}

// static void finalize(Function& func) {
//     if (func.flags & Function::Finalized) return;

//     llvm::Function* function = func.value;
//     if (function->use_empty() && !func.is_entry()) {
//         if (function->getParent()) {
//             function->eraseFromParent();
//         }

//         for (auto& call : func.calls) {
//             if (!call->use_empty() || !call->getParent()) {
//                 continue;
//             }

//             call->eraseFromParent();
//         }
//     }

//     func.flags |= Function::Finalized;
// }

void Scope::finalize(bool) {}

}