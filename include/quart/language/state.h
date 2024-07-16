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

    void set_current_scope(Scope* scope) { m_current_scope = scope; }
    void set_current_function(Function* function) { m_current_function = function; }

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

    ErrorOr<bytecode::Register> resolve_reference(ast::Expr const&, bool is_mutable = false);
    ErrorOr<bytecode::Register> resolve_reference(Scope*, Span, const String& name, bool is_mutable);

    ErrorOr<bytecode::Operand> type_check_and_cast(Span, bytecode::Operand, Type* target, StringView error_message);

private:
    bytecode::Generator m_generator;
    OwnPtr<TypeRegistry> m_type_registry;

    Vector<Type*> m_registers;

    size_t m_global_count = 0;

    Scope* m_current_scope = nullptr;
    Function* m_current_function = nullptr;

    HashMap<Type*, RefPtr<Struct>> m_all_structs;
    HashMap<String, RefPtr<Function>> m_all_functions;

    HashMap<String, RefPtr<Module>> m_modules;

};

}