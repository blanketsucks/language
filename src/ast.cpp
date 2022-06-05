#include "ast.h"

#include "visitor.h"

using namespace ast;

IntegerExpr::IntegerExpr(int value) : value(value) {}

llvm::Value* IntegerExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

StringExpr::StringExpr(std::string value) : value(value) {}

llvm::Value* StringExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

VariableExpr::VariableExpr(std::string name) : name(name) {}

llvm::Value* VariableExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

VariableAssignmentExpr::VariableAssignmentExpr(std::string name, Type type, std::unique_ptr<Expr> value) : name(name), type(type) {
    this->value = std::move(value);
}

llvm::Value* VariableAssignmentExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

ArrayExpr::ArrayExpr(std::vector<std::unique_ptr<Expr>> elements) {
    this->elements = std::move(elements);
}

llvm::Value* ArrayExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

UnaryOpExpr::UnaryOpExpr(TokenType op, std::unique_ptr<Expr> value) : op(op) {
    this->value = std::move(value);
}

llvm::Value* UnaryOpExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

BinaryOpExpr::BinaryOpExpr(TokenType op, std::unique_ptr<Expr> left, std::unique_ptr<Expr> right) : op(op) {
    this->left = std::move(left);
    this->right = std::move(right);
}

llvm::Value* BinaryOpExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

CallExpr::CallExpr(std::string callee, std::vector<std::unique_ptr<Expr>> args) : callee(callee) {
    this->args = std::move(args);
}

llvm::Value* CallExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

ReturnExpr::ReturnExpr(std::unique_ptr<Expr> value) {
    this->value = std::move(value);
}

llvm::Value* ReturnExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

PrototypeExpr::PrototypeExpr(std::string name, Type return_type, std::vector<Argument> args) : name(name), return_type(return_type) {
    this->args = std::move(args);
}

llvm::Value* PrototypeExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

FunctionExpr::FunctionExpr(std::unique_ptr<PrototypeExpr> prototype, std::vector<std::unique_ptr<Expr>> body) {
    this->prototype = std::move(prototype);
    this->body = std::move(body);
}

llvm::Value* FunctionExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

IfExpr::IfExpr(std::unique_ptr<Expr> condition, std::vector<std::unique_ptr<Expr>> body, std::vector<std::unique_ptr<Expr>> ebody) {
    this->condition = std::move(condition);
    this->body = std::move(body);
    this->ebody = std::move(ebody);
}

llvm::Value* IfExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

StructExpr::StructExpr(std::string name, bool packed, std::vector<Argument> fields) : name(name), packed(packed) {
    this->fields = std::move(fields);
}

llvm::Value* StructExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

Program::Program(std::vector<std::unique_ptr<Expr>> ast) {
    this->ast = std::move(ast);
}