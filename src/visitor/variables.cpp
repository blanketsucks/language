#include "visitor.h"

std::pair<llvm::Value*, bool> Visitor::get_variable(std::string name) {
    if (this->current_function) {
        llvm::Value* value = this->current_function->locals[name];
        if (value) {
            return std::make_pair(value, false);
        }

        value = this->current_function->constants[name];
        if (value) {
            return std::make_pair(value, true);
        }
    }

    return std::make_pair(nullptr, false);
}

Value Visitor::visit(ast::VariableExpr* expr) {
    if (this->structs.find(expr->name) != this->structs.end()) {
        return Value::with_struct(this->structs[expr->name]);
    } else if (this->namespaces.find(expr->name) != this->namespaces.end()) {
        return Value::with_namespace(this->namespaces[expr->name]);
    }

    if (this->current_function) {
        llvm::AllocaInst* variable = this->current_function->locals[expr->name];
        if (variable) {
            return this->builder->CreateLoad(variable->getAllocatedType(), variable);;
        }

        llvm::GlobalVariable* constant = this->current_function->constants[expr->name];
        if (constant) {
            return this->builder->CreateLoad(constant->getValueType(), constant);
        }
    }

    if (this->current_struct) {
        llvm::Value* value = this->current_struct->locals[expr->name];
        if (value) {
            return value;
        }
    }

    llvm::Constant* constant = this->constants[expr->name];
    if (constant) {
        llvm::GlobalVariable* global = this->module->getGlobalVariable(constant->getName());
        if (global) {
            return this->builder->CreateLoad(global->getValueType(), global);
        }

        return constant;
    }

    Value function = this->get_function(expr->name);
    if (function.function) {
        return function;
    }

    ERROR(expr->start, "Undefined variable '{s}'", expr->name); exit(1);
}

Value Visitor::visit(ast::VariableAssignmentExpr* expr) {
    llvm::Type* type;
    if (expr->external) {
        type = this->get_llvm_type(expr->type);
        this->module->getOrInsertGlobal(expr->name, type);

        llvm::GlobalVariable* global = this->module->getGlobalVariable(expr->name);
        global->setLinkage(llvm::GlobalValue::ExternalLinkage);

        return global;
    }

    llvm::Value* value = nullptr;
    bool is_constant = false;

    if (!expr->value) {
        type = this->get_llvm_type(expr->type);
        if (type->isAggregateType()) {
            value = llvm::ConstantAggregateZero::get(type);
        } else if (type->isPointerTy()) {
            value = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(type));
        } else {
            value = llvm::Constant::getNullValue(type);
        }
    } else {
        Value val = expr->value->accept(*this);
        is_constant = val.is_constant;

        value = val.unwrap(this, expr->start);
        if (!expr->type) {
            type = value->getType();
        } else {
            type = this->get_llvm_type(expr->type);
        }
    
        Type* ty = Type::from_llvm_type(type);
        if (!ty->is_compatible(value->getType())) {
            ERROR(expr->value->start, "Expected value of type '{t}' but got '{t}'", ty, Type::from_llvm_type(value->getType()));
        }
    }

    if (!this->current_function) {
        if (!is_constant) {
            ERROR(expr->start, "Initializer of global variable '{s}' must be a constant expression", expr->name); exit(1);
        }

        std::string name = "__const_" + expr->name;
        
        this->module->getOrInsertGlobal(name, type);
        llvm::GlobalVariable* variable = this->module->getNamedGlobal(name);

        variable->setLinkage(llvm::GlobalValue::ExternalLinkage);
        variable->setInitializer(llvm::cast<llvm::Constant>(value));

        this->constants[expr->name] = variable;
        return value;
    }

    llvm::Function* func = this->builder->GetInsertBlock()->getParent();

    if (is_constant) {
        std::string name = utils::fmt::format("__const_{s}.{s}", this->current_function->name, expr->name);
        this->module->getOrInsertGlobal(name, type);

        llvm::GlobalVariable* variable = this->module->getNamedGlobal(name);

        variable->setLinkage(llvm::GlobalValue::ExternalLinkage);
        variable->setInitializer((llvm::Constant*)value);

        this->current_function->constants[expr->name] = variable;

        llvm::AllocaInst* inst = this->create_alloca(func, type);

        llvm::Type* ty = this->builder->getInt8PtrTy();
        this->builder->CreateMemCpy(
            this->builder->CreateBitCast(inst, ty), llvm::MaybeAlign(0),
            this->builder->CreateBitCast(variable, ty), llvm::MaybeAlign(0),
            this->getsizeof(variable->getValueType())
        );

        this->current_function->locals[expr->name] = inst;
        return variable;
    }

    llvm::AllocaInst* alloca_inst = this->create_alloca(func, type);
    if (value) {
        this->builder->CreateStore(value, alloca_inst);
    }

    this->current_function->locals[expr->name] = alloca_inst;
    return value;
}

Value Visitor::visit(ast::ConstExpr* expr) {
    llvm::Value* value = expr->value->accept(*this).unwrap(this, expr->start);
    llvm::Type* type;

    if (!expr->type) {
        type = value->getType();
    } else {
        type = this->get_llvm_type(expr->type);
    }

    std::string name = expr->name;
    if (this->current_namespace) {
        name = this->current_namespace->name + "." + name;
    } else if (this->current_function) {
        name = this->current_function->name + "." + name;
    }

    name = "__const_" + name;
    this->module->getOrInsertGlobal(name, type);

    llvm::GlobalVariable* global = this->module->getNamedGlobal(name);
    global->setInitializer((llvm::Constant*)value);

    if (this->current_namespace) {
        this->current_namespace->locals[expr->name] = global;
    } else if (this->current_function) {
        this->current_function->constants[expr->name] = global;
    } else {
        this->constants[expr->name] = global;
    }

    return global;
}