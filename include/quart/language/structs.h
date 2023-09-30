#pragma once

#include <quart/lexer/location.h>
#include <quart/language/types.h>

#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>

#include <map>

namespace quart {

struct Function;
struct Scope;

struct StructField {
    enum Flags : uint8_t {
        None,
        Private  = 1 << 0,
        Readonly = 1 << 1,
        Mutable  = 1 << 2,
    };

    std::string name;
    quart::Type* type;
    
    uint8_t flags;

    uint32_t index;
    uint32_t offset;

    inline bool is_private() const { return this->flags & Flags::Private; }
    inline bool is_readonly() const { return this->flags & Flags::Readonly; }
    inline bool is_mutable() const { return this->flags & Flags::Mutable; }
};

struct Struct {
    std::string name;

    quart::StructType* type;

    std::map<std::string, StructField> fields;
    Scope* scope;

    std::vector<Struct*> parents;

    bool opaque;

    Span span;

    static std::shared_ptr<Struct> create(const std::string& name, quart::StructType* type, bool opaque);
    static std::shared_ptr<Struct> create(
        const std::string& name, quart::StructType* type, std::map<std::string, StructField> fields, bool opaque
    );

    int get_field_index(const std::string& name);
    StructField get_field_at(uint32_t index);
    std::vector<StructField> get_fields(bool with_private = false);

    bool has_method(const std::string& name);
    std::shared_ptr<Function> get_method(const std::string& name);

    std::vector<Struct*> expand();

private:
    Struct(const std::string& name, quart::StructType* type, bool opaque);
    Struct(const std::string& name, quart::StructType* type, std::map<std::string, StructField> fields, bool opaque);
};

using StructRef = std::shared_ptr<Struct>;

}