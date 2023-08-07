#pragma once

#include <quart/parser/ast.h>

#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/IRBuilder.h>

struct DebugInfo {
    llvm::IRBuilder<>* builder;
    llvm::DIBuilder* dbuilder;

    llvm::DICompileUnit* unit;
    llvm::DIFile* file;

    llvm::DIType* wrap(llvm::Type* type);
    llvm::DISubprogram* create_function(
        llvm::DIScope* scope, 
        llvm::Function* function, 
        const std::string& name,
        uint32_t line
    );

    void emit(ast::Expr* expr);

private:
    std::map<llvm::Type*, llvm::DIType*> types;
};