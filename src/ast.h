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

class Expr {
public:
    Location start;
    Location end;

    Expr(Location start, Location end) : start(start), end(end) {}

    virtual ~Expr() = default;
    virtual llvm::Value* accept(Visitor& visitor) = 0;
};

struct Argument {
    std::string name;
    Type* type;
};

class IntegerExpr : public Expr {
public:
    int value;

    IntegerExpr(Location start, Location end, int value);
    llvm::Value* accept(Visitor& visitor) override;
};

class StringExpr : public Expr {
public:
    std::string value;

    StringExpr(Location start, Location end, std::string value);
    llvm::Value* accept(Visitor& visitor) override;
};

class VariableExpr : public Expr {
public:
    std::string name;

    VariableExpr(Location start, Location end, std::string name);
    llvm::Value* accept(Visitor& visitor) override;
};

class VariableAssignmentExpr : public Expr {
public:
    std::string name;
    Type* type;
    std::unique_ptr<Expr> value;

    VariableAssignmentExpr(Location start, Location end, std::string name, Type* type, std::unique_ptr<Expr> value);
    llvm::Value* accept(Visitor& visitor) override;
};


class ArrayExpr : public Expr {
public:
    std::vector<std::unique_ptr<Expr>> elements;

    ArrayExpr(Location start, Location end, std::vector<std::unique_ptr<Expr>> elements);
    llvm::Value* accept(Visitor& visitor) override;
};

class UnaryOpExpr : public Expr {
public:
    std::unique_ptr<Expr> value;
    TokenType op;

    UnaryOpExpr(Location start, Location end, TokenType op, std::unique_ptr<Expr> value);
    llvm::Value* accept(Visitor& visitor) override;
};

class BinaryOpExpr : public Expr {
public:
    std::unique_ptr<Expr> left, right;
    TokenType op;

    BinaryOpExpr(Location start, Location end, TokenType op, std::unique_ptr<Expr> left, std::unique_ptr<Expr> right);
    llvm::Value* accept(Visitor& visitor) override;
};

class CallExpr : public Expr {
public:
    std::string callee;
    std::vector<std::unique_ptr<Expr>> args;

    CallExpr(Location start, Location end, std::string callee, std::vector<std::unique_ptr<Expr>> args);
    llvm::Value* accept(Visitor& visitor) override;
};

class ConstructorExpr : public Expr {
public:
    std::string name;
    std::vector<std::unique_ptr<Expr>> args;

    ConstructorExpr(Location start, Location end, std::string name, std::vector<std::unique_ptr<Expr>> args);
    llvm::Value* accept(Visitor& visitor) override;
};

class ReturnExpr : public Expr {
public:
    std::unique_ptr<Expr> value;

    ReturnExpr(Location start, Location end, std::unique_ptr<Expr> value);
    llvm::Value* accept(Visitor& visitor) override;
};

class PrototypeExpr : public Expr {
public:
    std::string name;
    std::vector<Argument> args;
    Type* return_type;

    PrototypeExpr(Location start, Location end, std::string name, Type* return_type, std::vector<Argument> args);
    llvm::Value* accept(Visitor& visitor) override;
};

class FunctionExpr : public Expr {
public:
    std::unique_ptr<PrototypeExpr> prototype;
    std::vector<std::unique_ptr<Expr>> body;
    
    FunctionExpr(Location start, Location end, std::unique_ptr<PrototypeExpr> prototype, std::vector<std::unique_ptr<Expr>> body);
    llvm::Value* accept(Visitor& visitor) override;
};

class IfExpr : public Expr {
public:
    std::unique_ptr<Expr> condition;
    std::vector<std::unique_ptr<Expr>> body;
    std::vector<std::unique_ptr<Expr>> ebody; // The else body

    IfExpr(Location start, Location end, std::unique_ptr<Expr> condition, std::vector<std::unique_ptr<Expr>> body, std::vector<std::unique_ptr<Expr>> ebody);
    llvm::Value* accept(Visitor& visitor) override;
};

class StructExpr : public Expr {
public:
    std::string name;
    bool packed;
    std::map<std::string, Argument> fields;

    StructExpr(Location start, Location end, std::string name, bool packed, std::map<std::string, Argument> fields);
    llvm::Value* accept(Visitor& visitor) override;
};

class AttributeExpr : public Expr {
public:
    std::unique_ptr<Expr> parent;
    std::string attribute;

    AttributeExpr(Location start, Location end, std::string attribute, std::unique_ptr<Expr> parent);
    llvm::Value* accept(Visitor& visitor) override;
};

class IncludeExpr : public Expr {
public:
    std::string path;

    IncludeExpr(Location start, Location end, std::string path);
    llvm::Value* accept(Visitor& visitor) override;
};

class Program {
public:
    std::vector<std::unique_ptr<Expr>> ast;

    Program(std::vector<std::unique_ptr<Expr>> ast);
};

}

#endif