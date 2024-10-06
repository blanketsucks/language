#include <quart/language/symbol.h>
#include <quart/language/scopes.h>

#include <llvm/ADT/StringExtras.h>

namespace quart {

String Symbol::parse_qualified_name(Symbol* symbol, Scope* scope) {
    Vector<String> parts;
    parts.push_back(symbol->name());

    for (; scope; scope = scope->parent()) {
        if (scope->type() == ScopeType::Global) {
            break;
        }

        parts.push_back(scope->name());
    }

    return llvm::join(parts.rbegin(), parts.rend(), ".");
}

}