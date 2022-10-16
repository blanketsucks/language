#include "utils.h"
#include "visitor.h"

void Visitor::store_tuple(Location location, Function* func, llvm::Value* value, std::vector<std::string> names) {
    std::vector<llvm::Value*> values = this->unpack(value, names.size(), location);
    for (auto& pair : utils::zip(names, values)) {
        llvm::AllocaInst* inst = this->create_alloca(pair.second->getType());
        this->builder->CreateStore(pair.second, inst);

        func->scope->variables[pair.first] = inst;
    }
}

Value Visitor::visit(ast::VariableExpr* expr) {
    Scope* scope = this->scope;
    if (scope->has_struct(expr->name)) {
        return Value::with_struct(scope->get_struct(expr->name));
    } else if (scope->has_namespace(expr->name)) {
        return Value::with_namespace(scope->get_namespace(expr->name));
    } else if (scope->has_enum(expr->name)) {
        return Value::with_enum(scope->get_enum(expr->name));
    } else if (scope->has_function(expr->name)) {
        return Value::with_function(scope->get_function(expr->name));
    }

    auto local = scope->get_local(expr->name);
    if (local.first) {
        return Value(this->load(local.first), local.second);
    }

    if (expr->name == "null") {
        llvm::Type* type = this->ctx != nullptr ? this->ctx : this->builder->getInt1Ty();
        return llvm::Constant::getNullValue(type);
    }

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

        is_constant = true;
    } else {
        if (expr->type) {
            type = this->get_llvm_type(expr->type);
            this->ctx = type;
        }

        Value val = expr->value->accept(*this); is_constant = val.is_constant;
        value = val.unwrap(expr->value->start);
        if (!expr->type) {
            type = value->getType();
        }

        if (type->isVoidTy()) {
            utils::error(expr->value->start, "Cannot store value of type 'void'");
        }
        
        if (!this->is_compatible(value->getType(), type)) {
            ERROR(
                expr->value->start, 
                "Expected expression of type '{0}' but got '{1}' instead", 
                this->get_type_name(type), this->get_type_name(value->getType())
            );
        }
    }

    if (!expr->is_multiple_variables) {
        if (!this->current_function) {
            if (!is_constant) {
                utils::error(expr->start, "Cannot store non-constant value in global scope");
            }

            std::string name = FORMAT("__global.{0}", expr->names[0]);
            this->module->getOrInsertGlobal(name, type);

            llvm::GlobalVariable* global = this->module->getGlobalVariable(name);

            global->setLinkage(llvm::GlobalValue::PrivateLinkage);
            global->setInitializer(llvm::cast<llvm::Constant>(value));

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
        }

        this->scope->variables[expr->names[0]] = inst;

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

    llvm::Constant* constant = llvm::cast<llvm::Constant>(value);

    name = "__const." + name;

    this->module->getOrInsertGlobal(name, type);
    llvm::GlobalVariable* global = this->module->getNamedGlobal(name);

    global->setInitializer(constant);
    global->setLinkage(llvm::GlobalVariable::PrivateLinkage);
    global->setConstant(true);

    this->scope->constants[expr->name] = global;
    return constant;
}