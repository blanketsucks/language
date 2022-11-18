#include "objects.h"

Value::Value(
    llvm::Value* value, 
    bool is_constant, 
    llvm::Value* parent, 
    utils::Shared<Function> function,
    utils::Shared<Struct> structure,
    utils::Shared<Enum> enumeration,
    utils::Shared<Namespace> namespace_,
    utils::Shared<Module> module,
    llvm::Type* type,
    FunctionCall* call
) {
    this->value = value;
    this->is_constant = is_constant;
    this->parent = parent;
    this->function = function;
    this->structure = structure;
    this->namespace_ = namespace_;
    this->enumeration = enumeration;
    this->module = module;
    this->type = type;
    this->call = call;
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

Value Value::with_function(utils::Shared<Function> function) {
    function->used = true;
    return Value(function->value, false, nullptr, function);
}

Value Value::with_struct(utils::Shared<Struct> structure) {
    return Value(nullptr, false, nullptr, nullptr, structure);
}

Value Value::with_namespace(utils::Shared<Namespace> ns) {
    return Value(nullptr, false, nullptr, nullptr, nullptr, nullptr, ns);
}

Value Value::with_enum(utils::Shared<Enum> enumeration) {
    return Value(nullptr, false, nullptr, nullptr, nullptr, enumeration);
}

Value Value::with_module(utils::Shared<Module> module) {
    return Value(nullptr, false, nullptr, nullptr, nullptr, nullptr, nullptr, module);
}

Value Value::with_type(llvm::Type* type) {
    return Value(nullptr, false, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, type);
}

Value Value::as_call(FunctionCall* call) {
    return Value(nullptr, false, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, call);
}