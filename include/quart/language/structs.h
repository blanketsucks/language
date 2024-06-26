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

    std::string name;
    quart::Type* type;
    
    u8 flags;

    u32 index;
    u32 offset;

    inline bool is_private() const { return this->flags & Flags::Private; }
    inline bool is_readonly() const { return this->flags & Flags::Readonly; }
    inline bool is_mutable() const { return this->flags & Flags::Mutable; }
};

class Struct : public Symbol {
public:
    static bool classof(const Symbol* symbol) { return symbol->type() == Symbol::Struct; }

    StructType* underlying_type() const { return m_underlying_type; }

    HashMap<String, StructField> const& fields() const { return m_fields; }
    Scope* scope() const { return m_scope; }

private:
    Struct(String name, StructType* underlying_type) : Symbol(move(name), Symbol::Struct), m_underlying_type(underlying_type) {}
    
    String m_name;
    StructType* m_underlying_type;

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