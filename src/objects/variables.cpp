#include "objects/variables.h"

Variable Variable::from_alloca(
    std::string name, llvm::AllocaInst* alloca, bool is_immutable, Span span
) {
    return { 
        name, 
        alloca->getAllocatedType(), 
        alloca, 
        nullptr, 
        false, 
        is_immutable,
        true,
        span
    };
}

Variable Variable::from_value(
    std::string name, 
    llvm::Value* value,
    bool is_immutable,
    bool is_reference,
    bool is_stack_allocated,
    Span span
) {
    llvm::Type* type = value->getType();
    assert(type->isPointerTy());

    return { 
        name, 
        type->getPointerElementType(), 
        value, 
        nullptr, 
        is_reference, 
        is_immutable,
        is_stack_allocated,
        span
    };
}

Variable Variable::null() {
    return { "", nullptr, nullptr, nullptr, false, false, false, Span() };
}

bool Variable::is_null() {
    return this->name.empty() && !this->type && !this->value;
}

Constant Constant::null() {
    return { "", nullptr, nullptr, nullptr, Span() };
}

bool Constant::is_null() {
    return this->name.empty() && !this->type && !this->value;
}
