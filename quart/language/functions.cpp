#include <quart/language/functions.h>
#include <quart/language/scopes.h>

#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/StringExtras.h>

namespace quart {

RefPtr<Function> Function::create(
    String name,
    Vector<FunctionParameter> parameters,
    FunctionType* underlying_type, 
    Scope* scope,
    LinkageSpecifier linkage_specifier
) {
    return RefPtr<Function>(new Function(move(name), move(parameters), underlying_type, scope, linkage_specifier));
}

void Function::set_qualified_name() {
    if (m_linkage_specifier == LinkageSpecifier::C) {
        m_qualified_name = name();
    } else {
        m_qualified_name = Symbol::parse_qualified_name(this, m_scope->parent());
    }
}

}