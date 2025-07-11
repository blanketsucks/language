#pragma once

#include <quart/language/types.h>
#include <quart/language/symbol.h>

#include <llvm/IR/Value.h>

#include <map>

namespace quart {

class Scope;

class Enum : public Symbol {
public:
    static bool classof(Symbol const* symbol) { return symbol->type() == Symbol::TypeAlias; }

    static RefPtr<Enum> create(String name, quart::Type* underlying_type, RefPtr<Scope>);

    quart::Type* underlying_type() const { return m_underlying_type; }
    RefPtr<Scope> scope() const { return m_scope; }

private:
    Enum(
        String name, quart::Type* underlying_type, RefPtr<Scope> scope
    ) : Symbol(move(name), Symbol::Enum, false), m_underlying_type(underlying_type), m_scope(move(scope)) {}


    quart::Type* m_underlying_type;
    RefPtr<Scope> m_scope;
};

// struct Enumerator {
//     std::string name;

//     llvm::Constant* value;
//     quart::Type* type;
// };

// struct Enum {
//     std::string name;
//     quart::Type* type;

//     Scope* scope;

//     std::map<std::string, Enumerator> enumerators;

//     Enum(const std::string& name, quart::Type* type);

//     void add_enumerator(const std::string& name, llvm::Constant* value, const Span& span);
//     bool has_enumerator(const std::string& name);

//     Enumerator* get_enumerator(const std::string& name);
// };

}