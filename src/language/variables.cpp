#include <quart/language/variables.h>

using namespace quart;

Variable Variable::from_alloca(
    const std::string& name,
    llvm::AllocaInst* alloca,
    quart::Type* type,
    uint8_t flags,
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
    uint8_t flags,
    const Span& span
) {
    return Variable {
        name,
        type->get_pointee_type(),
        value,
        nullptr,
        flags,
        span
    };
}