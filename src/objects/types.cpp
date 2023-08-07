#include <quart/objects/types.h>

Type::Type() : value(nullptr), is_reference(false), is_immutable(true) {};
Type::Type(
    llvm::Type* type,
    bool is_reference,
    bool is_pointer,
    bool is_immutable
) : value(type), is_reference(is_reference), is_pointer(is_pointer), is_immutable(is_immutable) {};

Type Type::null() { return Type(nullptr); }

bool Type::is_null() { return this->value == nullptr; }

llvm::Type* Type::operator->() { return this->value; }
llvm::Type* Type::operator*() { return this->value; }

Type::operator llvm::Type*() { return this->value; }