#pragma once

#include <quart/language/types.h>
#include <quart/bytecode/basic_block.h>
#include <quart/language/symbol.h>
#include <quart/parser/ast.h>
#include <quart/common.h>
#include <quart/llvm.h>

namespace quart {

class Scope;

struct FunctionParameter {
    enum Flags : u8 {
        None,
        Keyword  = 1 << 0,
        Mutable  = 1 << 1,
        Self     = 1 << 2,
        Variadic = 1 << 3,
        Byval    = 1 << 4
    };

    String name;
    Type* type;

    u8 flags;

    u32 index;
    Span span;

    bool is_reference() const { return this->type->is_reference(); }
    bool is_mutable() const { return this->flags & Flags::Mutable; }
    bool is_byval() const { return this->flags & Flags::Byval; }
    bool is_self() const { return this->flags & Flags::Self; }
};

struct Loop {
    bytecode::BasicBlock* start;
    bytecode::BasicBlock* end;
};

class Function : public Symbol {
public:
    static bool classof(const Symbol* symbol) { return symbol->type() == Symbol::Function; }

    static RefPtr<Function> create(
        Span,
        String name, 
        Vector<FunctionParameter> parameters, 
        FunctionType* underlying_type,
        RefPtr<Scope>,
        LinkageSpecifier,
        RefPtr<LinkInfo>,
        bool is_public
    );

    Span span() const { return m_span; }

    LinkageSpecifier linkage_specifier() const { return m_linkage_specifier; }
    LinkInfo const* link_info() const { return m_link_info.get(); }

    FunctionType* underlying_type() const { return m_underlying_type; }

    Type* return_type() const { return m_underlying_type->return_type(); }
    Vector<FunctionParameter> const& parameters() const { return m_parameters; }

    String const& qualified_name() const { return m_qualified_name; }

    RefPtr<Scope> scope() const { return m_scope; }

    bool is_decl() const { return !m_entry_block; }
    bool is_extern() const { return m_linkage_specifier > LinkageSpecifier::None; }
    bool is_main() const { return m_qualified_name == "main"; }

    bool is_struct_return() const { return m_underlying_type->return_type()->is_struct(); }
    bool is_member_method() const { return m_parameters.size() > 0 && m_parameters[0].is_self(); }

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

    void add_struct_local(size_t index) { m_struct_locals.insert(index); }
    bool is_struct_local(size_t index) const { return m_struct_locals.contains(index); }

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
        Span span,
        String name,
        Vector<FunctionParameter> parameters,
        FunctionType* underlying_type,
        RefPtr<Scope> scope,
        LinkageSpecifier linkage_specifier,
        RefPtr<LinkInfo> link_info,
        bool is_public
    ) : Symbol(move(name), Symbol::Function, is_public), m_span(span),
        m_linkage_specifier(linkage_specifier), m_underlying_type(underlying_type), m_parameters(move(parameters)), 
        m_link_info(move(link_info)), m_scope(move(scope)) {
            
        this->set_qualified_name();
    }

    Span m_span;

    LinkageSpecifier m_linkage_specifier;

    FunctionType* m_underlying_type;
    String m_qualified_name;

    Vector<FunctionParameter> m_parameters;

    RefPtr<LinkInfo> m_link_info;

    bytecode::BasicBlock* m_current_block = nullptr;

    bytecode::BasicBlock* m_entry_block = nullptr;
    Vector<bytecode::BasicBlock*> m_basic_blocks;

    Vector<Type*> m_locals;
    Set<size_t> m_struct_locals;

    Loop m_loop = {};
    
    RefPtr<Scope> m_scope = nullptr;
};

}