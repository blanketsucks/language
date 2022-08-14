#include "visitor.h"

llvm::Function* Visitor::create_function(
    std::string name, llvm::Type* ret, std::vector<llvm::Type*> args, bool has_varargs, llvm::Function::LinkageTypes linkage
) {
    llvm::FunctionType* type = llvm::FunctionType::get(ret, args, has_varargs);
    return llvm::Function::Create(type, linkage, name, this->module.get());
}

Value Visitor::get_function(std::string name) {
    if (this->current_struct) {
        Function* func = this->current_struct->methods[name];
        if (func) {
            func->used = true;
            return Value::with_function(func);
        }
    }

    if (this->current_namespace) {
        Function* func = this->current_namespace->functions[name];
        if (func) {
            func->used = true;
            return Value::with_function(func);
        }
    }

    if (this->functions.find(name) != this->functions.end()) {
        Function* func = this->functions[name];
        func->used = true;

        return Value::with_function(func);
    }

    return nullptr;
}

Value Visitor::visit(ast::PrototypeExpr* expr) {
    if (!this->current_struct && expr->attributes.has("private")) {
        ERROR(expr->start, "Cannot declare private function outside of a struct");
    }

    std::string original = this->format_name(expr->name);
    auto pair = this->is_intrinsic(original);

    if (expr->linkage_specifier == ast::ExternLinkageSpecifier::C) {
        if (!pair.second) {
            pair.first = expr->name;
        }
    }

    llvm::Type* ret = this->get_llvm_type(expr->return_type);

    std::vector<llvm::Type*> args;
    for (auto& arg : expr->args) {
        args.push_back(this->get_llvm_type(arg.type));
    }

    llvm::Function* function = this->create_function(pair.first, ret, args, expr->has_varargs, llvm::Function::ExternalLinkage);
    int i = 0;
    for (auto& arg : function->args()) {
        arg.setName(expr->args[i++].name);
    }

    Function* func = new Function(pair.first, args, ret, pair.second, expr->attributes);
    if (this->current_struct) {
        func->parent = this->current_struct;
        this->current_struct->methods[expr->name] = func;
    } else if (this->current_namespace) {
        this->current_namespace->functions[expr->name] = func;
    } else {
        this->functions[original] = func;
    }

    func->value = function;
    return function;
}

Value Visitor::visit(ast::FunctionExpr* expr) {
    std::string name = this->format_name(expr->prototype->name);
    llvm::Function* function = this->module->getFunction(name);
    if (!function) {
        function = (llvm::Function*)(expr->prototype->accept(*this).value);
    }

    if (!function->empty()) { 
        ERROR(expr->start, "Function '{s}' already defined", name);
    }

    Function* func;
    if (this->current_struct) {
        func = this->current_struct->methods[expr->prototype->name];
    } else if (this->current_namespace) {
        func = this->current_namespace->functions[expr->prototype->name];
    } else {
        func = this->functions[name];
    }

    if (func->is_intrinsic) {
        ERROR(expr->start, "Cannot define intrinsic function '{s}'", name);
    }

    llvm::BasicBlock* block = llvm::BasicBlock::Create(this->context, "", function);
    this->builder->SetInsertPoint(block);

    Branch* branch = func->create_branch(name);
    func->branch = branch;

    if (!func->ret->isVoidTy()) {
        func->return_value = this->builder->CreateAlloca(func->ret);
    }

    func->return_block = llvm::BasicBlock::Create(this->context);
    for (auto& arg : function->args()) {
        std::string name = arg.getName().str();

        llvm::AllocaInst* alloca_inst = this->create_alloca(function, arg.getType());
        this->builder->CreateStore(&arg, alloca_inst);

        func->locals[name] = alloca_inst;
    }
    
    this->current_function = func;
    if (!expr->body) {
        if (!func->ret->isVoidTy()) {
            ERROR(expr->start, "Function '{s}' expects a return value", name);
        }

        this->builder->CreateRetVoid();
    } else {
        expr->body->accept(*this);

        if (!func->has_return()) {
            if (func->ret->isVoidTy()) {
                func->defer(*this);
                this->builder->CreateRetVoid();
            } else {
                ERROR(expr->start, "Function '{s}' expects a return value", name);
            }
        } else {
            function->getBasicBlockList().push_back(func->return_block);
            this->builder->SetInsertPoint(func->return_block);

            if (func->ret->isVoidTy()) {
                this->builder->CreateRetVoid();
            } else {
                llvm::Value* value = this->builder->CreateLoad(function->getReturnType(), func->return_value);
                this->builder->CreateRet(value);
            }
        }
    }

    for (auto defer : func->defers) {
        delete defer;
    }
    
    this->current_function = nullptr;
    
    bool error = llvm::verifyFunction(*function, &llvm::errs());
    assert((!error) && "Error while verifying function IR. Most likely a compiler bug.");

    this->fpm->run(*function);
    return function;
}

Value Visitor::visit(ast::ReturnExpr* expr) {
    if (!this->current_function) {
        ERROR(expr->start, "Cannot return from top-level code");
    }

    Function* func = this->current_function;
    if (expr->value) {
        if (func->ret->isVoidTy()) {
            ERROR(expr->start, "Cannot return a value from void function");
        }

        llvm::Value* value = expr->value->accept(*this).unwrap(this, expr->start);
        func->branch->has_return = true;

        // Got rid of the typechecking for now.
        this->builder->CreateStore(value, func->return_value);
        func->defer(*this);

        this->builder->CreateBr(func->return_block);
        return nullptr;
    } else {
        if (!func->ret->isVoidTy()) {
            ERROR(expr->start, "Function '{s}' expects a return value", func->name);
        }

        func->branch->has_return = true;
        func->defer(*this);

        this->builder->CreateBr(func->return_block);
        return nullptr;
    }
}

Value Visitor::visit(ast::DeferExpr* expr) {
    Function* func = this->current_function;
    if (!func) {
        ERROR(expr->start, "Defer statement outside of function");
    }

    ast::Expr* defer = expr->expr.release();
    func->defers.push_back(defer);
    
    return nullptr;
}

Value Visitor::visit(ast::CallExpr* expr) {
    Value callable = expr->callee->accept(*this);
    Function* func = callable.function;

    size_t argc = expr->args.size();
    bool is_constructor = false;

    if (callable.structure) {
        Struct* structure = callable.structure;
        llvm::AllocaInst* instance = this->builder->CreateAlloca(structure->type);

        if (!structure->has_method("constructor")) {
            if (structure->fields.size() != argc) {
                ERROR(expr->start, "Wrong number of arguments for constructor");
            }

            int i = 0;
            for (auto& argument : expr->args) {
                llvm::Value* arg = argument->accept(*this).unwrap(this, argument->start);
                llvm::Value* ptr = this->builder->CreateStructGEP(structure->type, instance, i);

                this->builder->CreateStore(arg, ptr);
                structure->locals[arg->getName().str()] = ptr;

                i++;
            }

            return instance;
        }

        callable.parent = instance; // `self` parameter for the constructor
        func = structure->methods["constructor"];

        callable.value = this->module->getFunction(func->name);
        is_constructor = true;
    } else {
        if (!callable.function) {
            ERROR(expr->start, "Cannot call non-function");
        }

        if (callable.type()->isPointerTy()) {
            llvm::Type* type = callable.type()->getPointerElementType();
            if (!type->isFunctionTy()) {
                ERROR(expr->start, "Cannot call non-function type");
            }

        } else if (!callable.type()->isFunctionTy()) {
            ERROR(expr->start, "Cannot call non-function type");
        }
    }

    if (callable.parent) {
        argc++;
    }

    llvm::Function* function = (llvm::Function*)callable.value;
    if (function->arg_size() != argc) {
        if (function->isVarArg()) {
            if (argc < 1) {
                ERROR(expr->start, "Function '{s}' expects at least one argument", callable.name());
            }
        } else {
            ERROR(expr->start, "Function '{s}' expects {i} arguments", callable.name(), function->arg_size());
        }
    }

    
    std::vector<llvm::Value*> args;
    if (callable.parent) {
        args.push_back(callable.parent);
    }

    int i = callable.parent ? 1 : 0;
    for (auto& arg : expr->args) {
        llvm::Value* value = arg->accept(*this).unwrap(this, expr->start);

        if ((function->isVarArg() && i == 0) || !function->isVarArg()) {
            llvm::Value* argument = function->getArg(i);

            Type* expected = Type::from_llvm_type(argument->getType());
            Type* type = Type::from_llvm_type(value->getType());

            if (!expected->is_compatible(type)) {
                std::string name = argument->getName().str();
                ERROR(expr->start, "Argument '{s}' of type '{t}' does not match expected type '{t}'", name, type, expected);
            } else {
                value = this->cast(value, expected);
            }
        }

        args.push_back(value);
        i++;
    }

    if (this->current_function) {
        this->current_function->calls.push_back(func);
    }

    llvm::Value* ret = this->builder->CreateCall(function, args);
    if (is_constructor) {
        return callable.parent;
    }

    return ret;
}