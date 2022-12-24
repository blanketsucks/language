#ifndef _VISITOR_H
#define _VISITOR_H

#include "parser/parser.h"
#include "utils/pointer.h"
#include "utils/log.h"
#include "parser/ast.h"
#include "objects/scopes.h"
#include "objects/functions.h"
#include "objects/namespaces.h"
#include "objects/modules.h"
#include "objects/structs.h"
#include "objects/values.h"
#include "mangler.h"
#include "llvm.h"

#include <functional>
#include <map>
#include <stdint.h>
#include <string>
#include <unordered_map>
#include <memory>

#ifdef __GNUC__
    #define _UNREACHABLE __builtin_unreachable();
#elif _MSC_VER
    #undef alloca
    #define _UNREACHABLE __assume(false);
#else
    #define _UNREACHABLE
#endif

#define FILE_EXTENSION ".qr"

class Visitor {
public:
    using TupleKey = std::vector<llvm::Type*>;
    using Finalizer = std::function<void(Visitor&)>;

    Visitor(std::string name, std::string entry, bool with_optimizations = true);
    void finalize();

    void add_finalizer(Finalizer finalizer);

    void dump(llvm::raw_ostream& stream);

    void set_insert_point(llvm::BasicBlock* block, bool push = true);

    Scope* create_scope(std::string name, ScopeType type);

    std::string format_name(std::string name);
    std::pair<std::string, bool> format_intrinsic_function(std::string name);
    
    std::pair<llvm::Value*, bool> get_variable(std::string name);
    Function* get_function(std::string name);

    llvm::Value* cast(llvm::Value* value, Type type);
    llvm::Value* cast(llvm::Value* value, llvm::Type* type);

    llvm::Value* load(llvm::Value* value, llvm::Type* type = nullptr);

    uint32_t getallocsize(llvm::Type* type);
    uint32_t getsizeof(llvm::Value* value);
    uint32_t getsizeof(llvm::Type* type);

    llvm::StructType* create_tuple_type(std::vector<llvm::Type*> types);
    void store_tuple(Location location, utils::Shared<Function> func, llvm::Value* value, std::vector<std::string> names, std::string consume_rest);
    llvm::Value* make_tuple(std::vector<llvm::Value*> values, llvm::StructType* type = nullptr);
    std::vector<llvm::Value*> unpack(
        llvm::Value* value, uint32_t n, Location location = Location::dummy()
    );

    void store_struct_field(ast::AttributeExpr* expr, utils::Ref<ast::Expr> value);

    void store_array_element(ast::ElementExpr* expr, utils::Ref<ast::Expr> value);
    void bounds_check(llvm::Value* index, uint32_t size);

    bool is_struct(llvm::Value* value);
    bool is_struct(llvm::Type* type);
    bool is_tuple(llvm::Type* type);

    utils::Shared<Struct> get_struct(llvm::Value* value);
    utils::Shared<Struct> get_struct(llvm::Type* type);

    llvm::AllocaInst* create_alloca(llvm::Type* type);

    bool is_reserved_function(std::string name);
    llvm::Function* create_function(
        std::string name, 
        llvm::Type* ret,
        std::vector<llvm::Type*> args, 
        bool is_variadic, 
        llvm::Function::LinkageTypes linkage
    );

    std::vector<llvm::Value*> handle_function_arguments(
        Location location,
        utils::Shared<Function> function,
        llvm::Value* self,
        std::vector<utils::Ref<ast::Expr>> args,
        std::map<std::string, utils::Ref<ast::Expr>> kwargs
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
    
    static std::string get_type_name(Type type);
    static std::string get_type_name(llvm::Type* type);

    bool is_compatible(Type t1, llvm::Type* t2);
    bool is_compatible(llvm::Type* t1, llvm::Type* t2);

    ScopeLocal as_reference(ast::Expr* expr);

    uint32_t get_pointer_depth(llvm::Type* type);

    void visit(std::vector<std::unique_ptr<ast::Expr>> statements);

    Value visit(ast::BlockExpr* expr);

    Value visit(ast::IntegerExpr* expr);
    Value visit(ast::CharExpr* expr);
    Value visit(ast::FloatExpr* expr);
    Value visit(ast::StringExpr* expr);
    Value visit(ast::StaticAssertExpr* expr);

    Value visit(ast::MaybeExpr* expr);

    Value visit(ast::VariableExpr* expr);
    Value visit(ast::VariableAssignmentExpr* expr);
    Value visit(ast::ConstExpr* expr);

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
    Value visit(ast::BreakExpr* expr);
    Value visit(ast::ContinueExpr* expr);
    Value visit(ast::ForeachExpr* expr);

    Value visit(ast::StructExpr* expr);
    Value visit(ast::ConstructorExpr* expr);
    Value visit(ast::AttributeExpr* expr);

    Value visit(ast::ArrayExpr* expr);
    Value visit(ast::ArrayFillExpr* expr);
    Value visit(ast::ElementExpr* expr);

    Value visit(ast::BuiltinTypeExpr* expr);
    Value visit(ast::IntegerTypeExpr* expr);
    Value visit(ast::NamedTypeExpr* expr);
    Value visit(ast::TupleTypeExpr* expr);
    Value visit(ast::ArrayTypeExpr* expr);
    Value visit(ast::PointerTypeExpr* expr);
    Value visit(ast::FunctionTypeExpr* expr);
    Value visit(ast::ReferenceTypeExpr* expr);
    Value visit(ast::TypeAliasExpr* expr);

    Value visit(ast::CastExpr* expr);
    Value visit(ast::SizeofExpr* expr);
    Value visit(ast::OffsetofExpr* expr);

    Value visit(ast::NamespaceExpr* expr);
    Value visit(ast::NamespaceAttributeExpr* expr);
    Value visit(ast::UsingExpr* expr);

    Value visit(ast::TupleExpr* expr);

    Value visit(ast::EnumExpr* expr);

    Value visit(ast::WhereExpr* expr);

    Value visit(ast::ImportExpr* expr);

    std::string name;
    std::string entry;
    bool with_optimizations;

    uint64_t id = 0;
    
    utils::Ref<llvm::LLVMContext> context;
    utils::Ref<llvm::Module> module;
    utils::Ref<llvm::IRBuilder<>> builder;
    utils::Ref<llvm::legacy::FunctionPassManager> fpm;

    std::map<std::string, utils::Shared<Struct>> structs;
    std::map<std::string, utils::Shared<Module>> modules;
    std::map<TupleKey, llvm::StructType*> tuples;

    std::vector<FunctionCall> constructors;

    Scope* global_scope = nullptr;
    Scope* scope = nullptr;

    utils::Shared<Function> current_function = nullptr;
    utils::Shared<Struct> current_struct = nullptr;
    utils::Shared<Namespace> current_namespace = nullptr;
    utils::Shared<Module> current_module = nullptr;

    llvm::Type* ctx = nullptr;

    std::vector<Finalizer> finalizers;
};

#endif