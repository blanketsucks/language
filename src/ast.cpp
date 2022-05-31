#include "ast.h"

#include "visitor.h"

using namespace ast;

IntegerExpr::IntegerExpr(int value) : value(value) {}

llvm::Value* IntegerExpr::accept(Visitor& visitor) {
    return visitor.visit_IntegerExpr(this);
}

VariableExpr::VariableExpr(std::string name) : name(name) {}

llvm::Value* VariableExpr::accept(Visitor& visitor) {
    return visitor.visit_VariableExpr(this);
}

ListExpr::ListExpr(std::vector<std::unique_ptr<Expr>> elements) {
    this->elements = std::move(elements);
}

llvm::Value* ListExpr::accept(Visitor& visitor) {
    return visitor.visit_ListExpr(this);
}

BinaryOpExpr::BinaryOpExpr(TokenType op, std::unique_ptr<Expr> left, std::unique_ptr<Expr> right) : op(op) {
    this->left = std::move(left);
    this->right = std::move(right);
}

llvm::Value* BinaryOpExpr::accept(Visitor& visitor) {
    return visitor.visit_BinaryOpExpr(this);
}

CallExpr::CallExpr(std::string callee, std::vector<std::unique_ptr<Expr>> args) : callee(callee) {
    this->args = std::move(args);
}

llvm::Value* CallExpr::accept(Visitor& visitor) {
    return visitor.visit_CallExpr(this);
}

ReturnExpr::ReturnExpr(std::unique_ptr<Expr> value) {
    this->value = std::move(value);
}

llvm::Value* ReturnExpr::accept(Visitor& visitor) {
    return visitor.visit_ReturnExpr(this);
}

PrototypeExpr::PrototypeExpr(std::string name, Type return_type, std::vector<Argument> args) : name(name), return_type(return_type) {
    this->args = std::move(args);
}

llvm::Value* PrototypeExpr::accept(Visitor& visitor) {
    return visitor.visit_PrototypeExpr(this);
}

FunctionExpr::FunctionExpr(std::unique_ptr<PrototypeExpr> prototype, std::vector<std::unique_ptr<Expr>> body) {
    this->prototype = std::move(prototype);
    this->body = std::move(body);
}

llvm::Value* FunctionExpr::accept(Visitor& visitor) {
    return visitor.visit_FunctionExpr(this);
}

Program::Program(std::vector<std::unique_ptr<Expr>> ast) {
    this->ast = std::move(ast);
}