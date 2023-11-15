#include <quart/language/scopes.h>
#include <quart/visitor.h>

#include <algorithm>

#define HAS_VALUE(map, method)                           \
    if (this->map.find(name) != this->map.end()) {       \
        return true;                                     \
    }                                                    \
                                                         \
    if (this->parent) {                                  \
        return this->parent->method(name);               \
    }                                                    \
                                                         \
    return false                                         \


#define GET_VALUE(map, method)                           \
    auto iterator = this->map.find(name);                \
    if (iterator != this->map.end()) {                   \
        return iterator->second;                         \
    }                                                    \
                                                         \
    if (this->parent) {                                  \
        return this->parent->method(name);               \
    }                                                    \
                                                         \
    return nullptr                                       \

#define GET_VALUE_REF(map, method)                        \
    auto iterator = this->map.find(name);                 \
    if (iterator != this->map.end()) {                    \
        return &iterator->second;                         \
    }                                                     \
                                                          \
    if (this->parent) {                                   \
        return this->parent->method(name);                \
    }                                                     \
                                                          \
    return nullptr                                        \


using namespace quart;

ScopeLocal ScopeLocal::from_variable(const Variable& variable, bool use_store_value) {
    llvm::Value* value = use_store_value ? variable.value : variable.constant;
    u8 flags = ScopeLocal::LocalToScope;

    if (variable.flags & Variable::StackAllocated) flags |= ScopeLocal::StackAllocated;
    if (variable.flags & Variable::Mutable) flags |= ScopeLocal::Mutable;

    return ScopeLocal {
        variable.name,
        value,
        variable.type,
        flags
    };
}

ScopeLocal ScopeLocal::from_constant(const struct Constant& constant, bool use_store_value) {
    llvm::Value* value = use_store_value ? constant.store : constant.value;
    return {
        constant.name,
        value,
        constant.type,
        ScopeLocal::Constant | ScopeLocal::LocalToScope
    };
}

ScopeLocal ScopeLocal::from_scope_local(const ScopeLocal& local, llvm::Value* value, quart::Type* type) {
    return {
        local.name,
        value,
        type ? type : local.type,
        local.flags
    };
}

ScopeLocal ScopeLocal::null() {
    return { "", nullptr, nullptr, 0 };
}

bool ScopeLocal::is_null() {
    return this->name.empty() && !this->value && !this->type;
}

llvm::Constant* ScopeLocal::get_constant_value() {
    if (!this->is_constant()) {
        return nullptr;
    }

    if (llvm::isa<llvm::Constant>(this->value)) {
        return llvm::cast<llvm::Constant>(this->value);
    } else if (llvm::isa<llvm::GlobalVariable>(this->value)) {
        return llvm::cast<llvm::GlobalVariable>(this->value)->getInitializer();
    }

    return nullptr;
}

Scope* Scope::create(const std::string& name, ScopeType type, Scope* parent) {
    Scope* scope = new Scope(name, type, parent);
    if (parent) parent->add_child(scope);

    return scope;
}

bool Scope::has_struct(const std::string& name) { HAS_VALUE(structs, has_struct); }
bool Scope::has_function(const std::string& name) { HAS_VALUE(functions, has_function); }
bool Scope::has_constant(const std::string& name) { HAS_VALUE(constants, has_constant); }
bool Scope::has_enum(const std::string& name) { HAS_VALUE(enums, has_enum); }
bool Scope::has_variable(const std::string& name) { HAS_VALUE(variables, has_variable); }
bool Scope::has_module(const std::string& name) { HAS_VALUE(modules, has_module); }
bool Scope::has_type_alias(const std::string& name) { HAS_VALUE(type_aliases, has_type_alias); }

ScopeLocal Scope::get_local(const std::string& name, bool use_store_value) {
    if (this->variables.find(name) != this->variables.end()) {
        auto& variable = this->variables[name];
        variable.flags |= Variable::Used; // TODO: Track this in a better way

        return ScopeLocal::from_variable(variable, use_store_value);
    } else if (this->constants.find(name) != this->constants.end()) {
        auto& constant = this->constants[name];
        return ScopeLocal::from_constant(constant, use_store_value);
    }

    if (this->parent) {
        auto local = this->parent->get_local(name, use_store_value);
        local.flags &= ~ScopeLocal::LocalToScope;

        return local;
    } else {
        return ScopeLocal::null();
    }
}

Variable* Scope::get_variable(const std::string& name) { GET_VALUE_REF(variables, get_variable); }
Constant* Scope::get_constant(const std::string& name) { GET_VALUE_REF(constants, get_constant); }
TypeAlias* Scope::get_type_alias(const std::string& name) { GET_VALUE_REF(type_aliases, get_type_alias); }

RefPtr<Function> Scope::get_function(const std::string& name) { GET_VALUE(functions, get_function); }
RefPtr<Struct> Scope::get_struct(const std::string& name) { GET_VALUE(structs, get_struct); }
RefPtr<Enum> Scope::get_enum(const std::string& name) { GET_VALUE(enums, get_enum); }
RefPtr<Module> Scope::get_module(const std::string& name) { GET_VALUE(modules, get_module); }

static void finalize(Function& func) {
    if (func.flags & Function::Finalized) return;

    llvm::Function* function = func.value;
    if (function->use_empty() && !func.is_entry()) {
        if (function->getParent()) {
            function->eraseFromParent();
        }

        for (auto& call : func.calls) {
            if (!call->use_empty() && !call->getParent()) {
                continue;
            }

            call->eraseFromParent();
        }
    }

    func.flags |= Function::Finalized;
}

void Scope::finalize(bool eliminate_dead_functions) {
    if (eliminate_dead_functions) {
        for (auto& entry : this->functions) {
            if (!entry.second) continue;
            ::finalize(*entry.second);
        }
    }

    for (auto scope : this->children) {
        scope->finalize(eliminate_dead_functions);
        delete scope;
    }
}