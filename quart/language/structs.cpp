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
    if (!parent) {
        parent = m_scope->parent();
    }

    m_qualified_name = Symbol::parse_qualified_name(this, parent);
}

}