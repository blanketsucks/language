#include <quart/visitor.h>

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

        return this->builder->CreateLoad(local.type, local.value);
    }

    if (scope->has_struct(expr->name)) {
        return Value::from_struct(scope->get_struct(expr->name));
    } else if (scope->has_namespace(expr->name)) {
        auto ns = scope->get_namespace(expr->name);
        return Value::from_scope(ns->scope);
    } else if (scope->has_enum(expr->name)) {
        auto enumeration = scope->get_enum(expr->name);
        return Value::from_scope(enumeration->scope);
    } else if (scope->has_function(expr->name)) {
        return Value::from_function(scope->get_function(expr->name));
    } else if (scope->has_module(expr->name)) {
        auto module = scope->get_module(expr->name);
        return Value::from_scope(module->scope);
    }

    if (this->builtins.count(expr->name)) {
        return Value::from_builtin(this->builtins[expr->name]);
    }

    ERROR(expr->span, "Undefined variable '{0}'", expr->name);
}

Value Visitor::visit(ast::VariableAssignmentExpr* expr) {
    llvm::Type* type = nullptr;
    if (expr->external) {
        std::string name = expr->names[0].value;

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
    bool is_aggregate = false;
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
        is_aggregate = val.is_aggregate;

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
        ast::Ident& ident = expr->names[0];
        if (!this->current_function) {
            if (!llvm::isa<llvm::Constant>(value)) {
                ERROR(expr->value->span, "Cannot store non-constant value in a global variable");
            }

            std::string name = FORMAT("__global.{0}", ident.value);
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

            this->scope->variables[ident.value] = Variable { 
                ident.value, 
                type, 
                global, 
                constant, 
                false,
                ident.is_immutable,
                false,
                false,
                false,
                expr->span
            };

            return nullptr;
        }

        if (is_reference) {
            if (ident.is_immutable && !is_immutable) {
                ERROR(expr->span, "Cannot assign mutable reference to immutable reference variable '{0}'", ident.value);
            }

            this->scope->variables[ident.value] = Variable::from_value(
                ident.value, 
                value, 
                ident.is_immutable, 
                true, 
                is_stack_allocated, 
                ident.span
            );

            return nullptr;
        }

        llvm::Value* alloca = this->alloca(type);
        if (is_constant && type->isAggregateType() && has_initializer) {
            std::string name = FORMAT("__const.{0}.{1}", this->current_function->name, ident.value);
            this->module->getOrInsertGlobal(name, type);

            llvm::GlobalVariable* global = this->module->getGlobalVariable(name);
            
            global->setLinkage(llvm::GlobalVariable::PrivateLinkage);
            global->setInitializer(llvm::cast<llvm::Constant>(value));

            this->builder->CreateMemCpy(
                alloca, llvm::MaybeAlign(0),
                global, llvm::MaybeAlign(0),
                this->getsizeof(type)
            );
        } else {
            if (!has_initializer) {
                this->builder->CreateMemSet(
                    alloca, this->builder->getInt8(0), 
                    this->getsizeof(type), llvm::MaybeAlign(0)
                );
            } else if (is_aggregate) {
                alloca = value;
            } else {
                this->builder->CreateStore(value, alloca);
            }

            if (is_early_function_call) {
                auto& call = this->constructors.back();
                call.store = alloca;
            }
        }

        this->scope->variables[ident.value] = Variable {
            ident.value,
            type,
            alloca,
            is_constant ? llvm::cast<llvm::Constant>(value) : nullptr,
            false,
            ident.is_immutable,
            true,
            false,
            false,
            ident.span
        };
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