#pragma once

#include <quart/bytecode/generator.h>
#include <quart/language/type_registry.h>
#include <quart/language/scopes.h>

namespace quart {

class State {
public:
    State();

    bytecode::Generator& generator() { return m_generator; }
    TypeRegistry& types() { return *m_type_registry; }

    Scope* scope() const { return m_current_scope; }
    Function* function() const { return m_current_function; }
    Struct* structure() const { return m_current_struct; }

    void set_current_scope(Scope* scope) { m_current_scope = scope; }
    void set_current_function(Function* function) { m_current_function = function; }
    void set_current_struct(Struct* structure) { m_current_struct = structure; }

    ErrorOr<Scope*> resolve_scope_path(Span, const Path&);
    
    Vector<bytecode::Instruction*> const& global_instructions() const { return m_generator.global_instructions(); }

    bytecode::BasicBlock* create_block(String name = {}) { return m_generator.create_block(move(name)); }
    void switch_to(bytecode::BasicBlock* block);

    bytecode::BasicBlock* current_block() { return m_generator.current_block(); }

    size_t register_count() { return m_generator.register_count(); }

    bytecode::Register allocate_register();
    void set_register_type(bytecode::Register reg, quart::Type* type);

    size_t global_count() const { return m_global_count; }
    size_t allocate_global() { return m_global_count++; }

    Type* type(bytecode::Register) const;
    Type* type(bytecode::Operand) const;

    template<typename T, typename... Args>
    inline T* emit(Args&&... args) {
        return m_generator.emit<T>(std::forward<Args>(args)...);
    }

    void add_global_function(RefPtr<Function> function) {
        m_all_functions[function->qualified_name()] = function;
    }

    Function const& get_global_function(const String& name) const {
        return *m_all_functions.at(name);
    }

    void add_global_struct(RefPtr<Struct> structure) {
        m_all_structs[structure->underlying_type()] = structure;
    }

    Struct const* get_global_struct(Type* type) const {
        if (auto iterator = m_all_structs.find(type); iterator != m_all_structs.end()) {
            return iterator->second.get();
        }

        return nullptr;
    }

    ErrorOr<bytecode::Register> resolve_reference(ast::Expr const&, bool is_mutable = false, Optional<bytecode::Register> dst = {});
    ErrorOr<bytecode::Register> resolve_reference(Scope*, Span, const String& name, bool is_mutable, Optional<bytecode::Register> dst = {});

    ErrorOr<Symbol*> resolve_symbol(ast::Expr const&);
    ErrorOr<Struct*> resolve_struct(ast::Expr const&);

    ErrorOr<bytecode::Operand> type_check_and_cast(Span, bytecode::Operand, Type* target, StringView error_message);

    ErrorOr<bytecode::Operand> generate_attribute_access(ast::AttributeExpr const&, bool as_reference, Optional<bytecode::Register> dst = {}, bool as_mutable = false);

private:
    bytecode::Generator m_generator;
    OwnPtr<TypeRegistry> m_type_registry;

    Vector<Type*> m_registers;

    size_t m_global_count = 0;

    Scope* m_current_scope = nullptr;
    Function* m_current_function = nullptr;
    Struct* m_current_struct = nullptr;

    HashMap<Type*, RefPtr<Struct>> m_all_structs;
    HashMap<String, RefPtr<Function>> m_all_functions;

    HashMap<String, RefPtr<Module>> m_modules;

};

}