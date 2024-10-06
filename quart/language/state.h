#pragma once

#include <quart/bytecode/generator.h>
#include <quart/language/type_registry.h>
#include <quart/language/scopes.h>
#include <quart/language/impl.h>

namespace quart {

class State {
public:
    State();

    void dump() const;

    Scope* global_scope() const { return m_global_scope; }

    bytecode::Generator& generator() { return m_generator; }
    TypeRegistry& types() { return *m_type_registry; }

    Type* context() const { return m_context; }
    void set_type_context(Type* type) { m_context = type; }

    Scope* scope() const { return m_current_scope; }
    Function* function() const { return m_current_function; }
    Struct* structure() const { return m_current_struct; }
    Module* module() const { return m_current_module; }

    void set_current_scope(Scope* scope) { m_current_scope = scope; }
    void set_current_function(Function* function) { m_current_function = function; }
    void set_current_struct(Struct* structure) { m_current_struct = structure; }
    void set_current_module(Module* module) { m_current_module = module; }

    Type* self_type() const { return m_self_type; }
    void set_self_type(Type* type) { m_self_type = type; }

    ErrorOr<Scope*> resolve_scope(Span, Scope* current_scope, const String& name);
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

    void inject_self(bytecode::Register reg) { m_self = bytecode::Operand(reg); }
    Optional<bytecode::Operand> self() const { return m_self; }
    void reset_self() { m_self = {}; }

    template<typename T, typename... Args>
    inline T* emit(Args&&... args) {
        return m_generator.emit<T>(std::forward<Args>(args)...);
    }

    void add_global_function(RefPtr<Function> function);
    Function const& get_global_function(const String& name) const;

    void add_global_struct(RefPtr<Struct> structure);
    Struct const* get_global_struct(Type* type) const;

    bool has_global_module(const String& name) const;
    RefPtr<Module> get_global_module(const String& name) const;

    void add_global_module(RefPtr<Module> module);

    void add_impl(OwnPtr<Impl>);
    bool has_impl(Type*);
    
    ErrorOr<bytecode::Register> resolve_reference(ast::Expr const&, bool is_mutable = false, Optional<bytecode::Register> dst = {});
    ErrorOr<bytecode::Register> resolve_reference(Scope*, Span, const String& name, bool is_mutable, Optional<bytecode::Register> dst = {});

    ErrorOr<Symbol*> resolve_symbol(ast::Expr const&);
    ErrorOr<Struct*> resolve_struct(ast::Expr const&);

    ErrorOr<bytecode::Operand> type_check_and_cast(Span, bytecode::Operand, Type* target, StringView error_message);

    ErrorOr<bytecode::Operand> generate_attribute_access(ast::AttributeExpr const&, bool as_reference, Optional<bytecode::Register> dst = {}, bool as_mutable = false);

    fs::Path search_import_paths(const String& name);

private:
    bytecode::Generator m_generator;
    OwnPtr<TypeRegistry> m_type_registry;

    Vector<Type*> m_registers;

    Scope* m_global_scope = nullptr;

    size_t m_global_count = 0;

    Type* m_context = nullptr;

    Scope* m_current_scope = nullptr;
    Function* m_current_function = nullptr;
    Struct* m_current_struct = nullptr;
    Module* m_current_module = nullptr;

    Type* m_self_type = nullptr;

    HashMap<Type*, RefPtr<Struct>> m_all_structs;
    HashMap<String, RefPtr<Function>> m_all_functions;

    HashMap<String, RefPtr<Module>> m_modules;

    Optional<bytecode::Operand> m_self;

    HashMap<Type*, OwnPtr<Impl>> m_impls;
    Vector<OwnPtr<Impl>> m_generic_impls;
};

}