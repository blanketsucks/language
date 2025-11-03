#include <quart/language/trait.h>
#include <quart/language/scopes.h>

namespace quart {

Function const* Trait::get_method(const String& name) const {
    return m_scope->resolve<quart::Function>(name);
}

}