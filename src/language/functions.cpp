#include <quart/language/functions.h>
#include <quart/language/scopes.h>

#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/StringExtras.h>

namespace quart {

RefPtr<Function> Function::create(String name, Vector<FunctionParameter> parameters, FunctionType* underlying_type, Scope* scope) {
    return RefPtr<Function>(new Function(move(name), move(parameters), underlying_type, scope));
}

void Function::set_qualified_name() {
    Vector<String> parts;
    parts.push_back(name());

    for (auto* scope = m_scope->parent(); scope; scope = scope->parent()) {
        if (scope->type() == ScopeType::Global) {
            break;
        }

        parts.push_back(scope->name());
    }

    m_qualified_name = llvm::join(parts.rbegin(), parts.rend(), ".");

}

}