#include "objects/variables.h"

Variable Variable::from_alloca(
    std::string name, llvm::AllocaInst* alloca, bool is_immutable, Location start, Location end
) {
    return { 
        name, 
        alloca->getAllocatedType(), 
        alloca, 
        nullptr, 
        false, 
        is_immutable,
        true,
        start, 
        end
    };
}

Variable Variable::from_value(
    std::string name, 
    llvm::Value* value,
    bool is_immutable,
    bool is_reference,
    bool is_stack_allocated,
    Location start, 
    Location end
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
        start, 
        end
    };
}

Variable Variable::null() {
    return { "", nullptr, nullptr, nullptr, false, false, false, Location(), Location() };
}

bool Variable::is_null() {
    return this->name.empty() && !this->type && !this->value;
}

Constant Constant::null() {
    return { "", nullptr, nullptr, nullptr, Location(), Location() };
}

bool Constant::is_null() {
    return this->name.empty() && !this->type && !this->value;
}
