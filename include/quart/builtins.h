#pragma once

#include <quart/parser/ast.h>
#include <quart/llvm.h>

#define BUILTIN(n) llvm::Value* builtin_##n(Visitor& visitor, ast::CallExpr* call)

class Visitor;

typedef llvm::Value* (*BuiltinFunction)(Visitor&, ast::CallExpr*);

class Builtins {
public:
    static void init(Visitor& visitor);
};