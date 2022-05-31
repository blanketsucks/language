#ifndef _VISITOR_H
#define _VISITOR_H

#include "ast.h"
#include "parser.h"
#include "llvm.h"
#include "types.hpp"

#include <map>
#include <string>
#include <memory>

struct Function {
    std::string name;
    std::vector<llvm::Type*> args;
    llvm::Type* return_type;
};

class Visitor {
public:
    llvm::LLVMContext context;
    std::unique_ptr<llvm::Module> module;
    std::unique_ptr<llvm::IRBuilder<>> builder;
    
    std::map<std::string, Function> functions;
    std::map<std::string, llvm::Value*> variables;

    Function current_function;

    Visitor(std::string name);

    void dump(llvm::raw_ostream& stream);
    llvm::Type* get_type(Type type);

    void visit(std::unique_ptr<ast::Program> program);

    llvm::Value* visit_IntegerExpr(ast::IntegerExpr* expr);
    llvm::Value* visit_VariableExpr(ast::VariableExpr* expr);
    llvm::Value* visit_ListExpr(ast::ListExpr* expr);
    llvm::Value* visit_BinaryOpExpr(ast::BinaryOpExpr* expr);
    llvm::Value* visit_CallExpr(ast::CallExpr* expr);
    llvm::Value* visit_ReturnExpr(ast::ReturnExpr* expr);
    llvm::Value* visit_PrototypeExpr(ast::PrototypeExpr* prototype);
    llvm::Value* visit_FunctionExpr(ast::FunctionExpr* function);
};


#endif