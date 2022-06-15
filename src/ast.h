#ifndef _AST_H
#define _AST_H

#include "tokens.hpp"
#include "types.h"
#include "llvm.h"

#include <iostream>
#include <memory>
#include <vector>

class Visitor;

namespace ast {

struct Argument {
    std::string name;
    Type* type;
};


class Expr {
public:
    Location* start;
    Location* end;

    Expr(Location* start, Location* end) : start(start), end(end) {}

    virtual ~Expr() = default;
    virtual llvm::Value* accept(Visitor& visitor) = 0;
};

class BlockExpr : public Expr {
public:
    std::vector<std::unique_ptr<Expr>> block;

    BlockExpr(Location* start, Location* end, std::vector<std::unique_ptr<Expr>> block);
    llvm::Value* accept(Visitor& visitor) override;
};

class IntegerExpr : public Expr {
public:
    int value;

    IntegerExpr(Location* start, Location* end, int value);
    llvm::Value* accept(Visitor& visitor) override;
};

class FloatExpr : public Expr {
public:
    float value;

    FloatExpr(Location* start, Location* end, float value);
    llvm::Value* accept(Visitor& visitor) override;
};

class StringExpr : public Expr {
public:
    std::string value;

    StringExpr(Location* start, Location* end, std::string value);
    llvm::Value* accept(Visitor& visitor) override;
};

class VariableExpr : public Expr {
public:
    std::string name;

    VariableExpr(Location* start, Location* end, std::string name);
    llvm::Value* accept(Visitor& visitor) override;
};

class VariableAssignmentExpr : public Expr {
public:
    std::string name;
    Type* type;
    std::unique_ptr<Expr> value;

    VariableAssignmentExpr(Location* start, Location* end, std::string name, Type* type, std::unique_ptr<Expr> value);
    llvm::Value* accept(Visitor& visitor) override;
};

class ConstExpr : public VariableAssignmentExpr {
public:
    ConstExpr(Location* start, Location* end, std::string name, Type* type, std::unique_ptr<Expr> value);
    llvm::Value* accept(Visitor& visitor) override;
};

class ArrayExpr : public Expr {
public:
    std::vector<std::unique_ptr<Expr>> elements;

    ArrayExpr(Location* start, Location* end, std::vector<std::unique_ptr<Expr>> elements);
    llvm::Value* accept(Visitor& visitor) override;
};

class UnaryOpExpr : public Expr {
public:
    std::unique_ptr<Expr> value;
    TokenType op;

    UnaryOpExpr(Location* start, Location* end, TokenType op, std::unique_ptr<Expr> value);
    llvm::Value* accept(Visitor& visitor) override;
};

class BinaryOpExpr : public Expr {
public:
    std::unique_ptr<Expr> left, right;
    TokenType op;

    BinaryOpExpr(Location* start, Location* end, TokenType op, std::unique_ptr<Expr> left, std::unique_ptr<Expr> right);
    llvm::Value* accept(Visitor& visitor) override;
};

class CallExpr : public Expr {
public:
    std::unique_ptr<ast::Expr> callee;
    std::vector<std::unique_ptr<Expr>> args;

    CallExpr(Location* start, Location* end, std::unique_ptr<ast::Expr> callee, std::vector<std::unique_ptr<Expr>> args);
    llvm::Value* accept(Visitor& visitor) override;
};

class ReturnExpr : public Expr {
public:
    std::unique_ptr<Expr> value;

    ReturnExpr(Location* start, Location* end, std::unique_ptr<Expr> value);
    llvm::Value* accept(Visitor& visitor) override;
};

class PrototypeExpr : public Expr {
public:
    std::string name;
    std::vector<Argument> args;
    bool has_varargs;
    Type* return_type;

    PrototypeExpr(Location* start, Location* end, std::string name, Type* return_type, std::vector<Argument> args, bool has_varargs);
    llvm::Value* accept(Visitor& visitor) override;
};

class FunctionExpr : public Expr {
public:
    std::unique_ptr<PrototypeExpr> prototype;
    std::unique_ptr<BlockExpr> body;
    
    FunctionExpr(Location* start, Location* end, std::unique_ptr<PrototypeExpr> prototype, std::unique_ptr<BlockExpr> body);
    llvm::Value* accept(Visitor& visitor) override;
};

class IfExpr : public Expr {
public:
    std::unique_ptr<Expr> condition;
    std::unique_ptr<BlockExpr> body;
    std::unique_ptr<BlockExpr> ebody;

    IfExpr(Location* start, Location* end, std::unique_ptr<Expr> condition, std::unique_ptr<BlockExpr> body, std::unique_ptr<BlockExpr> ebody);
    llvm::Value* accept(Visitor& visitor) override;
};

class WhileExpr : public Expr {
public:
    std::unique_ptr<Expr> condition;
    std::unique_ptr<BlockExpr> body;

    WhileExpr(Location* start, Location* end, std::unique_ptr<Expr> condition, std::unique_ptr<BlockExpr> body);
    llvm::Value* accept(Visitor& visitor) override;
};

class StructExpr : public Expr {
public:
    std::string name;
    bool packed;
    std::map<std::string, Argument> fields;
    std::vector<std::unique_ptr<FunctionExpr>> methods;

    StructExpr(Location* start, Location* end, std::string name, bool packed, std::map<std::string, Argument> fields, std::vector<std::unique_ptr<FunctionExpr>> methods);
    llvm::Value* accept(Visitor& visitor) override;
};

class AttributeExpr : public Expr {
public:
    std::unique_ptr<Expr> parent;
    std::string attribute;

    AttributeExpr(Location* start, Location* end, std::string attribute, std::unique_ptr<Expr> parent);
    llvm::Value* accept(Visitor& visitor) override;
};

class ElementExpr : public Expr {
public:
    std::unique_ptr<Expr> value;
    std::unique_ptr<Expr> index;

    ElementExpr(Location* start, Location* end, std::unique_ptr<Expr> value, std::unique_ptr<Expr> index);
    llvm::Value* accept(Visitor& visitor) override;
};

class IncludeExpr : public Expr {
public:
    std::string path;

    IncludeExpr(Location* start, Location* end, std::string path);
    llvm::Value* accept(Visitor& visitor) override;
};

class NamespaceExpr : public Expr {
public:
    std::string name;
    
    std::vector<std::unique_ptr<FunctionExpr>> functions;
    std::vector<std::unique_ptr<StructExpr>> structs;
    std::vector<std::unique_ptr<ConstExpr>> consts;
    std::vector<std::unique_ptr<NamespaceExpr>> namespaces;

    NamespaceExpr(Location* start, Location* end, std::string name, std::vector<std::unique_ptr<FunctionExpr>> functions, std::vector<std::unique_ptr<StructExpr>> structs, std::vector<std::unique_ptr<ConstExpr>> consts, std::vector<std::unique_ptr<NamespaceExpr>> namespaces);
    llvm::Value* accept(Visitor& visitor) override;
};

class NamespaceAttributeExpr : public Expr {
public:
    std::unique_ptr<Expr> parent;
    std::string attribute;

    NamespaceAttributeExpr(Location* start, Location* end, std::string attribute, std::unique_ptr<Expr> parent);
    llvm::Value* accept(Visitor& visitor) override;
};

class Program {
public:
    std::vector<std::unique_ptr<Expr>> ast;

    Program(std::vector<std::unique_ptr<Expr>> ast);
};

}

#endif