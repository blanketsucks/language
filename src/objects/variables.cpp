#include <quart/objects/variables.h>

Variable Variable::from_alloca(
    const std::string& name, llvm::AllocaInst* alloca, bool is_immutable, Span span
) {
    return { 
        .name = name, 
        .type = alloca->getAllocatedType(), 
        .value = alloca, 
        .constant = nullptr, 
        .is_reference = false, 
        .is_immutable = is_immutable,
        .is_stack_allocated = true,
        .is_used = false,
        .is_mutated = false,
        .span = span
    };
}

Variable Variable::from_value(
    const std::string& name, 
    llvm::Value* value,
    bool is_immutable,
    bool is_reference,
    bool is_stack_allocated,
    Span span
) {
    llvm::Type* type = value->getType();
    assert(type->isPointerTy());

    return { 
        .name = name, 
        .type = type->getPointerElementType(), 
        .value = value, 
        .constant = nullptr, 
        .is_reference = is_reference, 
        .is_immutable = is_immutable,
        .is_stack_allocated = is_stack_allocated,
        .is_used = false,
        .is_mutated = false,
        .span = span
    };
}

Variable Variable::null() {
    return { "", nullptr, nullptr, nullptr, false, false, false, false, false, Span() };
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
