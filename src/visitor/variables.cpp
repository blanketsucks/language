#include "visitor.h"

Value Visitor::visit(ast::VariableExpr* expr) {
    if (expr->name == "null") {
        llvm::Type* type = this->ctx != nullptr ? this->ctx : this->builder->getInt1Ty();
        return Value(llvm::Constant::getNullValue(type), true);
    }

    Scope* scope = this->scope;
    auto local = scope->get_local(expr->name, bool(this->current_function));
    if (local.value) {
        if (!this->current_function) {
            return Value(local.value, local.is_constant);
        }

        if (local.type->isArrayTy()) {
            return this->builder->CreateGEP(local.type, local.value, this->builder->getInt32(0));
        }
            
        return Value(this->load(local.value, local.type), local.is_constant);
    }

    if (scope->has_struct(expr->name)) {
        return Value::from_struct(scope->get_struct(expr->name));
    } else if (scope->has_namespace(expr->name)) {
        return Value::from_namespace(scope->get_namespace(expr->name));
    } else if (scope->has_enum(expr->name)) {
        return Value::from_enum(scope->get_enum(expr->name));
    } else if (scope->has_function(expr->name)) {
        return Value::from_function(scope->get_function(expr->name));
    } else if (scope->has_module(expr->name)) {
        return Value::from_module(scope->get_module(expr->name));
    }

    ERROR(expr->span, "Undefined variable '{0}'", expr->name);
}

Value Visitor::visit(ast::VariableAssignmentExpr* expr) {
    llvm::Type* type = nullptr;
    if (expr->external) {
        std::string name = expr->names[0];

        type = expr->type->accept(*this).type.value;
        this->module->getOrInsertGlobal(name, type);

        llvm::GlobalVariable* global = this->module->getGlobalVariable(name);
        global->setLinkage(llvm::GlobalValue::ExternalLinkage);

        return global;
    }

    llvm::Value* value = nullptr;

    bool is_early_function_call = false;
    bool is_constant = false;
    bool is_reference = false;
    bool is_immutable = false;
    bool is_stack_allocated = false;
    bool has_initializer = !!expr->value;

    if (!expr->value) {
        type = expr->type->accept(*this).type.value;
        if (type->isAggregateType()) {
            value = llvm::ConstantAggregateZero::get(type);
        } else if (type->isPointerTy()) {
            value = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(type));
        } else {
            value = llvm::Constant::getNullValue(type);
        }

        is_constant = true;
    } else {
        if (expr->type) {
            type = expr->type->accept(*this).type.value;
            this->ctx = type;
        }

        Value val = expr->value->accept(*this); 

        is_constant = val.is_constant;
        is_early_function_call = val.is_early_function_call;
        is_reference = val.is_reference;
        is_immutable = val.is_immutable;
        is_stack_allocated = val.is_stack_allocated;

        llvm::Type* vtype = nullptr;
        if (!is_early_function_call) {
            value = val.unwrap(expr->value->span);
            vtype = value->getType();

            if (!type) {
                type = vtype;
            }
        } else {
            vtype = this->constructors.back().function->getReturnType();
            if (!type) {
                type = vtype;
            }

            value = llvm::Constant::getNullValue(type);
        }

        if (type->isVoidTy()) {
            ERROR(expr->value->span, "Cannot store value of type 'void'");
        }

        if (!this->is_compatible(type, vtype)) {
            ERROR(
                expr->value->span, 
                "Expected expression of type '{0}' but got '{1}' instead", 
                this->get_type_name(type), this->get_type_name(vtype)
            );
        } else {
            // TODO: Somehow be able to cast in this case
            if (is_early_function_call && (type != vtype)) {
                ERROR(
                    expr->value->span, 
                    "Expected expression of type '{0}' but got '{1}' instead", 
                    this->get_type_name(type), this->get_type_name(vtype)
                );
            } else {
                value = this->cast(value, type);
            }
        }
    }

    if (!expr->is_multiple_variables) {
        std::string first = expr->names[0];
        if (!this->current_function) {
            if (!llvm::isa<llvm::Constant>(value)) {
                ERROR(expr->value->span, "Cannot store non-constant value in a global variable");
            }

            std::string name = FORMAT("__global.{0}", first);
            this->module->getOrInsertGlobal(name, type);

            llvm::GlobalVariable* global = this->module->getGlobalVariable(name);
            llvm::Constant* constant = llvm::cast<llvm::Constant>(value);

            global->setInitializer(constant);
            if (!is_early_function_call) {
                global->setLinkage(llvm::GlobalValue::PrivateLinkage);
            } else {
                auto& call = this->constructors.back();
                call.store = global;
            }

            this->scope->variables[first] = Variable { 
                first, 
                type, 
                global, 
                constant, 
                false,
                expr->is_immutable,
                false,
                expr->span
            };

            return nullptr;
        }

        if (is_reference) {
            if (expr->is_immutable && !is_immutable) {
                ERROR(expr->span, "Cannot assign mutable reference to immutable reference variable '{0}'", first);
            }

            this->scope->variables[first] = Variable::from_value(
                first, 
                value, 
                expr->is_immutable, 
                true, 
                is_stack_allocated, 
                expr->span
            );

            return nullptr;
        }

        llvm::AllocaInst* inst = this->create_alloca(type);
        if (is_constant && type->isAggregateType() && has_initializer) {
            std::string name = FORMAT("__const.{0}.{1}", this->current_function->name, first);
            this->module->getOrInsertGlobal(name, type);

            llvm::GlobalVariable* global = this->module->getGlobalVariable(name);
            
            global->setLinkage(llvm::GlobalVariable::PrivateLinkage);
            global->setInitializer(llvm::cast<llvm::Constant>(value));

            this->builder->CreateMemCpy(
                inst, llvm::MaybeAlign(0),
                global, llvm::MaybeAlign(0),
                this->getsizeof(type)
            );
        } else {
            if (!has_initializer) {
                this->builder->CreateMemSet(
                    inst, this->builder->getInt8(0), 
                    this->getsizeof(type), llvm::MaybeAlign(0)
                );
            } else {
                this->builder->CreateStore(value, inst);
            }

            if (is_early_function_call) {
                auto& call = this->constructors.back();
                call.store = inst;
            }
        }

        this->scope->variables[first] = Variable {
            first, 
            type, 
            inst, 
            is_constant ? llvm::cast<llvm::Constant>(value) : nullptr, 
            false, 
            expr->is_immutable,
            true,
            expr->span
        };

        if (auto structure = this->get_struct(type)) {
            if (!structure->has_method("destructor")) {
                return nullptr;
            }

            this->current_function->dtors.push_back({
                inst, structure.get()
            });
        }

    } else {
        this->store_tuple(expr->span, this->current_function, value, expr->names, expr->consume_rest);
    }
    
    return value;
}

Value Visitor::visit(ast::ConstExpr* expr) {
    Value val = expr->value->accept(*this);
    bool is_early_function_call = val.is_early_function_call;

    llvm::Type* type = nullptr;
    llvm::Value* value = nullptr;

    if (!is_early_function_call) {
        value = val.unwrap(expr->value->span);
        if (!expr->type) {
            type = value->getType();
        } else {
            type = expr->type->accept(*this).type.value;
        }
    } else {
        type = this->constructors.back().function->getReturnType();
    }

    std::string name = expr->name;
    if (this->current_namespace) {
        name = this->current_namespace->name + "." + name;
    } else if (this->current_function) {
        name = this->current_function->name + "." + name;
    }

    name = "__const." + name;

    this->module->getOrInsertGlobal(name, type);
    llvm::GlobalVariable* global = this->module->getNamedGlobal(name);

    llvm::Constant* constant = nullptr;
    if (!is_early_function_call) {
        constant = llvm::cast<llvm::Constant>(value);
    } else {
        constant = llvm::Constant::getNullValue(type);
    }

    global->setInitializer(constant);
    if (!is_early_function_call) {
        global->setLinkage(llvm::GlobalVariable::PrivateLinkage);
    } else {
        global->setDSOLocal(true);

        auto& call = this->constructors.back();
        call.store = global;
    }

    this->scope->constants[expr->name] = Constant {
        name, type, global, constant, expr->span
    };
    
    return nullptr;
}