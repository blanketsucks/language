#include "objects.h"

Value::Value(
    llvm::Value* value, 
    bool is_constant, 
    llvm::Value* parent, 
    Function* function, 
    Struct* structure, 
    Namespace* ns,
    Enum* enumeration,
    Module* module,
    FunctionCall* call
) {
    this->value = value;
    this->is_constant = is_constant;
    this->parent = parent;
    this->function = function;
    this->structure = structure;
    this->ns = ns;
    this->enumeration = enumeration;
    this->module = module;
    this->call = call;
}

llvm::Value* Value::unwrap(Location location) {
    if (!this->value) {
        ERROR(location, "Expected an expression");
    }

    return this->value;
}

llvm::Type* Value::type() {
    assert(this->value);
    return this->value->getType();
}

std::string Value::name() {
    assert(this->value);
    return this->value->getName().str();
}

Value Value::with_function(Function* function) {
    function->used = true;
    return Value(function->value, false, nullptr, function);
}

Value Value::with_struct(Struct* structure) {
    return Value(nullptr, false, nullptr, nullptr, structure);
}

Value Value::with_namespace(Namespace* ns) {
    return Value(nullptr, false, nullptr, nullptr, nullptr, ns);
}

Value Value::with_enum(Enum* enumeration) {
    return Value(nullptr, false, nullptr, nullptr, nullptr, nullptr, enumeration);
}

Value Value::with_module(Module* module) {
    return Value(nullptr, false, nullptr, nullptr, nullptr, nullptr, nullptr, module);
}

Value Value::as_call(FunctionCall* call) {
    return Value(nullptr, false, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, call);
}