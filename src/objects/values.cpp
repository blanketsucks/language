#include "objects/values.h"

#include "utils/log.h"

Value::Value(
    llvm::Value* value, 
    bool is_constant,
    llvm::Value* self
) {
    this->value = value;
    this->is_constant = is_constant;
    this->self = self;

    this->is_early_function_call = false;
    this->is_reference = false;
    this->is_immutable = false;
    this->is_stack_allocated = false;

    this->type = nullptr;
}

bool Value::is_null() {
    return !this->value;
}

llvm::Value* Value::unwrap(Span span) {
    if (!this->value) {
        ERROR(span, "Expected an expression");
    }

    return this->value;
}

Value Value::null() {
    return Value(nullptr);
}

Value Value::from_function(utils::Ref<Function> function, llvm::Value* self) {
    auto value = Value(function->value, false, self);
    value.function = function;

    return value;
}

Value Value::from_struct(utils::Ref<Struct> structure) {
    auto value = Value::null();
    value.structure = structure;

    return value;
}

Value Value::from_namespace(utils::Ref<Namespace> ns) {
    auto value = Value::null();
    value.namespace_ = ns;

    return value;
}

Value Value::from_enum(utils::Ref<Enum> enumeration) {
    auto value = Value::null();
    value.enumeration = enumeration;

    return value;
}

Value Value::from_module(utils::Ref<Module> module) {
    auto value = Value::null();
    value.module = module;

    return value;
}

Value Value::from_type(Type type) {
    auto value = Value::null();
    value.type = type;

    return value;
}

Value Value::as_early_function_call() {
    auto value = Value::null();
    value.is_early_function_call = true;

    return value;
}

Value Value::as_reference(llvm::Value* ref_value, bool is_immutable, bool is_stack_allocated) {
    auto value = Value(ref_value, false);

    value.is_reference = true;
    value.is_immutable = is_immutable;
    value.is_stack_allocated = is_stack_allocated;

    return value;
}