#pragma once

#include <quart/language/types.h>
#include <quart/language/symbol.h>
#include <quart/common.h>

#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>

#include <map>

namespace quart {

struct Function;
class Scope;

struct StructField {
    enum Flags : u8 {
        None,
        Private  = 1 << 0,
        Readonly = 1 << 1,
        Mutable  = 1 << 2,
    };

    String name;
    Type* type;
    
    u8 flags;

    u32 index;

    inline bool is_private() const { return flags & Flags::Private; }
    inline bool is_readonly() const { return flags & Flags::Readonly; }
    inline bool is_mutable() const { return flags & Flags::Mutable; }
};

class Struct : public Symbol {
public:
    static bool classof(const Symbol* symbol) { return symbol->type() == Symbol::Struct; }

    static RefPtr<Struct> create(String name, StructType* underlying_type, Scope* parent) {
        return RefPtr<Struct>(new Struct(move(name), underlying_type, parent));
    }

    static RefPtr<Struct> create(String name, StructType* underlying_type, HashMap<String, StructField> fields, Scope* scope) {
        return RefPtr<Struct>(new Struct(move(name), underlying_type, move(fields), scope));
    }

    String const& qualified_name() const { return m_qualified_name; }

    StructType* underlying_type() const { return m_underlying_type; }

    HashMap<String, StructField> const& fields() const { return m_fields; }
    Scope* scope() const { return m_scope; }

    StructField const* find(const String& name) const;
    bool has_method(const String& name) const;

    bool opaque() const { return m_opaque; }

    void set_fields(HashMap<String, StructField> fields) { m_fields = move(fields); }

private:
    void set_qualified_name(Scope* parent = nullptr);

    Struct(String name, StructType* underlying_type, Scope* parent) : Symbol(move(name), Symbol::Struct), m_underlying_type(underlying_type), m_opaque(true) {
        this->set_qualified_name(parent);
    }

    Struct(
        String name, StructType* underlying_type, HashMap<String, StructField> fields, Scope* scope
    ) : Symbol(move(name), Symbol::Struct), m_underlying_type(underlying_type), m_opaque(false), m_fields(move(fields)), m_scope(scope) {
        this->set_qualified_name();
    }
    
    String m_qualified_name;

    StructType* m_underlying_type;

    bool m_opaque;

    HashMap<String, StructField> m_fields;
    Scope* m_scope = nullptr;
};

// struct Struct_ {
//     std::string name;

//     quart::StructType* type;

//     std::map<std::string, StructField> fields;
//     Scope* scope;

//     std::vector<Struct*> parents;

//     bool opaque;

//     static RefPtr<Struct> create(const std::string& name, quart::StructType* type, bool opaque);
//     static RefPtr<Struct> create(
//         const std::string& name, quart::StructType* type, std::map<std::string, StructField> fields, bool opaque
//     );

//     i32 get_field_index(const std::string& name);
//     StructField get_field_at(u32 index);
//     std::vector<StructField> get_fields(bool with_private = false);

//     bool has_method(const std::string& name);
//     RefPtr<Function> get_method(const std::string& name);

//     std::vector<Struct*> expand();

// private:
//     Struct(const std::string& name, quart::StructType* type, bool opaque);
//     Struct(const std::string& name, quart::StructType* type, std::map<std::string, StructField> fields, bool opaque);
// };

}