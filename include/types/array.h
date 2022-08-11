#ifndef _TYPES_ARRAY_H
#define _TYPES_ARRAY_H

#include "type.h"

#include <stdint.h>

class ArrayType : public Type {
public:
    static ArrayType* create(size_t length, Type* element);
    ~ArrayType() override;

    static ArrayType* fromLLVMType(llvm::ArrayType* type);
    llvm::ArrayType* toLLVMType(llvm::LLVMContext& context) override;

    ArrayType* copy() override;

    std::string str() override;

    size_t getLength() { return this->length; }
    Type* getElementType() { return this->element; }

    bool is_compatible(Type* other) override;
    bool is_compatible(llvm::Type* type) override;

private:
    ArrayType(size_t length, Type* element);

    size_t length;
    Type* element;
};

#endif