#include "visitor.h"

Value Visitor::visit(ast::VariableExpr* expr) {
    Scope* scope = this->scope;
    auto local = scope->get_local(expr->name);
    if (local.first) {
        return Value(this->load(local.first), local.second);
    }

    if (scope->has_struct(expr->name)) {
        return Value::with_struct(scope->get_struct(expr->name));
    } else if (scope->has_namespace(expr->name)) {
        return Value::with_namespace(scope->get_namespace(expr->name));
    } else if (scope->has_enum(expr->name)) {
        return Value::with_enum(scope->get_enum(expr->name));
    } else if (scope->has_function(expr->name)) {
        return Value::with_function(scope->get_function(expr->name));
    } else if (scope->has_module(expr->name)) {
        return Value::with_module(scope->get_module(expr->name));
    }

    if (expr->name == "null") {
        llvm::Type* type = this->ctx != nullptr ? this->ctx : this->builder->getInt1Ty();
        return llvm::Constant::getNullValue(type);
    }

    ERROR(expr->start, "Undefined variable '{0}'", expr->name);
}

Value Visitor::visit(ast::VariableAssignmentExpr* expr) {
    llvm::Type* type = nullptr;
    if (expr->external) {
        std::string name = expr->names[0];

        type = expr->type->accept(*this).type;
        this->module->getOrInsertGlobal(name, type);

        llvm::GlobalVariable* global = this->module->getGlobalVariable(name);
        global->setLinkage(llvm::GlobalValue::ExternalLinkage);

        return global;
    }

    llvm::Value* value = nullptr;
    FunctionCall* call = nullptr;

    bool is_constant = false;
    if (!expr->value) {
        type = expr->type->accept(*this).type;
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
            type = expr->type->accept(*this).type;
            this->ctx = type;
        }

        Value val = expr->value->accept(*this); is_constant = val.is_constant;
        call = val.call;

        if (!call) {
            value = val.unwrap(expr->value->start);
            if (!expr->type) {
                type = value->getType();
            }
        } else {
            type = call->function->getReturnType();
        }

        if (type->isVoidTy()) {
            ERROR(expr->value->start, "Cannot store value of type 'void'");
        }
        
        if (!this->is_compatible(type, value->getType())) {
            ERROR(
                expr->value->start, 
                "Expected expression of type '{0}' but got '{1}' instead", 
                this->get_type_name(type), this->get_type_name(value->getType())
            );
        } else {
            value = this->cast(value, type);
        }
    }

    if (!expr->is_multiple_variables) {
        if (!this->current_function) {
            if (!is_constant) {
                ERROR(expr->start, "Cannot store non-constant value in global scope");
            }

            std::string name = FORMAT("__global.{0}", expr->names[0]);
            this->module->getOrInsertGlobal(name, type);

            llvm::GlobalVariable* global = this->module->getGlobalVariable(name);

            if (!call) {
                global->setLinkage(llvm::GlobalValue::PrivateLinkage);
                global->setInitializer(llvm::cast<llvm::Constant>(value));
            } else {
                global->setDSOLocal(true);
                global->setInitializer(llvm::Constant::getNullValue(type));

                call->store = global;
                this->constructors.push_back(call);
            }

            this->scope->variables[expr->names[0]] = global;
            return value;
        }

        llvm::AllocaInst* inst = this->create_alloca(type);
        if (is_constant && type->isAggregateType()) {
            std::string name = FORMAT("__const.{0}.{1}", this->current_function->name, expr->names[0]);
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
            if (value) { this->builder->CreateStore(value, inst); }

            if (call) {
                call->store = inst;
                this->constructors.push_back(call);
            }
        }

        this->scope->variables[expr->names[0]] = inst;

    } else {
        this->store_tuple(expr->start, this->current_function, value, expr->names, expr->consume_rest);
    }
    
    return value;
}

Value Visitor::visit(ast::ConstExpr* expr) {
    Value val = expr->value->accept(*this);
    FunctionCall* call = val.call;

    llvm::Type* type = nullptr;
    llvm::Value* value = nullptr;

    if (!call) {
        value = val.unwrap(expr->value->start);
        if (!expr->type) {
            type = value->getType();
        } else {
            type = expr->type->accept(*this).type;
        }
    } else {
        type = call->function->getReturnType();
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

    if (!call) {
        global->setLinkage(llvm::GlobalVariable::PrivateLinkage);
        global->setInitializer(llvm::cast<llvm::Constant>(value));
    } else {
        global->setDSOLocal(true);
        global->setInitializer(llvm::Constant::getNullValue(type));

        call->store = global;
        this->constructors.push_back(call);
    }

    this->scope->constants[expr->name] = global;
    return nullptr;
}