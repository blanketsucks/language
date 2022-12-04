#include "objects.h"

Value::Value(
    llvm::Value* value, 
    bool is_constant,
    bool is_reference,
    bool is_early_function_call,
    llvm::Value* parent, 
    llvm::Type* type
) {
    this->value = value;
    this->is_constant = is_constant;
    this->parent = parent;
    this->type = type;
    this->is_reference = is_reference;
    this->is_early_function_call = is_early_function_call;
}

llvm::Value* Value::unwrap(Location location) {
    if (!this->value) {
        ERROR(location, "Expected an expression");
    }

    return this->value;
}

std::string Value::name() {
    assert(this->value);
    return this->value->getName().str();
}

Value Value::with_function(utils::Shared<Function> function, llvm::Value* parent) {
    auto value = Value(function->value, false, false, false, parent);
    value.function = function;

    return value;
}

Value Value::with_struct(utils::Shared<Struct> structure) {
    auto value = Value(nullptr);
    value.structure = structure;

    return value;
}

Value Value::with_namespace(utils::Shared<Namespace> ns) {
    auto value = Value(nullptr);
    value.namespace_ = ns;

    return value;
}

Value Value::with_enum(utils::Shared<Enum> enumeration) {
    auto value = Value(nullptr);
    value.enumeration = enumeration;

    return value;
}

Value Value::with_module(utils::Shared<Module> module) {
    auto value = Value(nullptr);
    value.module = module;

    return value;
}

Value Value::with_type(llvm::Type* type) {
    return Value(nullptr, false, false, false, nullptr, type);
}

Value Value::as_reference(llvm::Value *value, llvm::Value *parent) {
    return Value(value, false, true, false, parent);
}