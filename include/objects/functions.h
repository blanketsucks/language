#ifndef _OBJECTS_FUNCTIONS_H
#define _OBJECTS_FUNCTIONS_H

#include "utils/pointer.h"
#include "lexer/tokens.h"
#include "parser/ast.h"
#include "llvm.h"

struct Scope;
struct Struct;

struct FunctionCall {
    llvm::Function* function;
    std::vector<llvm::Value*> args;
    llvm::Value* store;
};

struct FunctionReturn {
    llvm::Type* type;
    llvm::AllocaInst* value;
    llvm::BasicBlock* block;

    FunctionReturn(llvm::Type* type, llvm::AllocaInst* value, llvm::BasicBlock* block) : type(type), value(value), block(block) {}
};

struct Branch {
    std::string name;

    bool has_return;
    bool has_break;
    bool has_continue;

    llvm::BasicBlock* loop;
    llvm::BasicBlock* end;

    Branch(std::string name) : name(name), has_return(false), has_break(false), has_continue(false) {}

    bool has_jump() { return this->has_return || this->has_break || this->has_continue; }
};

struct FunctionArgument {
    std::string name;

    llvm::Type* type;
    llvm::Value* default_value;

    uint32_t index;

    bool is_reference;
    bool is_kwarg;
    bool is_immutable;
};

struct Function {
    std::string name;
    llvm::Type* ret;

    llvm::Function* value;

    std::vector<FunctionArgument> args;
    std::map<std::string, FunctionArgument> kwargs;
    std::map<int, llvm::Value*> defaults;

    Scope* scope;

    std::vector<Branch*> branches;
    Branch* branch;

    llvm::AllocaInst* return_value;
    llvm::BasicBlock* return_block;
    llvm::BasicBlock* current_block;

    std::vector<llvm::Function*> calls;

    utils::Shared<Struct> parent;
    bool is_private;

    ast::Attributes attrs;

    bool is_entry;
    bool is_intrinsic;
    bool is_anonymous;
    bool used;
    bool noreturn;

    bool is_finalized;

    Location start;
    Location end;

    Function(
        std::string name,
        std::vector<FunctionArgument> args,
        std::map<std::string, FunctionArgument> kwargs,
        llvm::Type* ret,
        llvm::Function* value,
        bool is_entry,
        bool is_intrinsic,
        bool is_anonymous, 
        ast::Attributes attrs
    );

    std::string get_mangled_name();

    Branch* create_branch(std::string name, llvm::BasicBlock* loop = nullptr, llvm::BasicBlock* end = nullptr);
    bool has_return();

    uint32_t argc();
    bool is_variadic();

    bool has_any_default_value();
    uint32_t get_default_arguments_count();
    
    bool has_kwarg(std::string name);
    std::vector<FunctionArgument> params();
};


#endif