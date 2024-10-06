#include <quart/language/structs.h>
#include <quart/language/scopes.h>

#include <llvm/ADT/StringExtras.h>

namespace quart {

StructField const* Struct::find(const String& name) const {
    auto it = m_fields.find(name);
    return it == m_fields.end() ? nullptr : &it->second;
}

bool Struct::has_method(const String& name) const {
    return m_scope->resolve<quart::Function>(name) != nullptr;
}

void Struct::set_qualified_name(Scope* parent) {
    Vector<String> parts;
    parts.push_back(name());

    if (!parent) {
        parent = m_scope->parent();
    }

    for (Scope* scope = parent; scope; scope = scope->parent()) {
        if (scope->type() == ScopeType::Global) {
            break;
        }

        parts.push_back(scope->name());
    }

    m_qualified_name = llvm::join(parts.rbegin(), parts.rend(), ".");
}

}