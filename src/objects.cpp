#include "objects.h"

#include "visitor.h"
#include "utils.h"

// Functions

Function::Function(
    std::string name,
    std::vector<llvm::Type*> args,
    llvm::Type* ret,
    bool is_intrinsic,
    ast::Attributes attrs
) : name(name), args(args), ret(ret), is_intrinsic(is_intrinsic) {
    this->has_return = false;
    this->used = false;
    this->calls = {};
    this->attrs = attrs;
}
// Structures

Struct::Struct(
    std::string name,
    bool opaque,
    llvm::StructType* type,
    std::map<std::string, llvm::Type*> fields
) : name(name), opaque(opaque), type(type), fields(fields) {
    this->methods = {};
    this->locals = {};
}

bool Struct::has_method(const std::string& name) { 
    return this->methods.find(name) != this->methods.end(); 
}

int Struct::get_field_index(std::string name) {
    auto iter = this->fields.find(name);
    if (iter == this->fields.end()) {
        return -1;
    }

    return std::distance(this->fields.begin(), iter);
}

llvm::Value* Struct::get_variable(std::string name) {
    auto iter = this->locals.find(name);
    if (iter == this->locals.end()) {
        return nullptr;
    }

    return iter->second;
}

// Namespaces

Namespace::Namespace(std::string name) : name(name) {
    this->structs = {};
    this->functions = {};
    this->namespaces = {};
    this->locals = {};
}

// Values

Value::Value(
    llvm::Value* value, llvm::Value* parent, Function* function, Struct* structure, Namespace* ns
) {
    this->value = value;
    this->parent = parent;
    this->function = function;
    this->structure = structure;
    this->ns = ns;
}

llvm::Value* Value::unwrap(Visitor* visitor, Location location) {
    if (!this->value) {
        ERROR(location, "Value is null");
    }

    return this->value;
}

llvm::Type* Value::type() {
    return this->value->getType();
}

std::string Value::name() {
    return this->value->getName().str();
}

Value Value::with_function(llvm::Value* value, Function* function) {
    return Value(value, nullptr, function);
}

Value Value::with_struct(Struct* structure) {
    return Value(nullptr, nullptr, nullptr, structure);
}

Value Value::with_namespace(Namespace* ns) {
    return Value(nullptr, nullptr, nullptr, nullptr, ns);
}