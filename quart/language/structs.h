#pragma once

#include <quart/language/types.h>
#include <quart/language/symbol.h>
#include <quart/language/generics.h>
#include <quart/common.h>

#include <map>
#include <utility>

namespace quart {

namespace ast {
    class Expr;
}

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

    static RefPtr<Struct> create(String name, StructType* underlying_type, RefPtr<Scope> parent, bool is_public) {
        return RefPtr<Struct>(new Struct(move(name), underlying_type, move(parent), is_public));
    }

    static RefPtr<Struct> create(String name, StructType* underlying_type, HashMap<String, StructField> fields, RefPtr<Scope> scope, bool is_public) {
        return RefPtr<Struct>(new Struct(move(name), underlying_type, move(fields), move(scope), is_public));
    }

    static RefPtr<Struct> create(String name, StructType* underlying_type, RefPtr<Scope> scope, Vector<GenericTypeParameter> generic_parameters, bool is_public) {
        return RefPtr<Struct>(new Struct(move(name), underlying_type, move(scope), move(generic_parameters), is_public));
    }

    String const& qualified_name() const { return m_qualified_name; }
    StructType* underlying_type() const { return m_underlying_type; }
    RefPtr<Scope> scope() const { return m_scope; }
    bool opaque() const { return m_opaque; }

    void set_fields(HashMap<String, StructField> fields) { m_fields = move(fields); }
    HashMap<String, StructField> const& fields() const { return m_fields; }

    Vector<GenericTypeParameter> const& generic_parameters() const { return m_generic_parameters; }
    bool is_generic() const { return !m_generic_parameters.empty(); }

    void set_body(Vector<ast::Expr*> body) { m_body = move(body); }
    Vector<ast::Expr*> const& body() const { return m_body; }

    StructField const* find(const String& name) const;

    bool has_method(const String& name) const;
    class Function const* get_method(const String& name) const;

    Vector<TraitType*> const& impls() const { return m_impl_traits; }
    void add_impl_trait(TraitType* trait) { m_impl_traits.push_back(trait); }

    bool impls_trait(TraitType* trait) const {
        return std::find(m_impl_traits.begin(), m_impl_traits.end(), trait) != m_impl_traits.end();
    }

private:
    void set_qualified_name(RefPtr<Scope> = nullptr);

    Struct(
        String name,
        StructType* underlying_type,
        RefPtr<Scope> parent,
        bool is_public
    ) : Symbol(move(name), Symbol::Struct, is_public), m_underlying_type(underlying_type), m_opaque(true) {
        this->set_qualified_name(move(parent));
    }

    Struct(
        String name,
        StructType* underlying_type,
        HashMap<String, StructField> fields,
        RefPtr<Scope> scope,
        bool is_public
    ) : Symbol(move(name), Symbol::Struct, is_public), m_underlying_type(underlying_type), m_opaque(false), m_fields(move(fields)), m_scope(move(scope)) {
        this->set_qualified_name();
    }

    Struct(
        String name,
        StructType* underlying_type,
        RefPtr<Scope> scope,
        Vector<GenericTypeParameter> generic_parameters,
        bool is_public
    ) : Symbol(move(name), Symbol::Struct, is_public), m_underlying_type(underlying_type), m_opaque(false), m_scope(move(scope)), m_generic_parameters(move(generic_parameters)) {
        this->set_qualified_name();
    }
    
    String m_qualified_name;

    StructType* m_underlying_type;

    bool m_opaque;

    HashMap<String, StructField> m_fields;
    RefPtr<Scope> m_scope = nullptr;

    Vector<TraitType*> m_impl_traits;

    Vector<GenericTypeParameter> m_generic_parameters;
    Vector<ast::Expr*> m_body;
};

}