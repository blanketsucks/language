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
    virtual ~Expr() = default;
    virtual llvm::Value* accept(Visitor& visitor) = 0;
};

struct Argument {
    std::string name;
    Type type;
};

class IntegerExpr : public Expr {
public:
    int value;

    IntegerExpr(int value);
    llvm::Value* accept(Visitor& visitor) override;
};

class StringExpr : public Expr {
public:
    std::string value;

    StringExpr(std::string value);
    llvm::Value* accept(Visitor& visitor) override;
};

class VariableExpr : public Expr {
public:
    std::string name;

    VariableExpr(std::string name);
    llvm::Value* accept(Visitor& visitor) override;
};

class VariableAssignmentExpr : public Expr {
public:
    std::string name;
    Type type = VoidType;
    std::unique_ptr<Expr> value;

    VariableAssignmentExpr(std::string name, std::unique_ptr<Expr> value);
    llvm::Value* accept(Visitor& visitor) override;
};


class ArrayExpr : public Expr {
public:
    std::vector<std::unique_ptr<Expr>> elements;

    ArrayExpr(std::vector<std::unique_ptr<Expr>> elements);
    llvm::Value* accept(Visitor& visitor) override;
};

class BinaryOpExpr : public Expr {
public:
    std::unique_ptr<Expr> left, right;
    TokenType op;

    BinaryOpExpr(TokenType op, std::unique_ptr<Expr> left, std::unique_ptr<Expr> right);
    llvm::Value* accept(Visitor& visitor) override;
};

class CallExpr : public Expr {
public:
    std::string callee;
    std::vector<std::unique_ptr<Expr>> args;

    CallExpr(std::string callee, std::vector<std::unique_ptr<Expr>> args);
    llvm::Value* accept(Visitor& visitor) override;
};

class ReturnExpr : public Expr {
public:
    std::unique_ptr<Expr> value;

    ReturnExpr(std::unique_ptr<Expr> value);
    llvm::Value* accept(Visitor& visitor) override;
};

class PrototypeExpr : public Expr {
public:
    std::string name;
    std::vector<Argument> args;
    Type return_type;

    PrototypeExpr(std::string name, Type return_type, std::vector<Argument> args);
    llvm::Value* accept(Visitor& visitor) override;
};

class FunctionExpr : public Expr {
public:
    std::unique_ptr<PrototypeExpr> prototype;
    std::vector<std::unique_ptr<Expr>> body;
    
    FunctionExpr(std::unique_ptr<PrototypeExpr> prototype, std::vector<std::unique_ptr<Expr>> body);
    llvm::Value* accept(Visitor& visitor) override;
};

class IfExpr : public Expr {
public:
    std::unique_ptr<Expr> condition;
    std::vector<std::unique_ptr<Expr>> body;
    std::vector<std::unique_ptr<Expr>> ebody; // The else body

    IfExpr(std::unique_ptr<Expr> condition, std::vector<std::unique_ptr<Expr>> body, std::vector<std::unique_ptr<Expr>> ebody);
    llvm::Value* accept(Visitor& visitor) override;
};

class Program {
public:
    std::vector<std::unique_ptr<Expr>> ast;

    Program(std::vector<std::unique_ptr<Expr>> ast);
};

}

#endif