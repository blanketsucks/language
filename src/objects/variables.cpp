#include "objects.h"

Variable Variable::from_alloca(std::string name, llvm::AllocaInst* alloca, Location start, Location end) {
    return { name, alloca->getAllocatedType(), alloca, nullptr, false, start, end };
}

Variable Variable::from_value(std::string name, llvm::Value* value, Location start, Location end) {
    llvm::Type* type = value->getType();
    assert(type->isPointerTy());

    return { name, type->getNonOpaquePointerElementType(), value, nullptr, false, start, end };
}

Variable Variable::empty() {
    return { "", nullptr, nullptr, nullptr, false, Location(), Location() };
}

bool Variable::is_empty() {
    return this->name.empty() && !this->type && !this->value;
}

Constant Constant::empty() {
    return { "", nullptr, nullptr, nullptr, Location(), Location() };
}

bool Constant::is_empty() {
    return this->name.empty() && !this->type && !this->value;
}
