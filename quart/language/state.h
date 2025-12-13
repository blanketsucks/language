#pragma once

#include <quart/bytecode/generator.h>
#include <quart/language/context.h>
#include <quart/language/scopes.h>
#include <quart/language/impl.h>
#include <quart/language/trait.h>
#include <quart/language/type_checker.h>

namespace quart {

struct RegisterState {
    enum Flags {
        None,
        Constructor = 1 << 0,
        Struct = 1 << 1
    };

    Type* type = nullptr;
    Function* function = nullptr;

    u8 flags = 0;
};

class State {
public:
    State();
    ~State() = default;

    void dump() const;

    Vector<OwnPtr<bytecode::Instruction>> const& global_instructions() const { return m_generator.global_instructions(); }

    bytecode::BasicBlock* current_block() { return m_generator.current_block(); }
    size_t register_count() { return m_generator.register_count(); }

    RefPtr<Scope> global_scope() const { return m_global_scope; }

    bytecode::Generator& generator() { return m_generator; }
    Context& context() { return *m_context; }

    ConstantEvaluator& constant_evaluator() { return m_constant_evaluator; }
    TypeChecker& type_checker() { return m_type_checker; }

    Type* type_context() const { return m_type_context; }
    
    RefPtr<Scope> scope() const { return m_current_scope; }
    Function* function() const { return m_current_function; }
    Struct* structure() const { return m_current_struct; }
    Module* module() const { return m_current_module; }
    
    Type* self_type() const { return m_self_type; }

    HashMap<String, RefPtr<Function>> const& functions() const { return m_all_functions; }

    HashMap<Type*, OwnPtr<Impl>> const& impls() const { return m_impls; }
    Vector<OwnPtr<Impl>> const& generic_impls() const { return m_generic_impls; }

    size_t global_count() const { return m_global_count; }
    Vector<RefPtr<Variable>> const& globals() const { return m_globals; }

    Optional<bytecode::Register> self() const { return m_self; }
    Optional<bytecode::Register> return_register() const { return m_return; }
    
    void set_current_scope(RefPtr<Scope> scope) { m_current_scope = move(scope); }
    void set_current_function(Function* function) { m_current_function = function; }
    void set_current_struct(Struct* structure) { m_current_struct = structure; }
    void set_current_module(Module* module) { m_current_module = module; }
    
    void set_self_type(Type* type) { m_self_type = type; }
    void set_type_context(Type* type) { m_type_context = type; }
    
    ErrorOr<RefPtr<Scope>> resolve_scope(Span, Scope& current_scope, const String& name);
    ErrorOr<RefPtr<Scope>> resolve_scope_path(Span, const Path&, bool allow_generic_arguments = false);

    ErrorOr<Symbol*> access_symbol(Span, const Path&);

    bytecode::BasicBlock* create_block(String name = {}) { return m_generator.create_block(move(name)); }
    void switch_to(bytecode::BasicBlock* block);

    bytecode::Register allocate_register();

    void set_register_state(bytecode::Register reg, quart::Type* type, Function* function = nullptr, u8 flags = 0);
    void set_register_flags(bytecode::Register reg, u8 flags) { m_registers[reg.index()].flags = flags; }

    RegisterState const& register_state(bytecode::Register reg) const { return m_registers[reg.index()]; }

    size_t allocate_global() { return m_global_count++; }

    Type* type(bytecode::Register) const;
    Type* type(bytecode::Operand const&) const;

    void inject_self(bytecode::Register reg) { m_self = reg; }
    void reset_self() { m_self = {}; }

    void inject_return(bytecode::Register reg) { m_return = reg; }
    void reset_return() { m_return = {}; }

    template<typename T, typename... Args>
    inline T* emit(Args&&... args) {
        return m_generator.emit<T>(std::forward<Args>(args)...);
    }

    void add_global(RefPtr<Variable> variable) {
        m_globals.push_back(move(variable));
    }

    bool has_global_function(const String& name) const;
    void add_global_function(RefPtr<Function> function);
    Function const* get_global_function(const String& name) const;

    bool has_global_module(const String& name) const;
    RefPtr<Module> get_global_module(const String& name) const;

    void add_global_module(RefPtr<Module> module);

    void add_impl(OwnPtr<Impl>);
    bool has_impl(Type*);

    void add_trait(RefPtr<Trait> trait) {
        m_traits[trait->underlying_type()] = move(trait);
    }

    Trait const* get_trait(Type* type) const {
        auto iterator = m_traits.find(type);
        if (iterator != m_traits.end()) {
            return iterator->second.get();
        }

        return nullptr;
    }
    
    ErrorOr<bytecode::Register> resolve_reference(
        ast::Expr const&, 
        bool is_mutable = false, 
        Optional<bytecode::Register> dst = {},
        bool use_default_case = true,
        bool override_mutability = false
    );

    ErrorOr<bytecode::Register> resolve_reference(
        Scope&, Span,
        const String& name,
        bool is_mutable,
        Optional<bytecode::Register> dst = {},
        bool override_mutability = false
    );

    ErrorOr<Symbol*> resolve_symbol(ast::Expr const&);
    ErrorOr<Struct*> resolve_struct(ast::Expr const&);

    ErrorOr<bytecode::Operand> type_check_and_cast(Span, bytecode::Operand, Type* target, StringView error_message);

    ErrorOr<bytecode::Register> generate_attribute_access(
        ast::AttributeExpr const&,
        bool as_reference,
        bool as_mutable = false,
        Optional<bytecode::Register> dst = {}
    );

    ErrorOr<bytecode::Register> generate_index_access(
        ast::IndexExpr const&,
        bool as_reference,
        bool as_mutable = false,
        Optional<bytecode::Register> dst = {}
    );

    fs::Path search_import_paths(const String& name);

    Type* get_type_from_builtin(ast::BuiltinType);

    ErrorOr<size_t> size_of(ast::Expr const&);

private:
    bytecode::Generator m_generator;
    OwnPtr<Context> m_context;

    ConstantEvaluator m_constant_evaluator;
    TypeChecker m_type_checker;

    Vector<RegisterState> m_registers;

    RefPtr<Scope> m_global_scope = nullptr;

    size_t m_global_count = 0;

    Type* m_type_context = nullptr;

    RefPtr<Scope> m_current_scope = nullptr;
    Function* m_current_function = nullptr;
    Struct* m_current_struct = nullptr;
    Module* m_current_module = nullptr;

    Type* m_self_type = nullptr;

    HashMap<String, RefPtr<Function>> m_all_functions;

    HashMap<String, RefPtr<Module>> m_modules;

    Vector<RefPtr<Variable>> m_globals;

    Optional<bytecode::Register> m_self;
    Optional<bytecode::Register> m_return; // Used for constructor functions

    HashMap<Type*, OwnPtr<Impl>> m_impls;
    Vector<OwnPtr<Impl>> m_generic_impls;

    HashMap<Type*, RefPtr<Trait>> m_traits;
};

}