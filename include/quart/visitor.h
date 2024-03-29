#pragma once

#include <quart/lexer/location.h>

#include <quart/language/scopes.h>
#include <quart/language/registry.h>
#include <quart/language/values.h>

#include <quart/parser.h>
#include <quart/llvm.h>
#include <quart/builtins.h>
#include <quart/compiler.h>
#include <quart/debug.h>
#include <quart/common.h>

#include <functional>
#include <map>
#include <stdint.h>
#include <string>
#include <unordered_map>
#include <memory>

#ifdef alloca
    #undef alloca
#endif

namespace quart {

struct Impl {
    std::string name;
    quart::Type* type;

    Scope* scope;
};

class Visitor {
public:
    using Finalizer = std::function<void(Visitor&)>;

    Visitor(const std::string& name, CompilerOptions& options);

    void finalize();
    void add_finalizer(Finalizer finalizer);

    void dump(llvm::raw_ostream& stream);

    void set_insert_point(llvm::BasicBlock* block, bool push = true);

    void push_scope(Scope* scope);
    Scope* pop_scope();

    quart::Scope* create_scope(const std::string& name, quart::ScopeType type);

    std::string format_symbol(const std::string& name);

    llvm::Constant* to_str(const char* str);
    llvm::Constant* to_str(llvm::StringRef str);
    llvm::Constant* to_str(const std::string& str);

    llvm::Constant* to_int(u64 value, u32 bits = 32);

    llvm::Constant* to_float(double value);

    // Returns the `merge` block that should be inserted after doing stuff in the `then` block
    llvm::BasicBlock* create_if_statement(llvm::Value* condition);

    Value cast(const Value& value, quart::Type* to);

    llvm::Value* load(llvm::Value* value, llvm::Type* type = nullptr);

    u32 getallocsize(llvm::Type* type);
    u32 getsizeof(quart::Type* type);
    u32 getsizeof(llvm::Value* value);
    u32 getsizeof(llvm::Type* type);

    quart::Type* get_builtin_type(ast::BuiltinType type);

    void store_tuple(
        const Span& span, 
        Function& function,
        const Value& value, 
        const std::vector<ast::Ident>& identifiers, 
        const std::string& consume_rest
    );

    llvm::Value* make_tuple(std::vector<llvm::Value*> values, llvm::StructType* type);
    std::vector<Value> unpack(const Value& value, u32 n, const Span& span);

    RefPtr<Struct> make_struct(const std::string& name, const std::map<std::string, quart::Type*>& fields);
    Value create_struct_value(
        Struct& structure, const std::vector<llvm::Value*>& args
    );

    void create_bounds_check(llvm::Value* index, u32 count, const Span& span);

    RefPtr<Struct> get_struct_from_type(quart::Type* type);

    llvm::AllocaInst* alloca(llvm::Type* type);

    bool is_reserved_function(const std::string& name);
    Parameter parse_function_parameter(ast::Argument& argument, u32 index);
    llvm::Function* create_llvm_function(
        const std::string& name, 
        llvm::Type* ret,
        const std::vector<llvm::Type*>& args, 
        bool is_variadic, 
        llvm::Function::LinkageTypes linkage
    );

    std::vector<llvm::Value*> handle_function_arguments(
        const Span& span,
        Function const& function,
        llvm::Value* self,
        std::vector<OwnPtr<ast::Expr>>& args,
        std::map<std::string, OwnPtr<ast::Expr>>& kwargs
    );
    
    Value call(
        Function const& function, 
        std::vector<llvm::Value*> args, 
        llvm::Value* self = nullptr,
        bool is_constructor = false,
        llvm::FunctionType* type = nullptr
    );

    llvm::Value* call(
        llvm::Function* function, 
        std::vector<llvm::Value*> args, 
        llvm::Value* self = nullptr,
        bool is_constructor = false,
        llvm::FunctionType* type = nullptr
    );

    void create_global_constructors(
        llvm::Function::LinkageTypes linkage = llvm::Function::LinkageTypes::InternalLinkage
    );

    llvm::Value* as_reference(llvm::Value* value);
    ScopeLocal as_reference(ast::Expr& expr, bool require_ampersand = false);

    void panic(const std::string& message, const Span&);

    RefPtr<Module> import(const std::string& name, bool is_relative, const Span&);

    void mark_as_mutated(const std::string& name);
    void mark_as_mutated(const ScopeLocal& local);

    void visit(ast::ExprList<ast::Expr> statements);

    Value evaluate_tuple_assignment(ast::BinaryOpExpr* expr);
    Value evaluate_attribute_assignment(ast::AttributeExpr* expr, ast::Expr& value);
    Value evaluate_subscript_assignment(ast::IndexExpr* expr, ast::Expr& value);
    Value evaluate_assignment(ast::BinaryOpExpr* expr);

    Value evaluate_float_operation(const Value& lhs, BinaryOp op, const Value& rhs);
    Value evaluate_binary_operation(const Value& lhs, BinaryOp op, const Value& rhs);

    void evaluate_current_scope_defers();

    Value visit(ast::BlockExpr* expr);
    Value visit(ast::ExternBlockExpr* expr);

    Value visit(ast::IntegerExpr* expr);
    Value visit(ast::CharExpr* expr);
    Value visit(ast::FloatExpr* expr);
    Value visit(ast::StringExpr* expr);
    Value visit(ast::StaticAssertExpr* expr);

    Value visit(ast::MaybeExpr* expr);

    Value visit(ast::VariableExpr* expr);
    Value visit(ast::VariableAssignmentExpr* expr);
    Value visit(ast::ConstExpr* expr);

    Value visit(ast::ReferenceExpr* expr);
    Value visit(ast::UnaryOpExpr* expr);
    Value visit(ast::BinaryOpExpr* expr);
    Value visit(ast::InplaceBinaryOpExpr* expr);

    Value visit(ast::PrototypeExpr* expr);
    Value visit(ast::FunctionExpr* expr);
    Value visit(ast::ReturnExpr* expr);
    Value visit(ast::DeferExpr* expr);
    Value visit(ast::CallExpr* expr);

    Value visit(ast::IfExpr* expr);
    Value visit(ast::TernaryExpr* expr);

    Value visit(ast::WhileExpr* expr);
    Value visit(ast::ForExpr* expr);
    Value visit(ast::RangeForExpr* expr);
    Value visit(ast::BreakExpr* expr);
    Value visit(ast::ContinueExpr* expr);

    Value visit(ast::StructExpr* expr);
    Value visit(ast::ConstructorExpr* expr);
    Value visit(ast::EmptyConstructorExpr* expr);
    Value visit(ast::AttributeExpr* expr);

    Value visit(ast::ArrayExpr* expr);
    Value visit(ast::ArrayFillExpr* expr);
    Value visit(ast::IndexExpr* expr);

    quart::Type* visit(ast::BuiltinTypeExpr* expr);
    quart::Type* visit(ast::IntegerTypeExpr* expr);
    quart::Type* visit(ast::NamedTypeExpr* expr);
    quart::Type* visit(ast::TupleTypeExpr* expr);
    quart::Type* visit(ast::ArrayTypeExpr* expr);
    quart::Type* visit(ast::PointerTypeExpr* expr);
    quart::Type* visit(ast::FunctionTypeExpr* expr);
    quart::Type* visit(ast::ReferenceTypeExpr* expr);
    quart::Type* visit(ast::GenericTypeExpr* expr);
    Value visit(ast::TypeAliasExpr* expr);

    Value visit(ast::CastExpr* expr);
    Value visit(ast::SizeofExpr* expr);
    Value visit(ast::OffsetofExpr* expr);

    Value visit(ast::PathExpr* expr);
    Value visit(ast::UsingExpr* expr);

    Value visit(ast::TupleExpr* expr);

    Value visit(ast::EnumExpr* expr);
    Value visit(ast::MatchExpr* expr);

    Value visit(ast::ImportExpr* expr);
    Value visit(ast::ModuleExpr* expr);

    Value visit(ast::ImplExpr* expr);

    std::string name;
    CompilerOptions& options;

    u64 id = 0;
    
    OwnPtr<llvm::LLVMContext> context;
    OwnPtr<llvm::Module> module;
    OwnPtr<llvm::IRBuilder<>> builder;
    OwnPtr<llvm::DIBuilder> dbuilder;
    OwnPtr<llvm::legacy::FunctionPassManager> fpm;

    OwnPtr<quart::TypeRegistry> registry;

    DebugInfo debug;

    std::map<quart::Type*, RefPtr<Struct>> structs;
    std::map<std::string, RefPtr<Module>> modules;
    std::map<std::string, RefPtr<Function>> functions;

    std::map<llvm::Type*, llvm::StructType*> variadics;
    std::map<quart::Type*, Impl> impls;

    std::vector<EarlyFunctionCall> early_function_calls;

    Scope* global_scope = nullptr;
    Scope* scope = nullptr;

    RefPtr<Function> current_function = nullptr;
    RefPtr<Struct> current_struct = nullptr;
    RefPtr<Module> current_module = nullptr;
    Impl* current_impl = nullptr;

    std::map<std::string, BuiltinFunction> builtins;

    quart::Type* inferred = nullptr;
    quart::Type* self = nullptr;

    std::vector<Finalizer> finalizers;

    bool link_panic = false;
};

}