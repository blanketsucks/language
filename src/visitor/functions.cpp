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

void Visitor::typecheck_function_call(
    llvm::Function* function, std::vector<llvm::Value*>& args, uint32_t start, Location location
) {
    std::string fn = function->getName().str();
    
    for (auto& value : args) {
        if ((function->isVarArg() && start == 0) || !function->isVarArg()) {
            llvm::Argument* argument = function->getArg(start);
            if (!this->is_compatible(argument->getType(), value->getType())) {
                std::string name = argument->getName().str();
                if (fn.empty()) {
                    ERROR(
                        location, 
                        "Argument of index {0} of type '{1}' does not match expected type '{2}'",
                        start, this->get_type_name(value->getType()), this->get_type_name(argument->getType())
                    );
                }

                if (name.empty()) {
                    ERROR(
                        location, 
                        "Argument of index {0} of type '{1}' does not match expected type '{2}' in function '{3}'.", 
                        start, this->get_type_name(value->getType()), this->get_type_name(argument->getType()), fn
                    );
                }

                ERROR(
                    location, 
                    "Argument '{0}' of type '{1}' does not match expected type '{2}' in function '{3}'.", 
                    name, this->get_type_name(value->getType()), this->get_type_name(argument->getType()), fn
                );
            } else {
                value = this->cast(value, argument->getType());
            }
        }

        start++;
    }
}

llvm::Value* Visitor::call(
    llvm::Function* function, 
    std::vector<llvm::Value*> args, 
    bool is_constructor,
    llvm::Value* self, 
    llvm::FunctionType* type,
    Location location
) {
    if (!type) {
        type = function->getFunctionType();
    }

    this->typecheck_function_call(function, args, self ? 1 : 0, location);
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

        if (arg.is_kwarg) {
            kwargs[arg.name] = {arg.name, type, arg.is_reference, true};
        } else {
            args.push_back({arg.name, type, arg.is_reference, false});
        }

        llvm_args.push_back(type);
    }

    llvm::Function::LinkageTypes linkage = llvm::Function::LinkageTypes::ExternalLinkage;
    if (expr->attributes.has("internal")) {
        linkage = llvm::Function::LinkageTypes::InternalLinkage;
    }

    if (expr->linkage != ast::ExternLinkageSpecifier::C && pair.first != this->entry && !pair.second) {
        pair.first = Mangler::mangle(
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
        pair.first, ret, llvm_args, expr->is_variadic, linkage
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

    if (func->attrs.has("inline")) {
        function->addFnAttr(llvm::Attribute::AlwaysInline);
    }

    func->start = expr->start;
    func->end = expr->end;

    this->scope->functions[expr->name] = func;
    return function;
}

Value Visitor::visit(ast::FunctionExpr* expr) {
    std::string name = this->format_name(expr->prototype->name);
    llvm::Function* function = this->module->getFunction(name);
    if (!function) {
        function = llvm::cast<llvm::Function>(expr->prototype->accept(*this).value);
    }

    auto func = this->scope->functions[expr->prototype->name];
    if (!function->empty()) {
        NOTE(func->start, "Function '{0}' was previously defined here", name);
        ERROR(expr->start, "Function '{0}' is already defined", name);
    }

    if (func->is_intrinsic) {
        ERROR(expr->start, "Cannot define intrinsic function '{0}'", name);
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
    for (auto& arg : func->get_all_args()) {
        llvm::Argument* argument = function->getArg(i);
        argument->setName(arg.name);

        if (!arg.is_reference) {
            llvm::AllocaInst* inst = this->create_alloca(arg.type);
            this->builder->CreateStore(argument, inst);

            func->scope->variables[arg.name] = inst;
        } else {
            func->scope->variables[arg.name] = argument;
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
        llvm::Value* value = expr->value->accept(*this).unwrap(expr->start);

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

    std::vector<llvm::Value*> args;
    args.reserve(argc);

    uint32_t i = 0;
    auto visit = [&](ast::Expr* expr, bool is_reference = false) {
        if (i < argc) this->ctx = ftype->getParamType(i);

        if (is_reference) {
            // TODO: check null and raise error
            args.push_back(this->get_pointer_from_expr(expr).first);
        } else {
            args.push_back(expr->accept(*this).unwrap(expr->start));
        }

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

        auto all_args = func->get_all_args();
        for (auto& arg : expr->args) {
            bool is_reference = false;
            if (i < all_args.size()) {
                is_reference = all_args[i].is_reference;
            }

            visit(arg.get(), is_reference);
        }

        for (auto& kwarg : expr->kwargs) {
            if (!func->has_kwarg(kwarg.first)) {
                ERROR(kwarg.second->start, "Function '{0}' does not have a keyword argument '{1}'", name, kwarg.first);
            }

            bool is_reference = false;
            if (i < all_args.size()) {
                is_reference = all_args[i].is_reference;
            }

            visit(kwarg.second.get(), is_reference);
        }        
    } else {
        for (auto& arg : expr->args) {
            visit(arg.get());
        }
    }

    if (!this->current_function) {
        FunctionCall* call = new FunctionCall;

        call->function = function;
        call->args = args;
        call->store = nullptr;
        call->start = expr->start;
        call->end = expr->end;

        return Value::as_call(call);
    }

    return this->call(
        function, args, is_constructor, callable.parent, ftype, expr->start
    );
}