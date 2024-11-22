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

    static RefPtr<Function> create(
        String name, Vector<FunctionParameter> parameters, FunctionType* underlying_type, Scope*, LinkageSpecifier, RefPtr<LinkInfo>
    );

    LinkageSpecifier linkage_specifier() const { return m_linkage_specifier; }
    LinkInfo const* link_info() const { return m_link_info.get(); }

    FunctionType* underlying_type() const { return m_underlying_type; }

    Type* return_type() const { return m_underlying_type->return_type(); }
    Vector<FunctionParameter> const& parameters() const { return m_parameters; }

    String const& qualified_name() const { return m_qualified_name; }

    Scope* scope() const { return m_scope; }
    bool is_decl() const { return !m_entry_block; }

    size_t local_count() const { return m_locals.size(); }
    Vector<Type*> const& locals() const { return m_locals; }

    bytecode::BasicBlock* entry_block() { return m_entry_block; }
    bytecode::BasicBlock* current_block() { return m_current_block; }
    Vector<bytecode::BasicBlock*> const& basic_blocks() const { return m_basic_blocks; }

    Loop const& current_loop() const { return m_loop; }
    Loop& current_loop() { return m_loop; }
    
    size_t allocate_local() {
        m_locals.push_back(nullptr);
        return m_locals.size() - 1;
    }

    void set_local_type(size_t index, Type* type) { m_locals[index] = type; }

    void set_current_block(bytecode::BasicBlock* block) { m_current_block = block; }
    void set_entry_block(bytecode::BasicBlock* block) {
        m_basic_blocks.push_back(block);
        m_entry_block = block;
    }

    void insert_block(bytecode::BasicBlock* block) { m_basic_blocks.push_back(block); }
    void set_current_loop(Loop loop) { m_loop = loop; }

    void dump() const;

private:
    void set_qualified_name();

    Function(
        String name,
        Vector<FunctionParameter> parameters,
        FunctionType* underlying_type,
        Scope* scope,
        LinkageSpecifier linkage_specifier,
        RefPtr<LinkInfo> link_info
    ) : Symbol(move(name), Symbol::Function), 
        m_linkage_specifier(linkage_specifier), m_underlying_type(underlying_type), m_parameters(move(parameters)), 
        m_scope(scope), m_link_info(move(link_info)) {
        this->set_qualified_name();
    }

    LinkageSpecifier m_linkage_specifier;

    FunctionType* m_underlying_type;
    String m_qualified_name;

    Vector<FunctionParameter> m_parameters;

    RefPtr<LinkInfo> m_link_info;

    bytecode::BasicBlock* m_current_block = nullptr;

    bytecode::BasicBlock* m_entry_block = nullptr;
    Vector<bytecode::BasicBlock*> m_basic_blocks;

    Vector<Type*> m_locals;

    Loop m_loop = {};
    
    Scope* m_scope = nullptr;
};

}