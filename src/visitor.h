#ifndef _VISITOR_H
#define _VISITOR_H

#include "ast.h"
#include "parser.h"
#include "llvm.h"
#include "types.h"

#include <map>
#include <string>
#include <memory>

struct Function {
    std::string name;
    std::vector<llvm::Type*> args;
    llvm::Type* ret;
    std::map<std::string, llvm::AllocaInst*> locals;

    Function(std::string name, std::vector<llvm::Type*> args, llvm::Type* ret) {
        this->name = name;
        this->args = args;
        this->ret = ret;
    }
};

class Visitor {
public:
    llvm::LLVMContext context;
    std::unique_ptr<llvm::Module> module;
    std::unique_ptr<llvm::IRBuilder<>> builder;

    std::map<std::string, Function*> functions;
    std::map<std::string, llvm::Constant*> constants;
    std::map<std::string, llvm::AllocaInst*> globals;

    Function* current_function = nullptr;

    Visitor(std::string name);

    void dump(llvm::raw_ostream& stream);
    llvm::Type* get_type(Type type);
    llvm::Value* get_variable(std::string name);
    llvm::AllocaInst* create_alloca(llvm::Function* function, llvm::Type* type, std::string name);

    void visit(std::unique_ptr<ast::Program> program);
    llvm::Value* visit(ast::IntegerExpr* expr);
    llvm::Value* visit(ast::StringExpr* expr);
    llvm::Value* visit(ast::VariableExpr* expr);
    llvm::Value* visit(ast::VariableAssignmentExpr* expr);
    llvm::Value* visit(ast::ArrayExpr* expr);
    llvm::Value* visit(ast::BinaryOpExpr* expr);
    llvm::Value* visit(ast::CallExpr* expr);
    llvm::Value* visit(ast::ReturnExpr* expr);
    llvm::Value* visit(ast::PrototypeExpr* expr);
    llvm::Value* visit(ast::FunctionExpr* expr);
    llvm::Value* visit(ast::IfExpr* expr);
};


#endif