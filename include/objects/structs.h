#ifndef _OBJECTS_STRUCTS_H
#define _OBJECTS_STRUCTS_H

#include "utils/pointer.h"
#include "lexer/tokens.h"

#include "llvm/IR/Type.h"
#include "llvm/IR/DerivedTypes.h"

struct Scope;
struct Function;

struct StructField {
    std::string name;
    llvm::Type* type;
    
    bool is_private;
    bool is_readonly;

    uint32_t index;
    uint32_t offset;
};

struct Struct {
    std::string name;
    std::string qualified_name;

    llvm::StructType* type;

    std::map<std::string, StructField> fields;
    Scope* scope;

    std::vector<utils::Ref<Struct>> parents;
    std::vector<utils::Ref<Struct>> children;

    bool opaque;

    Span span;

    Struct(
        std::string name,
        std::string qualified_name,
        bool opaque, 
        llvm::StructType* type, 
        std::map<std::string, StructField> fields
    );

    int get_field_index(std::string name);
    StructField get_field_at(uint32_t index);
    std::vector<StructField> get_fields(bool with_private = false);

    bool has_method(std::string name);
    utils::Ref<Function> get_method(std::string name);

    std::vector<utils::Ref<Struct>> expand();
};

#endif