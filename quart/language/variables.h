#pragma once

#include <quart/language/types.h>
#include <quart/language/symbol.h>
#include <quart/bytecode/register.h>
#include <quart/common.h>

namespace quart {

class State;

class Variable : public Symbol {
public:
    static bool classof(const Symbol* symbol) { return symbol->type() == Symbol::Variable; }

    enum Flags : u8 {
        None,
        Reference = 1 << 0,
        Mutable   = 1 << 1,
        Used      = 1 << 2,
        Mutated   = 1 << 3,
        Constant  = 1 << 4
    };

    static RefPtr<Variable> create(String name, size_t index, Type* type, u8 flags = None) {
        return RefPtr<Variable>(new Variable(move(name), index, type, flags));
    }

    u8 flags() const { return m_flags; }

    size_t index() const { return m_index; }
    Type* value_type() const { return m_type; }

    bool is_reference() const { return m_flags & Reference; }
    bool is_mutable() const { return m_flags & Mutable; }

    bool is_used() const { return m_flags & Used; }
    bool is_mutated() const { return m_flags & Mutated; }

    bool is_constant() { return m_flags & Constant; }

    void emit(State&, bytecode::Register dst);

private:
    Variable(
        String name, size_t index, Type* type, u8 flags = None
    ) : Symbol(move(name), Symbol::Variable), m_index(index), m_type(type), m_flags(flags) {}

    void set_used(bool used) { m_flags = used ? m_flags | Used : m_flags & ~Used; }
    void set_mutated(bool mutated) { m_flags = mutated ? m_flags | Mutated : m_flags & ~Mutated; }
    
    size_t m_index;
    Type* m_type;

    u8 m_flags = None;
};

// struct Variable {
//     enum Flags : u8 {
//         None,
//         StackAllocated = 1 << 0,
//         Reference      = 1 << 1,
//         Mutable        = 1 << 2,
//         Used           = 1 << 3,
//         Mutated        = 1 << 4,
//     };

//     std::string name;
//     quart::Type* type;

//     llvm::Value* value;
//     llvm::Constant* constant;

//     u8 flags;

//     Span span;

//     static Variable from_alloca(
//         const std::string& name,
//         llvm::AllocaInst* alloca,
//         quart::Type* type,
//         u8 flags,
//         const Span& span
//     );

//     static Variable from_value(
//         const std::string& name,
//         llvm::Value* value,
//         quart::Type* type,
//         u8 flags,
//         const Span& span
//     );

//     bool is_used() const { return this->flags & Variable::Used; }
//     bool is_mutated() const { return this->flags & Variable::Mutated; }
//     bool is_mutable() const { return this->flags & Variable::Mutable; }

//     bool can_ignore_usage() const { return this->name[0] == '_'; }
// };


}