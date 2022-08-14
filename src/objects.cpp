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
    this->used = false;
    this->is_private = attrs.has("private");
    this->attrs = attrs;
    
    this->calls = {};
    this->defers = {};
    this->locals = {};
}

Branch* Function::create_branch(std::string name) {
    Branch* branch = new Branch(name, false);
    this->branches.push_back(branch);

    return branch;
}

bool Function::has_return() {
    for (Branch* branch : this->branches) {
        if (branch->has_return) {
            return true;
        }
    }

    return false;
}

void Function::defer(Visitor& visitor) {
    for (auto expr : this->defers) {
        expr->accept(visitor);
    }
}

// Structures

Struct::Struct(
    std::string name,
    bool opaque,
    llvm::StructType* type,
    std::map<std::string, StructField> fields
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

std::vector<StructField> Struct::get_fields(bool with_private) {
    std::vector<StructField> fields;
    for (auto pair : this->fields) {
        if (pair.second.is_private) {
            if (with_private) {
                fields.push_back(pair.second);
            }   
        } else {
            fields.push_back(pair.second);
        }
    }

    return fields;
}

llvm::Value* Struct::get_variable(std::string name) {
    auto iter = this->locals.find(name);
    if (iter == this->locals.end()) {
        return nullptr;
    }

    return iter->second;
}

std::vector<Struct*> Struct::expand() {
   std::vector<Struct*> parents;

    parents.push_back(this);
    for (Struct* parent : this->parents) {
        parents.push_back(parent);

        std::vector<Struct*> expanded = parent->expand();
        parents.insert(parents.end(), expanded.begin(), expanded.end());
    }

    return parents;
}

// Namespaces

Namespace::Namespace(std::string name) : name(name) {
    this->structs = {};
    this->functions = {};
    this->namespaces = {};
    this->locals = {};
}

// Modules

// Values

Value::Value(
    llvm::Value* value, bool is_constant, llvm::Value* parent, Function* function, Struct* structure, Namespace* ns
) {
    this->value = value;
    this->is_constant = is_constant;
    this->parent = parent;
    this->function = function;
    this->structure = structure;
    this->ns = ns;
}

llvm::Value* Value::unwrap(Visitor* visitor, Location location) {
    if (!this->value) {
        ERROR(location, "Expected an expression");
    }

    return this->value;
}

llvm::Type* Value::type() {
    return this->value->getType();
}

std::string Value::name() {
    return this->value->getName().str();
}

Value Value::with_function(Function* function) {
    return Value(function->value, false, nullptr, function);
}

Value Value::with_struct(Struct* structure) {
    return Value(nullptr, false, nullptr, nullptr, structure);
}

Value Value::with_namespace(Namespace* ns) {
    return Value(nullptr, false, nullptr, nullptr, nullptr, ns);
}