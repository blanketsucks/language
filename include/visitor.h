#ifndef _VISITOR_H
#define _VISITOR_H

#include "types/include.h"
#include "parser/ast.h"
#include "parser/parser.h"
#include "llvm.h"
#include "objects.h"

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

class Visitor {
public:
    std::string name;
    std::string entry;
    bool with_optimizations;

    llvm::LLVMContext context;
    utils::Ref<llvm::Module> module;
    utils::Ref<llvm::IRBuilder<>> builder;
    std::unique_ptr<llvm::legacy::FunctionPassManager> fpm;

    std::map<std::string, Function*> functions;
    std::map<std::string, llvm::Constant*> constants;
    std::map<std::string, Struct*> structs;
    std::map<std::string, Namespace*> namespaces;

    std::map<uint32_t, llvm::StructType*> tuples; 

    Function* current_function = nullptr;
    Struct* current_struct = nullptr;
    Namespace* current_namespace = nullptr;
    
    std::vector<Type*> allocated_types;

    llvm::Type* ctx;

    Visitor(std::string name, std::string entry, bool with_optimizations = true);

    void cleanup();
    void free();

    void dump(llvm::raw_ostream& stream);

    std::string format_name(std::string name);

    std::pair<std::string, bool> is_intrinsic(std::string name);
    
    std::pair<llvm::Value*, bool> get_variable(std::string name);
    Value get_function(std::string name);

    void store_function(std::string name, Function* function);
    void store_struct(std::string name, Struct* structure);
    void store_namespace(std::string name, Namespace* ns);

    llvm::Value* cast(llvm::Value* value, Type* type);
    llvm::Value* cast(llvm::Value* value, llvm::Type* type);

    llvm::Value* load(llvm::Type* type, llvm::Value* value);
    llvm::Value* load(llvm::Value* value);

    std::vector<llvm::Value*> unpack(Location location, llvm::Value* value, uint32_t n);

    uint32_t getallocsize(llvm::Type* type);
    uint32_t getsizeof(llvm::Value* value);
    uint32_t getsizeof(llvm::Type* type);

    std::pair<llvm::Value*, int> get_struct_field(ast::AttributeExpr* expr);
    std::pair<llvm::Value*, int> get_array_element(ast::ElementExpr* expr);

    llvm::Type* get_llvm_type(Type* name);
    Type* from_llvm_type(llvm::Type* type);

    llvm::Constant* create_constant(std::string name, llvm::Constant* value);
    llvm::AllocaInst* create_alloca(llvm::Function* function, llvm::Type* type);
    llvm::Function* create_function(std::string name, llvm::Type* ret, std::vector<llvm::Type*> args, bool has_varargs, llvm::Function::LinkageTypes linkage);

    void store_tuple(Location location, Function* func, llvm::Value* value, std::vector<std::string> names);

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
};


#endif