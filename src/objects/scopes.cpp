#include "objects/scopes.h"
#include "visitor.h"

ScopeLocal ScopeLocal::from_variable(const Variable& variable, bool use_store_value) {
    llvm::Value* value = use_store_value ? variable.value : variable.constant;
    return {
        variable.name,
        value,
        variable.type,
        false,
        variable.is_immutable,
        variable.is_stack_allocated,
        true, // Assume it always belongs to the current scope
    };    
}

ScopeLocal ScopeLocal::from_constant(const Constant& constant, bool use_store_value) {
    llvm::Value* value = use_store_value ? constant.store : constant.value;
    return {
        constant.name,
        value,
        constant.type,
        true,
        false,
        false,
        true, // Assume it always belongs to the current scope
    };
}

ScopeLocal ScopeLocal::from_scope_local(const ScopeLocal& local, llvm::Value* value, llvm::Type* type) {
    return {
        local.name,
        value,
        type ? type : local.type,
        false,
        local.is_immutable,
        local.is_stack_allocated,
        local.is_scope_local,
    };
}

ScopeLocal ScopeLocal::null() {
    return { "", nullptr, nullptr, false, false, false, false };
}

bool ScopeLocal::is_null() {
    return this->name.empty() && !this->value && !this->type;
}

llvm::Constant* ScopeLocal::get_constant_value() {
    if (!this->is_constant) {
        return nullptr;
    }

    if (llvm::isa<llvm::Constant>(this->value)) {
        return llvm::cast<llvm::Constant>(this->value);
    } else if (llvm::isa<llvm::GlobalVariable>(this->value)) {
        return llvm::cast<llvm::GlobalVariable>(this->value)->getInitializer();
    }

    return nullptr;
}

Scope::Scope(std::string name, ScopeType type, Scope* parent) : name(name), type(type), parent(parent) {
    this->namespaces = {};
    this->structs = {};
    this->functions = {};
    this->constants = {};
    this->enums = {};
    this->variables = {};
    this->modules = {};
}

bool Scope::has_namespace(std::string name) { 
    if (this->namespaces.find(name) != this->namespaces.end()) {
        return true;
    }

    if (this->parent) {
        return this->parent->has_namespace(name);
    }

    return false;
}

bool Scope::has_struct(std::string name) { 
    if (this->structs.find(name) != this->structs.end()) {
        return true;
    }

    if (this->parent) {
        return this->parent->has_struct(name);
    }

    return false;
}

bool Scope::has_function(std::string name) { 
    if (this->functions.find(name) != this->functions.end()) {
        return true;
    }

    if (this->parent) {
        return this->parent->has_function(name);
    }

    return false;
}

bool Scope::has_constant(std::string name) {
    if (this->constants.find(name) != this->constants.end()) {
        return true;
    }

    if (this->parent) {
        return this->parent->has_constant(name);
    }

    return false;
}

bool Scope::has_enum(std::string name) {
    if (this->enums.find(name) != this->enums.end()) {
        return true;
    }

    if (this->parent) {
        return this->parent->has_enum(name);
    }

    return false;
}

bool Scope::has_variable(std::string name) {
    if (this->variables.find(name) != this->variables.end()) {
        return true;
    }

    if (this->parent) {
        return this->parent->has_variable(name);
    }

    return false;
}

bool Scope::has_module(std::string name) {
    if (this->modules.find(name) != this->modules.end()) {
        return true;
    }

    if (this->parent) {
        return this->parent->has_module(name);
    }

    return false;
}

bool Scope::has_type(std::string name) {
    if (this->types.find(name) != this->types.end()) {
        return true;
    }

    if (this->parent) {
        return this->parent->has_type(name);
    }

    return false;
}

ScopeLocal Scope::get_local(std::string name, bool use_store_value) {
    if (this->variables.find(name) != this->variables.end()) {
        auto& variable = this->variables[name];
        return ScopeLocal::from_variable(variable, use_store_value);
    } else if (this->constants.find(name) != this->constants.end()) {
        auto& constant = this->constants[name];
        return ScopeLocal::from_constant(constant, use_store_value);
    }

    if (this->parent) {
        auto local = this->parent->get_local(name, use_store_value);
        local.is_scope_local = false;

        return local;
    } else {
        return ScopeLocal::null();
    }
}

Variable Scope::get_variable(std::string name) {
    if (this->variables.find(name) != this->variables.end()) {
        return this->variables[name];
    }

    if (this->parent) {
        return this->parent->get_variable(name);
    }
    
    return Variable::null();
}

Constant Scope::get_constant(std::string name) {
    if (this->constants.find(name) != this->constants.end()) {
        return this->constants[name];
    }

    if (this->parent) {
        return this->parent->get_constant(name);
    }
    
    return Constant::null();
}

utils::Ref<Function> Scope::get_function(std::string name) {
    if (this->functions.find(name) != this->functions.end()) {
        return this->functions[name];
    }

    if (this->parent) {
        return this->parent->get_function(name);
    }
    
    return nullptr;
}

utils::Ref<Struct> Scope::get_struct(std::string name) {
    if (this->structs.find(name) != this->structs.end()) {
        return this->structs[name];
    }

    if (this->parent) {
        return this->parent->get_struct(name);
    }
    
    return nullptr;
}

utils::Ref<Enum> Scope::get_enum(std::string name) {
    if (this->enums.find(name) != this->enums.end()) {
        return this->enums[name];
    }

    if (this->parent) {
        return this->parent->get_enum(name);
    }
    
    return nullptr;
}

utils::Ref<Module> Scope::get_module(std::string name) {
    if (this->modules.find(name) != this->modules.end()) {
        return this->modules[name];
    }

    if (this->parent) {
        return this->parent->get_module(name);
    }
    
    return nullptr;
}

utils::Ref<Namespace> Scope::get_namespace(std::string name) {
    if (this->namespaces.find(name) != this->namespaces.end()) {
        return this->namespaces[name];
    }

    if (this->parent) {
        return this->parent->get_namespace(name);
    }
    
    return nullptr;
}

TypeAlias Scope::get_type(std::string name) {
    if (this->types.find(name) != this->types.end()) {
        return this->types[name];
    }

    if (this->parent) {
        return this->parent->get_type(name);
    }
    
    return TypeAlias::null();
}

void Scope::exit(Visitor* visitor) {
    visitor->scope = this->parent;
}

void Scope::finalize(bool eliminate_dead_functions) {
    if (eliminate_dead_functions) {
        for (auto& entry : this->functions) {
            if (entry.second->is_finalized) {
                continue;
            }

            auto func = entry.second;
            if (!func->has_return()) {
                delete func->ret.block;
            }

            llvm::Function* function = func->value;
            if (function->use_empty() && !func->is_entry) {
                if (function->getParent()) {
                    function->eraseFromParent();
                }

                for (auto& call : func->calls) {
                    if (call->use_empty()) {
                        if (call->getParent()) {
                            call->eraseFromParent();
                        }
                    }
                }
            }
            
            for (auto branch : func->branches) delete branch;
            func->is_finalized = true;
        }
    }

    for (auto scope : this->children) {
        scope->finalize(eliminate_dead_functions); delete scope;
    }
}