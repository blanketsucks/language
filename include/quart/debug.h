#pragma once

#include <quart/parser/ast.h>

#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/IRBuilder.h>

namespace quart {

struct DebugInfo {
    llvm::IRBuilder<>* builder;
    llvm::DIBuilder* dbuilder;

    llvm::DICompileUnit* unit;
    llvm::DIFile* file;

    std::vector<llvm::DIScope*> scopes;

    DebugInfo() = default;
    DebugInfo(
        llvm::IRBuilder<>* builder, llvm::DIBuilder* dbuilder, llvm::DICompileUnit* unit, llvm::DIFile* file
    ) : builder(builder), dbuilder(dbuilder), unit(unit), file(file) {}

    llvm::DIType* wrap(quart::Type* type);
    llvm::DISubprogram* create_function(
        llvm::DIScope* scope, 
        llvm::Function* function, 
        const std::string& name,
        u32 line
    );

    void emit(ast::Expr* expr);

private:
    std::map<llvm::Type*, llvm::DIType*> types;
};

}