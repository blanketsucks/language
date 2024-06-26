#pragma once

#include <quart/parser/ast.h>
#include <quart/language/types.h>
#include <quart/llvm.h>
#include <quart/common.h>

#include <quart/language/symbol.h>

namespace quart {

class Scope;
struct Struct;

// struct EarlyFunctionCall {
//     llvm::Function* function;
//     std::vector<llvm::Value*> args;

//     const quart::FunctionType* type;

//     llvm::Value* self;
//     llvm::Value* store;
// };

// struct FunctionReturn {
//     quart::Type* type;
//     llvm::AllocaInst* value;

//     llvm::BasicBlock* block;

//     FunctionReturn() : type(nullptr), value(nullptr), block(nullptr) {}
//     FunctionReturn(
//         quart::Type* type, llvm::AllocaInst* value, llvm::BasicBlock* block
//     ) : type(type), value(value), block(block) {}

//     quart::Type* operator->() const { return this->type; }
// };

struct Parameter {
    enum Flags : u8 {
        None,
        Keyword  = 1 << 0,
        Mutable  = 1 << 1,
        Self     = 1 << 2,
        Variadic = 1 << 3
    };

    std::string name;
    quart::Type* type;

    llvm::Value* default_value = nullptr;
    u8 flags;

    u32 index;
    Span span;

    bool is_reference() const { return this->type->is_reference(); }
    bool is_mutable() const { return this->flags & Flags::Mutable; }
    bool has_default_value() const { return this->default_value != nullptr; }
};

// struct Loop {
//     llvm::BasicBlock* start = nullptr;
//     llvm::BasicBlock* end = nullptr;
// };

class Function : public Symbol {
public:
    static bool classof(const Symbol* symbol) { return symbol->type() == Symbol::Function; }

    RefPtr<Function> create(String name, Type* underlying_type, Scope*);

    FunctionType* underlying_type() const { return m_underlying_type; }
    Type* return_type() const { return m_underlying_type->return_type(); }

    String const& qualified_name() const { return m_qualified_name; }

    Scope* scope() const { return m_scope; }

private:
    void set_qualified_name();

    Function(
        String name, 
        FunctionType* underlying_type,
        Scope* scope
    ) : Symbol(move(name), Symbol::Function), m_underlying_type(underlying_type), m_scope(scope) {
        this->set_qualified_name();
    }

    FunctionType* m_underlying_type;
    String m_qualified_name;

    Scope* m_scope = nullptr;
};

// struct Function_ {
//     enum Flags : u16 {
//         None,
//         Private       = 1 << 0,
//         Entry         = 1 << 1,
//         Anonymous     = 1 << 2,
//         LLVMIntrinsic = 1 << 3,
//         NoReturn      = 1 << 4,
//         Operator      = 1 << 5,
//         Used          = 1 << 6,
//         Finalized     = 1 << 7,
//         HasReturn     = 1 << 8
//     };

//     static RefPtr<Function> create(
//         llvm::Function* value,
//         quart::Type* type,
//         const std::string& name,
//         std::vector<Parameter> params,
//         std::map<std::string, Parameter> kwargs,
//         quart::Type* return_type,
//         u16 flags,
//         const Span& span,
//         const ast::Attributes& attrs
//     );

//     operator llvm::Function*() const { return this->value; }
    
//     inline bool is_entry() const { return this->flags & Flags::Entry; }
//     inline bool is_private() const { return this->flags & Flags::Private; }
//     inline bool has_return() const { return this->flags & Flags::HasReturn; }

//     u32 argc() const { return this->params.size() + this->kwargs.size(); }
//     bool is_c_variadic() const { return this->value->isVarArg(); }
//     bool is_variadic() const;   // (arg1, arg2, *args)

//     bool has_any_default_value() const;
//     u32 get_default_arguments_count() const;
    
//     bool has_keyword_parameter(const std::string& name) const;

//     quart::Type* get_return_type() const { return this->ret.type; }
//     llvm::AllocaInst* get_return_value() const { return this->ret.value; }
//     llvm::BasicBlock* get_return_block() const { return this->ret.block; }

//     std::string name;

//     llvm::Function* value;
//     quart::Type* type;
//     FunctionReturn ret;

//     std::vector<Parameter> params;
//     std::map<std::string, Parameter> kwargs;

//     u16 flags;

//     Struct* parent;

//     llvm::BasicBlock* current_block;
//     Loop loop;

//     std::vector<llvm::Function*> calls;

//     Scope* scope;

//     ast::Attributes attrs;
//     Span span;
// private:
//     Function(
//         llvm::Function* value,
//         quart::Type* type,
//         const std::string& name,
//         std::vector<Parameter> params,
//         std::map<std::string, Parameter> kwargs,
//         quart::Type* return_type,
//         u16 flags,
//         const Span& span,
//         const ast::Attributes& attrs
//     );
// };

}