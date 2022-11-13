#ifndef _VISITOR_H
#define _VISITOR_H

#include "types/include.h"
#include "parser/ast.h"
#include "parser/parser.h"
#include "llvm.h"
#include "objects.h"
#include "mangler.h"
#include "utils.h"

#include "llvm/ADT/Optional.h"
#include "llvm/IR/DerivedTypes.h"

#include <functional>
#include <map>
#include <stdint.h>
#include <string>
#include <memory>

#ifdef __GNUC__
    #define _UNREACHABLE __builtin_unreachable();
#elif _MSC_VER
    #define _UNREACHABLE __assume(false);
#else
    #define _UNREACHABLE
#endif

// Key value struct for tuples
struct TupleKey {
    std::vector<llvm::Type*> types;

    static TupleKey create(std::vector<llvm::Type*> types) {
        return {types};
    }

    bool operator==(const TupleKey& other) const {
        if (this->types.size() != other.types.size()) {
            return false;
        }
        
        for (size_t i = 0; i < this->types.size(); i++) {
            if (this->types[i] != other.types[i]) {
                return false;
            }
        }

        return true;
    }
};


class Visitor {
public:
    Visitor(std::string name, std::string entry, bool with_optimizations = true);

    void free();

    void dump(llvm::raw_ostream& stream);

    void set_insert_point(llvm::BasicBlock* block, bool push = true);

    Scope* create_scope(std::string name, ScopeType type);

    std::string format_name(std::string name);
    std::pair<std::string, bool> format_intrinsic_function(std::string name);
    
    std::pair<llvm::Value*, bool> get_variable(std::string name);
    Function* get_function(std::string name);

    llvm::Value* cast(llvm::Value* value, Type* type);
    llvm::Value* cast(llvm::Value* value, llvm::Type* type);

    llvm::Value* load(llvm::Value* value, llvm::Type* type = nullptr);

    uint32_t getallocsize(llvm::Type* type);
    uint32_t getsizeof(llvm::Value* value);
    uint32_t getsizeof(llvm::Type* type);

    std::vector<llvm::Value*> unpack(
        llvm::Value* value, uint32_t n, Location location = Location::dummy()
    );

    void store_struct_field(ast::AttributeExpr* expr, utils::Ref<ast::Expr> value);
    void store_array_element(ast::ElementExpr* expr, utils::Ref<ast::Expr> value);
    void store_tuple(Location location, utils::Shared<Function> func, llvm::Value* value, std::vector<std::string> names);

    llvm::Type* get_llvm_type(Type* name);
    Type* from_llvm_type(llvm::Type* type);

    llvm::AllocaInst* create_alloca(llvm::Type* type);

    llvm::Function* create_function(
        std::string name, 
        llvm::Type* ret,
        std::vector<llvm::Type*> args, 
        bool is_variadic, 
        llvm::Function::LinkageTypes linkage
    );

    std::vector<llvm::Value*> typecheck_function_call(
        llvm::Function* function,
        std::vector<llvm::Value*>& args,
        uint32_t start = 0, 
        Location location = Location::dummy()
    );

    llvm::Value* call(
        llvm::Function* function, 
        std::vector<llvm::Value*> args, 
        bool is_constructor = false,
        llvm::Value* self = nullptr, 
        llvm::FunctionType* type = nullptr,
        Location location = Location::dummy()
    );

    void create_global_initializers();

    static std::string get_type_name(llvm::Type* type);
    bool is_compatible(llvm::Type* t1, llvm::Type* t2);

    Scope::Local get_pointer_from_expr(ast::Expr* expr);

    void visit(std::vector<std::unique_ptr<ast::Expr>> statements);

    Value visit(ast::BlockExpr* expr);

    Value visit(ast::IntegerExpr* expr);
    Value visit(ast::FloatExpr* expr);
    Value visit(ast::StringExpr* expr);

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

    Value visit(ast::StructExpr* expr);
    Value visit(ast::ConstructorExpr* expr);
    Value visit(ast::AttributeExpr* expr);

    Value visit(ast::ArrayExpr* expr);
    Value visit(ast::ElementExpr* expr);

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

    std::map<TupleKey, llvm::StructType*, std::equal_to<TupleKey>> tuples;
    std::map<uint32_t, llvm::Type*> typeids;

    std::vector<FunctionCall*> initializers;

    Scope* global_scope;
    Scope* scope;

    utils::Shared<Function> current_function = nullptr;
    utils::Shared<Struct> current_struct = nullptr;
    utils::Shared<Namespace> current_namespace = nullptr;
    utils::Shared<Module> current_module = nullptr;

    llvm::Type* ctx;
};

#endif