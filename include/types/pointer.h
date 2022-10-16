#ifndef _TYPES_POINTER_H
#define _TYPES_POINTER_H

#include "type.h"

class PointerType : public Type {
public:
    static PointerType* create(Type* type);

    static PointerType* from_llvm_type(llvm::Type* type);
    llvm::PointerType* to_llvm_type(llvm::LLVMContext& context) override;
    
    PointerType* copy() override;

    std::string name() override;
    std::string str() override;

    Type* getElementType() { return this->type; }

    bool is_compatible(Type* other) override;
    bool is_compatible(llvm::Type* type) override;

private:
    PointerType(Type* type);

    Type* type;
};

#endif