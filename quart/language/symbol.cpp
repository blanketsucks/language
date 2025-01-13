#include <quart/language/symbol.h>
#include <quart/language/scopes.h>

#include <llvm/ADT/StringExtras.h>

namespace quart {

StringView Symbol::str() const {
    switch (m_type) {
        case Variable: return "Variable";
        case Function: return "Function";
        case Struct: return "Struct";
        case Enum: return "Enum";
        case TypeAlias: return "TypeAlias";
        case Module: return "Module";
        case Trait: return "Trait";
    }

    return {};
}

String Symbol::parse_qualified_name(Symbol* symbol, Scope* scope) {
    Vector<String> parts;
    parts.push_back(symbol->name());

    for (; scope; scope = scope->parent()) {
        if (scope->type() == ScopeType::Global) {
            break;
        } else if (scope->type() == ScopeType::Module) {
            class Module* module = scope->module();
            parts.push_back(module->qualified_name());
        } else {
            parts.push_back(scope->name());
        }
    }

    return llvm::join(parts.rbegin(), parts.rend(), "::");
}

}