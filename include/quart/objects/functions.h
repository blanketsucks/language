#pragma once

#include <quart/utils/pointer.h>
#include <quart/lexer/location.h>
#include <quart/parser/ast.h>
#include <quart/objects/types.h>
#include <quart/llvm.h>

struct Scope;
struct Struct;

struct FunctionCall {
    llvm::Function* function;
    std::vector<llvm::Value*> args;
    llvm::Value* self;
    llvm::Value* store;
};

struct FunctionReturn {
    Type type;
    llvm::AllocaInst* value;
    llvm::BasicBlock* block;

    FunctionReturn(Type type, llvm::AllocaInst* value, llvm::BasicBlock* block) : type(type), value(value), block(block) {}
    llvm::Type* operator->() { return this->type.value; }

    bool is_reference() { return this->type.is_reference; }
};

struct Branch {
    bool has_return;
    bool has_break;
    bool has_continue;

    llvm::BasicBlock* loop;
    llvm::BasicBlock* end;

    Branch() : has_return(false), has_break(false), has_continue(false) {}

    bool has_jump() { return this->has_return || this->has_break || this->has_continue; }
};

struct FunctionArgument {
    std::string name;

    Type type;
    llvm::Value* default_value;

    uint32_t index;

    bool is_kwarg;
    bool is_immutable;
    bool is_self;
    bool is_variadic;

    Span span;

    bool is_reference() { return this->type.is_reference; }
};

struct StructDestructor {
    llvm::Value* self;
    Struct* structure;
};

struct Function {
    std::string name;
    llvm::Function* value;

    FunctionReturn ret;
    std::vector<FunctionArgument> args;
    std::map<std::string, FunctionArgument> kwargs;

    bool is_private;
    bool is_entry;
    bool is_intrinsic;
    bool is_anonymous;
    bool used;
    bool noreturn;
    bool is_finalized;
    bool is_operator;

    std::vector<Branch*> branches;

    Branch* current_branch;
    llvm::BasicBlock* current_block;

    std::vector<llvm::Function*> calls;
    std::vector<StructDestructor> destructors;
    std::vector<utils::Scope<ast::Expr>> defers;

    utils::Ref<Struct> parent;

    Scope* scope;

    ast::Attributes attrs;

    Span span;

    Function(
        const std::string& name,
        std::vector<FunctionArgument> args,
        std::map<std::string, FunctionArgument> kwargs,
        Type return_type,
        llvm::Function* value,
        bool is_entry,
        bool is_intrinsic,
        bool is_anonymous, 
        bool is_operator,
        Span span,
        ast::Attributes attrs
    );

    std::string get_mangled_name();

    Branch* create_branch(llvm::BasicBlock* loop = nullptr, llvm::BasicBlock* end = nullptr);

    bool has_return();

    uint32_t argc();
    bool is_c_variadic(); // (arg1, arg2, ...)
    bool is_variadic();   // (arg1, arg2, *args)

    bool has_any_default_value();
    uint32_t get_default_arguments_count();
    
    bool has_kwarg(const std::string& name);
    std::vector<FunctionArgument> params();

    void destruct(Visitor& visitor);
};
