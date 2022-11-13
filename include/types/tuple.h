#ifndef _TYPES_TUPLE_H
#define _TYPES_TUPLE_H

#include "type.h"

// The main idea here of tuples is that each tuple type is going to be it's own independent structure.
// For example if you have a tuple `(int, float, char*)` it's going to be translated into the LLVM IR as
// `%tuple.0 = type { i32, float, i8* }`, and whenever you reference that same tuple type it's going to use
// `tuple.0` in the IR.
// Each tuple type will be identified with a "hash" which is just a bitwise or of all of it's fields.
class TupleType : public Type {
public:
    static TupleType* create(std::vector<Type*> types);

    static TupleType* from_llvm_type(llvm::StructType* type);
    llvm::Type* to_llvm_type(Visitor& visitor) override;

    std::vector<Type*> getElementTypes();

    static uint32_t getHashFromTypes(std::vector<Type*> types);
    uint32_t hash();

    std::string str() override;

    bool is_compatible(Type* other) override;
    bool is_compatible(llvm::Type* type) override;

private:
    TupleType(std::vector<Type*> types);

    std::vector<Type*> types;
};

#endif