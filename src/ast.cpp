#include "ast.h"

#include "visitor.h"

using namespace ast;

BlockExpr::BlockExpr(Location* start, Location* end, std::vector<std::unique_ptr<Expr>> block) : Expr(start, end) {
    this->block = std::move(block);
}

llvm::Value* BlockExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

IntegerExpr::IntegerExpr(Location* start, Location* end, int value) : Expr(start, end), value(value) {}

llvm::Value* IntegerExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

FloatExpr::FloatExpr(Location* start, Location* end, float value) : Expr(start, end), value(value) {}

llvm::Value* FloatExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

StringExpr::StringExpr(Location* start, Location* end, std::string value) : Expr(start, end), value(value) {}

llvm::Value* StringExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

VariableExpr::VariableExpr(Location* start, Location* end, std::string name) : Expr(start, end), name(name) {}

llvm::Value* VariableExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

VariableAssignmentExpr::VariableAssignmentExpr(
    Location* start, Location* end, std::string name, Type* type, std::unique_ptr<Expr> value
) : Expr(start, end), name(name), type(type) {
    this->value = std::move(value);
}

llvm::Value* VariableAssignmentExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

ConstExpr::ConstExpr(
    Location* start, Location* end, std::string name, Type* type, std::unique_ptr<Expr> value
) : VariableAssignmentExpr(start, end, name, type, std::move(value)) {}

llvm::Value* ConstExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

ArrayExpr::ArrayExpr(Location* start, Location* end, std::vector<std::unique_ptr<Expr>> elements) : Expr(start, end) {
    this->elements = std::move(elements);
}

llvm::Value* ArrayExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

UnaryOpExpr::UnaryOpExpr(
    Location* start, Location* end, TokenType op, std::unique_ptr<Expr> value
) : Expr(start, end), op(op) {
    this->value = std::move(value);
}

llvm::Value* UnaryOpExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

BinaryOpExpr::BinaryOpExpr(
    Location* start, Location* end, TokenType op, std::unique_ptr<Expr> left, std::unique_ptr<Expr> right
) : Expr(start, end), op(op) {
    this->left = std::move(left);
    this->right = std::move(right);
}

llvm::Value* BinaryOpExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

CallExpr::CallExpr(
    Location* start, Location* end, std::unique_ptr<ast::Expr> callee, std::vector<std::unique_ptr<Expr>> args
) : Expr(start, end) {
    this->callee = std::move(callee);
    this->args = std::move(args);
}

llvm::Value* CallExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

ReturnExpr::ReturnExpr(Location* start, Location* end, std::unique_ptr<Expr> value) : Expr(start, end) {
    this->value = std::move(value);
}

llvm::Value* ReturnExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

PrototypeExpr::PrototypeExpr(
    Location* start, Location* end, std::string name, Type* return_type, std::vector<Argument> args, bool has_varargs
) : Expr(start, end), name(name), return_type(return_type), has_varargs(has_varargs) {
    this->args = std::move(args);
}

llvm::Value* PrototypeExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

FunctionExpr::FunctionExpr(
    Location* start, Location* end, std::unique_ptr<PrototypeExpr> prototype, std::unique_ptr<BlockExpr> body
) : Expr(start, end) {
    this->prototype = std::move(prototype);
    this->body = std::move(body);
}

llvm::Value* FunctionExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

IfExpr::IfExpr(
    Location* start, Location* end, std::unique_ptr<Expr> condition, std::unique_ptr<BlockExpr> body, std::unique_ptr<BlockExpr> ebody
) : Expr(start, end) {
    this->condition = std::move(condition);
    this->body = std::move(body);
    this->ebody = std::move(ebody);
}

llvm::Value* IfExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

WhileExpr::WhileExpr(Location* start, Location* end, std::unique_ptr<Expr> condition, std::unique_ptr<BlockExpr> body) : Expr(start, end) {
    this->condition = std::move(condition);
    this->body = std::move(body);
}

llvm::Value* WhileExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

StructExpr::StructExpr(
    Location* start, Location* end, std::string name, bool packed, std::map<std::string, Argument> fields, std::vector<std::unique_ptr<FunctionExpr>> methods
) : Expr(start, end), name(name), packed(packed) {
    this->fields = std::move(fields);
    this->methods = std::move(methods);
}

llvm::Value* StructExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

AttributeExpr::AttributeExpr(
    Location* start, Location* end, std::string attribute, std::unique_ptr<Expr> parent
) : Expr(start, end), attribute(attribute) {
    this->parent = std::move(parent);
}

llvm::Value* AttributeExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

ElementExpr::ElementExpr(
    Location* start, Location* end, std::unique_ptr<Expr> value, std::unique_ptr<Expr> index
) : Expr(start, end) {
    this->value = std::move(value);
    this->index = std::move(index);
}

llvm::Value* ElementExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

IncludeExpr::IncludeExpr(Location* start, Location* end, std::string path) : Expr(start, end), path(path) {}

llvm::Value* IncludeExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

NamespaceExpr::NamespaceExpr(
    Location* start, Location* end, std::string name, std::vector<std::unique_ptr<FunctionExpr>> functions, std::vector<std::unique_ptr<StructExpr>> structs, std::vector<std::unique_ptr<ConstExpr>> consts, std::vector<std::unique_ptr<NamespaceExpr>> namespaces
) : Expr(start, end), name(name) {
    this->functions = std::move(functions);
    this->structs = std::move(structs);
    this->consts = std::move(consts);
    this->namespaces = std::move(namespaces);
}

llvm::Value* NamespaceExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

NamespaceAttributeExpr::NamespaceAttributeExpr(
    Location* start, Location* end, std::string attribute, std::unique_ptr<Expr> parent
) : Expr(start, end), attribute(attribute) {
    this->parent = std::move(parent);
}

llvm::Value* NamespaceAttributeExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

Program::Program(std::vector<std::unique_ptr<Expr>> ast) {
    this->ast = std::move(ast);
}