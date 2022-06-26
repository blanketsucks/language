#include "values.h"

#include "visitor.h"

// Functions

Function::Function(
    std::string name,
    std::vector<llvm::Type*> args,
    llvm::Type* ret,
    bool is_intrinsic
) : name(name), args(args), ret(ret), is_intrinsic(is_intrinsic) {
    this->has_return = false;
    this->used = false;
}

// Structures

Struct::Struct(
    std::string name,
    llvm::StructType* type,
    std::map<std::string, llvm::Type*> fields
) : name(name), type(type), fields(fields) {
    this->methods = {};
    this->locals = {};
    this->constructor = nullptr;
}

bool Struct::has_method(const std::string& name) { 
    return this->methods.find(name) != this->methods.end(); 
}

int Struct::get_attribute(std::string name) {
    auto iter = this->fields.find(name);
    if (iter == this->fields.end()) {
        return -1;
    }

    return std::distance(this->fields.begin(), iter);
}

// Modules

bool Module::is_ready() {
    return this->state == ModuleState::Compiled;
}

// Namespaces

Namespace::Namespace(std::string name) : name(name) {
    this->structs = {};
    this->functions = {};
    this->namespaces = {};
}

// Values

Value::Value(
    llvm::Value* value, llvm::Value* parent, Struct* structure, Namespace* ns
) {
    this->value = value;
    this->parent = parent;
    this->structure = structure;
    this->ns = ns;
}

llvm::Value* Value::unwrap(Visitor* visitor, Location location) {
    if (!this->value) {
        visitor->error("Invalid operand type", location);
    }

    return this->value;
}

llvm::Type* Value::type() {
    return this->value->getType();
}

std::string Value::name() {
    return this->value->getName().str();
}