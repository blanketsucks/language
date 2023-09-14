#include <quart/language/values.h>

using namespace quart;

Value::Value(
    llvm::Value* value, uint16_t flags, llvm::Any extra, llvm::Value* self
) : inner(value), type(nullptr), flags(flags), self(self), extra(extra) {
    if (!value) this->flags |= Value::Empty;
}

Value::Value(
    llvm::Value* value, quart::Type* type, uint16_t flags, llvm::Any extra, llvm::Value* self
) : inner(value), type(type), flags(flags), self(self), extra(extra) {
    if (!value) this->flags |= Value::Empty;
    assert(value && type && "When value is not null, type must not be null");
}

bool Value::is_reference() const {
    return this->type->is_reference();
}

bool Value::is_mutable() const {
    return this->type->is_mutable();
}

bool Value::is_empty_value() const {
    return this->flags & Value::Empty;
}