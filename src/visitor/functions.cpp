#include "visitor.h"

llvm::Function* Visitor::create_function(
    std::string name, 
    llvm::Type* ret, 
    std::vector<llvm::Type*> args, 
    bool is_variadic, 
    llvm::Function::LinkageTypes linkage
) {
    llvm::FunctionType* type = llvm::FunctionType::get(ret, args, is_variadic);
    return llvm::Function::Create(type, linkage, name, this->module.get());
}

llvm::Value* Visitor::call(
    llvm::Function* function, 
    std::vector<llvm::Value*> args, 
    llvm::Value* self, 
    bool is_constructor,
    llvm::FunctionType* type
) {
    if (!type) {
        type = function->getFunctionType();
    }

    if (self) {
        args.emplace(args.begin(), self);
    }

    llvm::Value* ret = this->builder->CreateCall({type, function}, args);
    if (is_constructor) {
        return self;
    }

    return ret;
}

Value Visitor::visit(ast::PrototypeExpr* expr) {
    if (!this->current_struct && expr->attributes.has("private")) {
        ERROR(expr->start, "Cannot declare private function outside of a struct");
    }

    std::string name;
    bool is_anonymous = false;

    if (expr->name.empty()) {
        name = FORMAT("__anon.{0}", this->id);
        this->id++;

        is_anonymous = true;
    } else {
        if (expr->linkage == ast::ExternLinkageSpecifier::C) {
            name = expr->name;
        } else {
            name = this->format_name(expr->name);
        }
    }

    auto pair = this->format_intrinsic_function(name);
    switch (expr->linkage) {
        case ast::ExternLinkageSpecifier::C:
            if (!pair.second) pair.first = expr->name;
        default: break;
    }

    llvm::Type* ret = nullptr;
    if (!expr->return_type) {
        ret = this->builder->getVoidTy();
    } else {
        ret = expr->return_type->accept(*this).type;
        if (ret->isVoidTy()) {
            NOTE(expr->return_type->start, "Redundant return type. Function return types default to 'void'");
        }
    }

    if (name == this->entry) {
        if (!ret->isVoidTy() && !ret->isIntegerTy()) {
            ERROR(expr->start, "Entry point function must either return void or an integer");
        }

        if (ret->isVoidTy()) {
            ret = this->builder->getInt32Ty();
        }
    }

    std::vector<FunctionArgument> args;
    std::map<std::string, FunctionArgument> kwargs;

    std::vector<llvm::Type*> llvm_args;

    uint32_t index = 0;
    for (auto& arg : expr->args) {
        llvm::Type* type = nullptr;
        if (arg.is_self) {
            type = this->current_struct->type->getPointerTo();
        } else {
            type = arg.type->accept(*this).type;
            if (arg.is_reference) {
                type = type->getPointerTo();
            }
        }

        llvm::Value* default_value = nullptr;
        if (arg.default_value) {
            this->ctx = type;
            default_value = arg.default_value->accept(*this).unwrap(arg.default_value->start);

            if (!llvm::isa<llvm::Constant>(default_value)) {
                ERROR(arg.default_value->start, "Default values must be constants");
            }

            if (!this->is_compatible(type, default_value->getType())) {
                ERROR(
                    arg.default_value->start, 
                    "Default value of type '{0}' does not match expected type '{1}'",
                    this->get_type_name(default_value->getType()), this->get_type_name(type)
                );
            }

            default_value = this->cast(default_value, type);
            this->ctx = nullptr;
        }

        if (arg.is_kwarg) {
            kwargs[arg.name] = {arg.name, type, default_value, index, arg.is_reference, false};
        } else {
            args.push_back({arg.name, type, default_value, index, arg.is_reference, false});
        }

        llvm_args.push_back(type); index++;
    }

    llvm::Function::LinkageTypes linkage = llvm::Function::LinkageTypes::ExternalLinkage;
    if (expr->attributes.has("internal")) {
        linkage = llvm::Function::LinkageTypes::InternalLinkage;
    }

    std::string fn = pair.first;
    if (expr->linkage != ast::ExternLinkageSpecifier::C && pair.first != this->entry && !pair.second) {
        fn = Mangler::mangle(
            expr->name, 
            llvm_args, 
            expr->is_variadic, 
            ret, 
            this->current_namespace, 
            this->current_struct, 
            this->current_module
        );
    }

    llvm::Function* function = this->create_function(
        fn, ret, llvm_args, expr->is_variadic, linkage
    );

    auto func = utils::make_shared<Function>(
        pair.first, 
        args,
        kwargs,
        ret, 
        function,
        (name == this->entry), 
        pair.second,
        is_anonymous,
        expr->attributes
    );

    if (func->noreturn) {
        function->addFnAttr(llvm::Attribute::NoReturn);
    }

    if (this->current_struct) {
        func->parent = this->current_struct;
    }

    func->start = expr->start;
    func->end = expr->end;

    this->scope->functions[expr->name] = func;
    return function;
}

Value Visitor::visit(ast::FunctionExpr* expr) {
    auto func = this->scope->functions[expr->prototype->name];
    if (!func) {
        expr->prototype->accept(*this);
        func = this->scope->functions[expr->prototype->name];
    }

    llvm::Function* function = func->value;
    if (!function->empty()) {
        NOTE(func->start, "Function '{0}' was previously defined here", func->name);
        ERROR(expr->start, "Function '{0}' is already defined", func->name);
    }

    if (func->is_intrinsic) {
        ERROR(expr->start, "Cannot define intrinsic function '{0}'", func->name);
    }

    func->end = expr->end;

    llvm::BasicBlock* block = llvm::BasicBlock::Create(*this->context, "", function);
    this->set_insert_point(block, false);

    Branch* branch = func->create_branch(name);
    func->branch = branch;

    if (!func->ret->isVoidTy()) {
        func->return_value = this->builder->CreateAlloca(func->ret);
    }

    func->return_block = llvm::BasicBlock::Create(*this->context);
    func->scope = this->create_scope(func->name, ScopeType::Function);

    uint32_t i = 0;
    for (auto& arg : func->params()) {
        llvm::Argument* argument = function->getArg(i);
        argument->setName(arg.name);

        if (!arg.is_reference) {
            llvm::AllocaInst* inst = this->create_alloca(arg.type);
            this->builder->CreateStore(argument, inst);

            func->scope->variables[arg.name] = Variable::from_alloca(arg.name, inst);
        } else {
            func->scope->variables[arg.name] = Variable::from_value(arg.name, argument);
        }

        i++;
    }
    
    auto outer = this->current_function;
    this->current_function = func;

    if (!expr->body) {
        if (!func->ret->isVoidTy()) {
            ERROR(expr->start, "Function '{0}' expects a return value", name);
        }

        this->builder->CreateRetVoid();
    } else {
        expr->body->accept(*this);

        if (!func->has_return()) {
            if (func->ret->isVoidTy() || func->is_entry) {
                func->defer(*this);
                if (func->is_entry) {
                    this->builder->CreateRet(this->builder->getInt32(0));
                } else {
                    this->builder->CreateRetVoid();
                }
            } else {
                ERROR(expr->start, "Function '{0}' expects a return value", name);
            }
        } else {
            this->set_insert_point(func->return_block);

            if (func->ret->isVoidTy()) {
                this->builder->CreateRetVoid();
            } else {
                llvm::Value* value = this->builder->CreateLoad(function->getReturnType(), func->return_value);
                this->builder->CreateRet(value);
            }
        }
    }

    bool error = llvm::verifyFunction(*function, &llvm::errs());
    assert((!error) && "Error while verifying function IR. Most likely a compiler bug.");

    if (this->with_optimizations) {
        this->fpm->run(*function);
    }

    this->scope->exit(this);

    this->current_function = outer;
    if (outer) {
        this->builder->SetInsertPoint(outer->current_block);
    }

    return Value(function, true);
}

Value Visitor::visit(ast::ReturnExpr* expr) {
    auto func = this->current_function;
    if (expr->value) {
        if (func->ret->isVoidTy()) {
            ERROR(expr->start, "Cannot return a value from void function");
        }

        this->ctx = func->ret;
        Value val = expr->value->accept(*this);
        if (val.is_reference) {
            ERROR(expr->value->start, "Cannot return a reference associated with a local stack variable");
        }

        llvm::Value* value = val.unwrap(expr->value->start);
        if (!this->is_compatible(func->ret, value->getType())) {
            ERROR(
                expr->start, "Cannot return value of type '{0}' from function expecting '{1}'", 
                this->get_type_name(value->getType()), this->get_type_name(func->ret)
            );
        } else {
            value = this->cast(value, func->ret);
        }

        func->branch->has_return = true;

        this->builder->CreateStore(value, func->return_value);
        func->defer(*this);

        this->builder->CreateBr(func->return_block);
        this->ctx = nullptr;

        return nullptr;
    } else {
        if (!func->ret->isVoidTy()) {
            ERROR(expr->start, "Function '{0}' expects a return value", func->name);
        }

        func->branch->has_return = true;
        func->defer(*this);

        this->builder->CreateBr(func->return_block);
        return nullptr;
    }
}

Value Visitor::visit(ast::DeferExpr* expr) {
    auto func = this->current_function;
    if (!func) {
        ERROR(expr->start, "Defer statement outside of function");
    }

    func->defers.push_back({std::move(expr->expr), expr->attributes.has("ignore_noreturn")});
    return nullptr;
}

Value Visitor::visit(ast::CallExpr* expr) {
    Value callable = expr->callee->accept(*this);
    auto func = callable.function;

    if (!func && !expr->kwargs.empty()) {
        ERROR(expr->start, "Keyword arguments are not allowed in this context");
    }

    if (func) func->used = true;

    size_t argc = expr->args.size() + expr->kwargs.size();
    bool is_constructor = false;

    llvm::Type* type = nullptr;
    if (callable.structure) {
        auto structure = callable.structure;
        
        llvm::AllocaInst* instance = this->builder->CreateAlloca(structure->type);
        callable.parent = instance; // `self` parameter for the constructor
    
        func = structure->scope->functions["constructor"];
        func->used = true;

        callable.value = func->value;
        is_constructor = true;

        type = func->value->getFunctionType();
    } else {
        type = callable.value->getType();
        if (type->isPointerTy()) {
            type = type->getNonOpaquePointerElementType();
            if (!type->isFunctionTy()) {
                ERROR(expr->start, "Expected a function but got value of type '{0}'", this->get_type_name(type));
            }
        } else {
            if (!callable.function) {
                ERROR(expr->start, "Expected a function but got value of type '{0}'", this->get_type_name(type));
            }
        }
    }

    if (callable.parent) {
        argc++;
    }

    llvm::Function* function = (llvm::Function*)callable.value;

    std::string name = function->getName().str();
    if (function->getName() == this->entry) {
        ERROR(expr->start, "Cannot call the main entry function");
    }

    if (func) {
        name = func->name;
    }

    llvm::FunctionType* ftype = nullptr;
    if (type->isFunctionTy()) {
        ftype = llvm::cast<llvm::FunctionType>(type);
    } else {
        type = type->getNonOpaquePointerElementType();
        if (type->isFunctionTy()) {
            ftype = llvm::cast<llvm::FunctionType>(type);
        } else {
            ERROR(expr->start, "Expected a function but got value of type '{0}'", this->get_type_name(type));
        }
    }

    if (!func->has_any_default_value()) {
        if (ftype->getNumParams() != argc) {
            if (ftype->isVarArg()) {
                if (argc < 1) {
                    if (name.empty()) {
                        ERROR(expr->start, "Function call expects at least one argument", name);
                    }

                    ERROR(expr->start, "Function '{0}' expects at least one argument", name);
                }
            } else {
                if (name.empty()) {
                    ERROR(
                        expr->start, 
                        "Function call expects {0} arguments but got {1}", 
                        function->arg_size(), argc
                    );
                }

                ERROR(
                    expr->start, 
                    "Function '{s}' expects {0} arguments but got {1}", 
                    name, function->arg_size(), argc
                );
            }
        }
    }

    std::vector<llvm::Value*> args;
    uint32_t i = callable.parent ? 1 : 0;

    auto visit = [&](ast::Expr* expr, bool is_reference = false) {
        if (i < argc) this->ctx = ftype->getParamType(i);

        llvm::Value* value = nullptr;
        if (is_reference) {
            value = this->as_reference(expr).value;
            if (!value) {
                ERROR(expr->start, "Expected variable, struct attribute or array element as reference value");
            }
        } else {
            value = expr->accept(*this).unwrap(expr->start);
        }

        // TODO: Fix when kwargs are passed in a different order
        if (i < ftype->getNumParams()) {
            if (!this->is_compatible(ftype->getParamType(i), value->getType())) {
                ERROR(
                    expr->start, 
                    "Cannot pass value of type '{0}' to parameter of type '{1}'", 
                    this->get_type_name(value->getType()), this->get_type_name(ftype->getParamType(i))
                );
            } else {
                value = this->cast(value, ftype->getParamType(i));
            }
        }

        args.push_back(value);
        this->ctx = nullptr;

        i++;
    };

    if (func) {
        if (func->noreturn && !this->current_function) {
            ERROR(expr->start, "Cannot call a noreturn function from top level code");
        }

        if (func->noreturn) {
            this->current_function->defer(*this, true);
        }

        auto params = func->params();
        std::map<uint32_t, llvm::Value*> values;

        for (auto& arg : expr->args) {
            bool is_reference = false;
            if (i < params.size()) {
                is_reference = params[i].is_reference;
            }

            auto& value = values[i];

            visit(arg.get(), is_reference);
            value = args.back();
        }

        for (auto& kwarg : expr->kwargs) {
            if (!func->has_kwarg(kwarg.first)) {
                ERROR(kwarg.second->start, "Function '{0}' does not have a keyword argument '{1}'", name, kwarg.first);
            }

            bool is_reference = false;
            if (i < params.size()) {
                is_reference = params[i].is_reference;
            }

            auto& value = values[i];
            visit(kwarg.second.get(), is_reference);

            value = args.back();
        }

        if (callable.parent) {
            params.erase(params.begin());
        }

        for (auto& param : params) {
            auto value = values[param.index];
            if (!value) {
                if (param.default_value) {
                    args.push_back(param.default_value);
                } else {
                    if (i < ftype->getNumParams() && !ftype->isVarArg()) {
                        ERROR(expr->start, "Missing value for parameter '{0}'", param.name);
                    }
                }
            }
        }
    } else {
        for (auto& arg : expr->args) {
            visit(arg.get());
        }
    }

    if (!this->current_function) {
        this->constructors.push_back(FunctionCall {
            function, args, nullptr
        });

        return Value(nullptr, false, false, true);
    }

    return this->call(
        function, args, callable.parent, is_constructor, ftype
    );
}