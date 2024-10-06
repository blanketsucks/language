#pragma once

#include <quart/language/types.h>
#include <quart/bytecode/basic_block.h>
#include <quart/language/symbol.h>
#include <quart/parser/ast.h>
#include <quart/common.h>
#include <quart/llvm.h>

namespace quart {

class Scope;
struct Struct;

struct FunctionParameter {
    enum Flags : u8 {
        None,
        Keyword  = 1 << 0,
        Mutable  = 1 << 1,
        Self     = 1 << 2,
        Variadic = 1 << 3
    };

    String name;
    quart::Type* type;

    u8 flags;

    u32 index;
    Span span;

    bool is_reference() const { return this->type->is_reference(); }
    bool is_mutable() const { return this->flags & Flags::Mutable; }
};

struct Loop {
    bytecode::BasicBlock* start;
    bytecode::BasicBlock* end;
};

class Function : public Symbol {
public:
    static bool classof(const Symbol* symbol) { return symbol->type() == Symbol::Function; }

    static RefPtr<Function> create(String name, Vector<FunctionParameter> parameters, FunctionType* underlying_type, Scope*, LinkageSpecifier);

    LinkageSpecifier linkage_specifier() const { return m_linkage_specifier; }

    FunctionType* underlying_type() const { return m_underlying_type; }
    Type* return_type() const { return m_underlying_type->return_type(); }

    Vector<FunctionParameter> const& parameters() const { return m_parameters; }

    String const& qualified_name() const { return m_qualified_name; }

    Scope* scope() const { return m_scope; }

    size_t local_count() const { return m_locals.size(); }
    
    size_t allocate_local() {
        m_locals.push_back(nullptr);
        return m_locals.size() - 1;
    }

    void set_local_type(size_t index, Type* type) { m_locals[index] = type; }
    Vector<Type*> const& locals() const { return m_locals; }

    bytecode::BasicBlock* entry_block() { return m_entry_block; }
    bytecode::BasicBlock* current_block() { return m_current_block; }

    Vector<bytecode::BasicBlock*> const& basic_blocks() const { return m_basic_blocks; }

    void set_current_block(bytecode::BasicBlock* block) { m_current_block = block; }
    void set_entry_block(bytecode::BasicBlock* block) {
        m_basic_blocks.push_back(block);
        m_entry_block = block;
    }

    void insert_block(bytecode::BasicBlock* block) { m_basic_blocks.push_back(block); }

    Loop const& current_loop() const { return m_loop; }
    Loop& current_loop() { return m_loop; }

    void set_current_loop(Loop loop) { m_loop = loop; }

private:
    void set_qualified_name();

    Function(
        String name,
        Vector<FunctionParameter> parameters,
        FunctionType* underlying_type,
        Scope* scope,
        LinkageSpecifier linkage_specifier
    ) : Symbol(move(name), Symbol::Function), m_linkage_specifier(linkage_specifier), m_underlying_type(underlying_type), m_parameters(move(parameters)), m_scope(scope) {
        this->set_qualified_name();
    }

    LinkageSpecifier m_linkage_specifier;

    FunctionType* m_underlying_type;
    String m_qualified_name;

    Vector<FunctionParameter> m_parameters;

    bytecode::BasicBlock* m_current_block = nullptr;

    bytecode::BasicBlock* m_entry_block = nullptr;
    Vector<bytecode::BasicBlock*> m_basic_blocks;

    Vector<Type*> m_locals;

    Loop m_loop = {};
    
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