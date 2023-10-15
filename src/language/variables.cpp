#include <quart/language/variables.h>

using namespace quart;

Variable Variable::from_alloca(
    const std::string& name,
    llvm::AllocaInst* alloca,
    quart::Type* type,
    u8 flags,
    const Span& span
) {
    return Variable {
        name,
        type,
        alloca,
        nullptr,
        flags,
        span
    };
}

Variable Variable::from_value(
    const std::string& name,
    llvm::Value* value,
    quart::Type* type,
    u8 flags,
    const Span& span
) {
    if (type->is_pointer()) {
        type = type->get_pointee_type();
    } else if (type->is_reference()) {
        type = type->get_reference_type();
    }

    return Variable {
        name,
        type,
        value,
        nullptr,
        flags,
        span
    };
}