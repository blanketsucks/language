#pragma once

#include <quart/lexer/location.h>
#include <quart/parser/ast.h>
#include <quart/language/types.h>
#include <quart/llvm.h>

namespace quart {

struct Scope;
struct Struct;

struct EarlyFunctionCall {
    llvm::Function* function;
    std::vector<llvm::Value*> args;

    llvm::Value* self;
    llvm::Value* store;
};

struct FunctionReturn {
    quart::Type* type;
    llvm::AllocaInst* value;

    llvm::BasicBlock* block;

    FunctionReturn() : type(nullptr), value(nullptr), block(nullptr) {}
    FunctionReturn(
        quart::Type* type, llvm::AllocaInst* value, llvm::BasicBlock* block
    ) : type(type), value(value), block(block) {}

    quart::Type* operator->() { return this->type; }
};

struct Parameter {
    enum Flags : uint8_t {
        None,
        Keyword  = 1 << 0,
        Mutable  = 1 << 1,
        Self     = 1 << 2,
        Variadic = 1 << 3
    };

    std::string name;
    quart::Type* type;

    llvm::Value* default_value;
    uint8_t flags;

    uint32_t index;
    Span span;

    bool is_reference() const { return this->type->is_reference(); }
    bool is_mutable() const { return this->flags & Flags::Mutable; }
    bool has_default_value() const { return this->default_value != nullptr; }
};

struct Loop {
    llvm::BasicBlock* start = nullptr;
    llvm::BasicBlock* end = nullptr;
};

struct Function {
    enum Flags : uint16_t {
        None,
        Private       = 1 << 0,
        Entry         = 1 << 1,
        Anonymous     = 1 << 2,
        LLVMIntrinsic = 1 << 3,
        NoReturn      = 1 << 4,
        Operator      = 1 << 5,
        Used          = 1 << 6,
        Finalized     = 1 << 7,
        HasReturn     = 1 << 8
    };

    std::string name;
    llvm::Function* value;

    quart::Type* type;
    FunctionReturn ret;

    std::vector<Parameter> params;
    std::map<std::string, Parameter> kwargs;

    uint16_t flags;

    Struct* parent;

    llvm::BasicBlock* current_block;
    
    Loop loop;

    std::vector<llvm::Function*> calls;

    Scope* scope;

    ast::Attributes attrs;

    Span span;

    static std::shared_ptr<Function> create(
        llvm::Function* value,
        quart::Type* type,
        const std::string& name,
        std::vector<Parameter> params,
        std::map<std::string, Parameter> kwargs,
        quart::Type* return_type,
        uint16_t flags,
        const Span& span,
        const ast::Attributes& attrs
    );

    operator llvm::Function*() { return this->value; }
    
    inline bool is_entry() const { return this->flags & Flags::Entry; }
    inline bool has_return() const { return this->flags & Flags::HasReturn; }

    uint32_t argc();
    bool is_c_variadic(); // (arg1, arg2, ...)
    bool is_variadic();   // (arg1, arg2, *args)

    bool has_any_default_value();
    uint32_t get_default_arguments_count();
    
    bool has_keyword_parameter(const std::string& name);

private:
    Function(
        llvm::Function* value,
        quart::Type* type,
        const std::string& name,
        std::vector<Parameter> params,
        std::map<std::string, Parameter> kwargs,
        quart::Type* return_type,
        uint16_t flags,
        const Span& span,
        const ast::Attributes& attrs
    );
};

using FunctionRef = std::shared_ptr<Function>;

}