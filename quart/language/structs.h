#pragma once

#include <quart/language/types.h>
#include <quart/language/symbol.h>
#include <quart/common.h>

#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>

#include <map>

namespace quart {

class Scope;

struct StructField {
    enum Flags : u8 {
        None,
        Public   = 1 << 0,
        Readonly = 1 << 1,
        Mutable  = 1 << 2,
    };

    String name;
    Type* type;
    
    u8 flags;

    u32 index;

    inline bool is_public() const { return flags & Flags::Public; }
    inline bool is_readonly() const { return flags & Flags::Readonly; }
    inline bool is_mutable() const { return flags & Flags::Mutable; }
};

struct GenericStruct {
    StructType* type;
    Scope* scope;
};

class Struct : public Symbol {
public:
    static bool classof(const Symbol* symbol) { return symbol->type() == Symbol::Struct; }

    static RefPtr<Struct> create(String name, StructType* underlying_type, Scope* parent, bool is_public) {
        return RefPtr<Struct>(new Struct(move(name), underlying_type, parent, is_public));
    }

    static RefPtr<Struct> create(String name, StructType* underlying_type, HashMap<String, StructField> fields, Scope* scope, bool is_public) {
        return RefPtr<Struct>(new Struct(move(name), underlying_type, move(fields), scope, is_public));
    }

    String const& qualified_name() const { return m_qualified_name; }
    StructType* underlying_type() const { return m_underlying_type; }
    Scope* scope() const { return m_scope; }
    bool opaque() const { return m_opaque; }

    void set_fields(HashMap<String, StructField> fields) { m_fields = move(fields); }
    HashMap<String, StructField> const& fields() const { return m_fields; }

    StructField const* find(const String& name) const;
    bool has_method(const String& name) const;

private:
    void set_qualified_name(Scope* parent = nullptr);

    Struct(
        String name,
        StructType* underlying_type,
        Scope* parent,
        bool is_public
    ) : Symbol(move(name), Symbol::Struct, is_public), m_underlying_type(underlying_type), m_opaque(true) {
        this->set_qualified_name(parent);
    }

    Struct(
        String name, StructType* underlying_type, HashMap<String, StructField> fields, Scope* scope, bool is_public
    ) : Symbol(move(name), Symbol::Struct, is_public), m_underlying_type(underlying_type), m_opaque(false), m_fields(move(fields)), m_scope(scope) {
        this->set_qualified_name();
    }
    
    String m_qualified_name;

    StructType* m_underlying_type;

    bool m_opaque;

    HashMap<String, StructField> m_fields;
    Scope* m_scope = nullptr;
};

}