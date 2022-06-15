#ifndef _VISITOR_H
#define _VISITOR_H

#include "ast.h"
#include "parser.h"
#include "llvm.h"
#include "types.h"

#include <map>
#include <string>
#include <memory>

enum class ModuleState {
    Initialized,
    Compiled
};

struct Function {
    std::string name;
    std::vector<llvm::Type*> args;
    llvm::Type* ret;
    std::map<std::string, llvm::AllocaInst*> locals;
    std::map<std::string, llvm::Value*> constants;
    bool has_return;
    bool is_intrinsic;
    bool used;

    Function(std::string name, std::vector<llvm::Type*> args, llvm::Type* ret, bool is_intrinsic) {
        this->name = name;
        this->args = args;
        this->ret = ret;
        this->has_return = false;
        this->is_intrinsic = is_intrinsic;
        this->used = false;
    }
};

struct Struct {
    std::string name;
    llvm::StructType* type;
    std::map<std::string, llvm::Type*> fields;
    std::map<std::string, Function*> methods;
    std::map<std::string, llvm::Value*> locals;
    llvm::Function* constructor;

    Struct(std::string name, llvm::StructType* type, std::map<std::string, llvm::Type*> fields) {
        this->name = name;
        this->type = type;
        this->fields = fields;
        this->methods = {};
        this->locals = {};
    }
};

struct Module {
    std::string path;
    ModuleState state;

    bool is_ready() { return this->state == ModuleState::Compiled; }
};

struct Namespace {
    std::string name;
    std::map<std::string, Struct*> structs;
    std::map<std::string, Function*> functions;
    std::map<std::string, Namespace*> namespaces;

    Namespace(std::string name) {
        this->name = name;
        this->structs = {};
        this->functions = {};
        this->namespaces = {};
    }
};

class Visitor {
public:
    llvm::LLVMContext context;
    std::unique_ptr<llvm::Module> module;
    std::unique_ptr<llvm::IRBuilder<>> builder;
    std::unique_ptr<llvm::legacy::FunctionPassManager> fpm;

    std::map<std::string, Function*> functions;
    std::map<std::string, llvm::Constant*> constants;
    std::map<std::string, Struct*> structs;
    std::map<std::string, Module> includes;
    std::map<std::string, Namespace*> namespaces;

    Function* current_function = nullptr;
    Struct* current_struct = nullptr;
    Namespace* current_namespace = nullptr;
    
    Visitor(std::string name);

    void dump(llvm::raw_ostream& stream);
    std::string format_name(std::string name);
    std::pair<std::string, bool> is_intrinsic(std::string name);
    std::pair<llvm::Value*, bool> get_variable(std::string name);
    llvm::Function* get_function(std::string name);
    llvm::Type* get_llvm_type(Type* name);
    llvm::AllocaInst* create_alloca(llvm::Function* function, llvm::Type* type);
    llvm::Value* cast(llvm::Value* value, Type* type);
    llvm::Value* cast(llvm::Value* value, llvm::Type* type);
    llvm::Function* create_struct_constructor(Struct* structure, std::vector<llvm::Type*> types);

    void visit(std::unique_ptr<ast::Program> program);

    llvm::Value* visit(ast::BlockExpr* expr);
    llvm::Value* visit(ast::IntegerExpr* expr);
    llvm::Value* visit(ast::FloatExpr* expr);
    llvm::Value* visit(ast::StringExpr* expr);
    llvm::Value* visit(ast::VariableExpr* expr);
    llvm::Value* visit(ast::VariableAssignmentExpr* expr);
    llvm::Value* visit(ast::ConstExpr* expr);
    llvm::Value* visit(ast::ArrayExpr* expr);
    llvm::Value* visit(ast::UnaryOpExpr* expr);
    llvm::Value* visit(ast::BinaryOpExpr* expr);
    llvm::Value* visit(ast::CallExpr* expr);
    llvm::Value* visit(ast::ReturnExpr* expr);
    llvm::Value* visit(ast::PrototypeExpr* expr);
    llvm::Value* visit(ast::FunctionExpr* expr);
    llvm::Value* visit(ast::IfExpr* expr);
    llvm::Value* visit(ast::WhileExpr* expr);
    llvm::Value* visit(ast::StructExpr* expr);
    llvm::Value* visit(ast::AttributeExpr* expr);
    llvm::Value* visit(ast::ElementExpr* expr);
    llvm::Value* visit(ast::IncludeExpr* expr);
    llvm::Value* visit(ast::NamespaceExpr* expr);
    llvm::Value* visit(ast::NamespaceAttributeExpr* expr);

};


#endif