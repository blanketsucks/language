#include "objects.h"
#include "visitor.h"

Scope::Scope(std::string name, ScopeType type, Scope* parent) : name(name), type(type), parent(parent) {
    this->namespaces = {};
    this->structs = {};
    this->functions = {};
    this->constants = {};
    this->enums = {};
    this->variables = {};
    this->modules = {};
}

// Really repetitive code here. Not sure how to make it better.

bool Scope::has_namespace(std::string name) { 
    if (this->namespaces.find(name) == this->namespaces.end()) {
        if (this->parent) {
            return this->parent->has_namespace(name);
        }

        return false;
    }

    return true;
}

bool Scope::has_struct(std::string name) { 
    if (this->structs.find(name) == this->structs.end()) {
        if (this->parent) {
            return this->parent->has_struct(name);
        }

        return false;
    }

    return true;
}

bool Scope::has_function(std::string name) { 
    if (this->functions.find(name) == this->functions.end()) {
        if (this->parent) {
            return this->parent->has_function(name);
        }

        return false;
    }

    return true;
}

bool Scope::has_constant(std::string name) { 
    if (this->constants.find(name) == this->constants.end()) {
        if (this->parent) {
            return this->parent->has_constant(name);
        }

        return false;
    }

    return true;
}

bool Scope::has_enum(std::string name) { 
    if (this->enums.find(name) == this->enums.end()) {
        if (this->parent) {
            return this->parent->has_enum(name);
        }

        return false;
    }

    return true;
}

bool Scope::has_variable(std::string name) {
    if (this->variables.find(name) == this->variables.end()) {
        if (this->parent) {
            return this->parent->has_variable(name);
        }

        return false;
    }

    return true;
}

bool Scope::has_module(std::string name) {
    if (this->modules.find(name) == this->modules.end()) {
        if (this->parent) {
            return this->parent->has_module(name);
        }

        return false;
    }

    return true;
}

Scope::Local Scope::get_local(std::string name) {
    if (this->variables.find(name) != this->variables.end()) {
        return {this->variables[name], false};
    } else if (this->constants.find(name) != this->constants.end()) {
        return {this->constants[name], true};
    }

    if (this->parent) {
        return this->parent->get_local(name);
    } else {
        return {nullptr, false};
    }
}

llvm::Value* Scope::get_variable(std::string name) {
    if (this->variables.find(name) != this->variables.end()) {
        return this->variables[name];
    }

    if (this->parent) {
        return this->parent->get_variable(name);
    } else {
        return nullptr;
    }
}

llvm::Value* Scope::get_constant(std::string name) {
    if (this->constants.find(name) != this->constants.end()) {
        return this->constants[name];
    }

    if (this->parent) {
        return this->parent->get_constant(name);
    } else {
        return nullptr;
    }
}

utils::Shared<Function> Scope::get_function(std::string name) {
    if (this->functions.find(name) != this->functions.end()) {
        return this->functions[name];
    }

    if (this->parent) {
        return this->parent->get_function(name);
    } else {
        return nullptr;
    }
}

utils::Shared<Struct> Scope::get_struct(std::string name) {
    if (this->structs.find(name) != this->structs.end()) {
        return this->structs[name];
    }

    if (this->parent) {
        return this->parent->get_struct(name);
    } else {
        return nullptr;
    }
}

utils::Shared<Enum> Scope::get_enum(std::string name) {
    if (this->enums.find(name) != this->enums.end()) {
        return this->enums[name];
    }

    if (this->parent) {
        return this->parent->get_enum(name);
    } else {
        return nullptr;
    }
}

utils::Shared<Module> Scope::get_module(std::string name) {
    if (this->modules.find(name) != this->modules.end()) {
        return this->modules[name];
    }

    if (this->parent) {
        return this->parent->get_module(name);
    } else {
        return nullptr;
    }
}

utils::Shared<Namespace> Scope::get_namespace(std::string name) {
    if (this->namespaces.find(name) != this->namespaces.end()) {
        return this->namespaces[name];
    }

    if (this->parent) {
        return this->parent->get_namespace(name);
    } else {
        return nullptr;
    }
}

void Scope::exit(Visitor* visitor) {
    visitor->scope = this->parent;
}

void Scope::finalize() {
    for (auto& entry : this->functions) {
        if (entry.second->is_finalized) {
            continue;
        }

        auto func = entry.second;
        if (!func->has_return()) {
            delete func->return_block;
        }

        if (func->value->use_empty() && !func->is_entry) {
            func->value->eraseFromParent();
        }
        
        for (auto branch : func->branches) delete branch;
        func->is_finalized = true;
    }

    for (auto scope : this->children) {
        scope->finalize(); delete scope;
    }
}