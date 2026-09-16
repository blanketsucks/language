#pragma once

#include <quart/bytecode/generator.h>
#include <quart/language/context.h>
#include <quart/language/scopes.h>
#include <quart/language/impl.h>
#include <quart/language/trait.h>
#include <quart/language/type_checker.h>
#include <quart/language/consteval.h>
#include <quart/enums.h>

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

enum class ReferenceAccess {
    None                       = 0,
    Member                     = 1 << 0,
    Mutable                    = 1 << 1,
    PreserveOriginalMutability = 1 << 2,
};

MAKE_ENUM_BITWISE_OPS(ReferenceAccess)

class State {
public:
    State();
    ~State() = default;

    NO_MOVE(State)
    NO_COPY(State)

    void dump() const;

    Vector<OwnPtr<bytecode::Instruction>> const& global_instructions() const { return m_generator.global_instructions(); }

    bytecode::BasicBlock* current_block() { return m_generator.current_block(); }
    size_t register_count() { return m_generator.register_count(); }

    bytecode::RegisterUse& register_uses(bytecode::Register reg) {
        return m_generator.register_uses(reg);
    }

    RefPtr<Scope> global_scope() const { return m_global_scope; }

    bytecode::Generator& generator() { return m_generator; }
    Context& context() { return *m_context; }

    Type* i1() const { return m_context->i1(); }
    Type* i32() const { return m_context->i32(); }

    TypeChecker& type_checker() { return m_type_checker; }
    ConstantEvaluator& constant_evaluator() { return m_constant_evaluator; }

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

    Optional<bytecode::Value*> self() const { return m_self; }
    Optional<bytecode::Value*> return_register() const { return m_return; }
    
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

    size_t allocate_global() { return m_global_count++; }

    void inject_self(bytecode::Value* value) { m_self = value; }
    void reset_self() { m_self = {}; }

    void inject_return(bytecode::Value* value) { m_return = value; }
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

    void add_trait(RefPtr<Trait> trait) { m_traits[trait->underlying_type()] = move(trait); }
    void add_trait(Type* type, RefPtr<Trait> trait) { m_traits[type] = move(trait); }

    RefPtr<Trait> get_trait(Type* type) const {
        auto iterator = m_traits.find(type);
        if (iterator != m_traits.end()) {
            return iterator->second;
        }

        return nullptr;
    }
    
    ErrorOr<bytecode::Value*> resolve_reference(ast::Expr const&, ReferenceAccess access, bool use_default_case = true);
    ErrorOr<bytecode::Value*> resolve_reference(Scope&, Span, const String& name, ReferenceAccess access);

    ErrorOr<Symbol*> resolve_symbol(ast::Expr const&);
    ErrorOr<Struct*> resolve_struct(ast::Expr const&);

    ErrorOr<bytecode::Value*> type_check_and_cast(Span, bytecode::Value*, Type* target, StringView error_message);

    ErrorOr<bytecode::Value*> generate_attribute_access(ast::AttributeExpr const&, ReferenceAccess access);
    ErrorOr<bytecode::Value*> generate_index_access(ast::IndexExpr const&, ReferenceAccess access);

    fs::Path search_import_paths(const String& name);

    Type* get_type_from_builtin(ast::BuiltinType);

    ErrorOr<size_t> size_of(ast::Expr const&);

    HashMap<String, Type*> get_struct_generic_impl_arguments(Struct* structure, TraitType* trait) const;

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

    Optional<bytecode::Value*> m_self;
    Optional<bytecode::Value*> m_return; // Used for constructor functions

    HashMap<Type*, OwnPtr<Impl>> m_impls;
    Vector<OwnPtr<Impl>> m_generic_impls;

    HashMap<Type*, RefPtr<Trait>> m_traits;
};

}