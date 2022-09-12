#include "visitor.h"

void Visitor::store_tuple(Location location, Function *func, llvm::Value *value, std::vector<std::string> names) {
    std::vector<llvm::Value*> values = this->unpack(location, value, names.size());
    for (auto& pair : utils::zip(names, values)) {
        llvm::AllocaInst* inst = this->create_alloca(func->value, pair.second->getType());
        this->builder->CreateStore(pair.second, inst);

        func->locals[pair.first] = inst;
    }
}

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
    } else if (this->enums.find(expr->name) != this->enums.end()) {
        return Value::with_enum(this->enums[expr->name]);
    }

    if (this->current_function) {
        llvm::AllocaInst* variable = this->current_function->locals[expr->name];
        if (variable) { return this->load(variable->getAllocatedType(), variable); }

        llvm::GlobalVariable* constant = this->current_function->constants[expr->name];
        if (constant) { return Value(this->load(constant->getValueType(), constant), true); }
    }

    if (this->current_struct) {
        llvm::Value* value = this->current_struct->locals[expr->name];
        if (value) { return value; }
    }

    if (this->current_namespace) {
        llvm::Constant* constant = this->current_namespace->constants[expr->name];
        if (constant) { return Value(constant, true); }
    }

    if (expr->name == "null" || expr->name == "nullptr") {
        llvm::Type* type = this->ctx != nullptr ? this->ctx : this->builder->getInt1Ty();
        if (expr->name == "nullptr") {
            if (!type->isPointerTy()) {
                ERROR(expr->start, "Cannot use nullptr with non-pointer types");
            }

            return llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(type));
        }

        return llvm::Constant::getNullValue(type);
    }

    llvm::Constant* constant = this->constants[expr->name];
    if (constant) {
        llvm::GlobalVariable* global = this->module->getGlobalVariable(constant->getName());
        if (global) { 
            llvm::Type* type = global->getValueType();
            return Value(this->load(type, global), true); 
        }

        return Value(constant, true);
    }

    Value val = this->get_function(expr->name);
    if (val.function) { return val; }

    ERROR(expr->start, "Undefined variable '{s}'", expr->name); exit(1);
}

Value Visitor::visit(ast::VariableAssignmentExpr* expr) {
    llvm::Type* type = nullptr;
    if (expr->external) {
        std::string name = expr->names[0];

        type = this->get_llvm_type(expr->type);
        this->module->getOrInsertGlobal(name, type);

        llvm::GlobalVariable* global = this->module->getGlobalVariable(name);
        global->setLinkage(llvm::GlobalValue::ExternalLinkage);

        return global;
    }

    llvm::Value* value = nullptr;
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
        if (expr->type) {
            type = this->get_llvm_type(expr->type);
            this->ctx = type;
        }

        value = expr->value->accept(*this).unwrap(expr->start);
        if (!expr->type) {
            type = value->getType();
        }
    
        Type* ty = Type::from_llvm_type(type);
        if (!ty->is_compatible(value->getType())) {
            ERROR(expr->value->start, "Expected value of type '{t}' but got '{t}'", ty, Type::from_llvm_type(value->getType()));
        }
    }

    llvm::Function* func = this->builder->GetInsertBlock()->getParent();
    if (!expr->is_multiple_variables) {
        llvm::AllocaInst* alloca_inst = this->create_alloca(func, type);
        if (value) { this->builder->CreateStore(value, alloca_inst); }

        this->current_function->locals[expr->names[0]] = alloca_inst;
    } else {
        this->store_tuple(expr->start, this->current_function, value, expr->names);
    }
    
    return value;
}

Value Visitor::visit(ast::ConstExpr* expr) {
    llvm::Value* value = expr->value->accept(*this).unwrap(expr->start);
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

    if (this->current_namespace) {
        this->current_namespace->constants[expr->name] = (llvm::Constant*)value;
        return nullptr;
    }

    name = "__const." + name;
    this->module->getOrInsertGlobal(name, type);

    llvm::GlobalVariable* global = this->module->getNamedGlobal(name);
    global->setInitializer((llvm::Constant*)value);

    if (this->current_function) {
        this->current_function->constants[expr->name] = global;
    } else {
        this->constants[expr->name] = global;
    }

    return global;
}