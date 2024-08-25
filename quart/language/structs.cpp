#include <quart/language/structs.h>
#include <quart/language/scopes.h>

namespace quart {

StructField const* Struct::find(const String& name) const {
    auto it = m_fields.find(name);
    return it == m_fields.end() ? nullptr : &it->second;
}

bool Struct::has_method(const String& name) const {
    return m_scope->resolve<quart::Function>(name) != nullptr;
}

}