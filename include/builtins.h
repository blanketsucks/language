#ifndef _BUILTINS_H
#define _BUILTINS_H

#include "parser/ast.h"
#include "llvm.h"

#define BUILTIN(n) llvm::Value* builtin_##n(Visitor& visitor, ast::CallExpr* call)

class Visitor;

typedef llvm::Value* (*BuiltinFunction)(Visitor&, ast::CallExpr*);

class Builtins {
public:
    static void init(Visitor& visitor);
};

#endif