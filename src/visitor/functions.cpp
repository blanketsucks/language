#include "visitor.h"

static std::vector<std::string> RESERVED_FUNCTION_NAMES = {
    "__global_constructors_init"
};

bool Visitor::is_reserved_function(std::string name) {
    return std::find(
        RESERVED_FUNCTION_NAMES.begin(), RESERVED_FUNCTION_NAMES.end(), name
    ) != RESERVED_FUNCTION_NAMES.end();
}

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

std::vector<llvm::Value*> Visitor::handle_function_arguments(
    Location location,
    utils::Shared<Function> function,
    llvm::Value* self,
    std::vector<utils::Ref<ast::Expr>> args,
    std::map<std::string, utils::Ref<ast::Expr>> kwargs
) {
    uint32_t argc = args.size() + kwargs.size() + (self ? 1 : 0);
    if (function->has_any_default_value()) {
        if (argc + function->get_default_arguments_count() < function->argc()) {
            ERROR(location, "Function expects at least {0} arguments but got {1}", function->argc(), argc);
        } else if (argc > function->argc() && !function->is_variadic()) {
            ERROR(location, "Function expects at most {0} arguments but got {1}", function->argc(), argc);
        }
    } else {
        if (argc < function->argc()) {
            ERROR(location, "Function expects at least {0} arguments but got {1}", function->argc(), argc);
        } else if (argc > function->argc() && !function->is_variadic()) {
            ERROR(location, "Function expects at most {0} arguments but got {1}", function->argc(), argc);
        }
    }

    if (!this->current_function && function->noreturn) {
        ERROR(location, "Cannot call noreturn function '{0}' from global scope", function->name);
    }

    std::map<int64_t, llvm::Value*> values;
    std::vector<llvm::Value*> varargs;

    uint32_t i = self ? 1 : 0;
    auto params = function->params();

    // Name is only used for kwargs when they are not in order
    const auto& visit = [&](utils::Ref<ast::Expr> expr, std::string name = "") {
        llvm::Value* value = nullptr;
        if (i < params.size()) {
            FunctionArgument param;
            if (!name.empty()) {
                param = function->kwargs[name]; i = param.index;
            } else {
                param = params[i];
            }

            this->ctx = param.type;
            if (param.is_reference) {
                auto ref = this->as_reference(expr.get());
                if (ref.is_immutable && !param.is_immutable) {
                    ERROR(expr->start, "Cannot pass immutable reference to mutable reference parameter '{0}'", param.name);
                }

                value = ref.value;
                if (!this->is_compatible(param.type, value->getType())) {
                    ERROR(
                        expr->start, 
                        "Cannot pass reference value of type '{0}' to reference parameter of type '{1}'", 
                        this->get_type_name(value->getType()->getNonOpaquePointerElementType()), 
                        this->get_type_name(param.type->getNonOpaquePointerElementType())
                    );
                }
            } else {
                Value val = expr->accept(*this);
                if (val.is_immutable && !param.is_immutable) {
                    ERROR(expr->start, "Cannot pass immutable value to mutable parameter '{0}'", param.name);
                }

                value = val.unwrap(expr->start);
                if (!this->is_compatible(param.type, value->getType())) {
                    ERROR(
                        expr->start, 
                        "Cannot pass value of type '{0}' to parameter of type '{1}'", 
                        this->get_type_name(value->getType()), 
                        this->get_type_name(param.type)
                    );
                }

                value = this->cast(value, param.type);
            }

            values[i] = value;
            this->ctx = nullptr;
        } else {
            if (!function->is_variadic()) {
                ERROR(
                    expr->start, 
                    "Function call expects {0} arguments but got {1}", 
                    function->value->size(), i
                );
            }

            value = expr->accept(*this).unwrap(expr->start);
            varargs.push_back(value);
        }

        i++; 
    };

    for (auto& arg : args) {
        visit(std::move(arg));
    }

    for (auto& entry : kwargs) {
        if (!function->has_kwarg(entry.first)) {
            ERROR(entry.second->start, "Function does not have a keyword parameter named '{0}'", entry.first);
        }

        visit(std::move(entry.second), entry.first);
    }

    std::vector<llvm::Value*> ret;
    ret.reserve(function->argc());

    for (auto& param : params) {
        bool found = values.find(param.index) != values.end();
        llvm::Value* value = found ? values[param.index] : param.default_value;

        ret.push_back(value);
    }

    for (auto& value : varargs) {
        ret.push_back(value);
    }


    return ret;
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
            arg.is_reference = true;
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
            kwargs[arg.name] = {
                arg.name, 
                type, 
                default_value, 
                index, 
                arg.is_reference, 
                false,
                arg.is_immutable
            };
        } else {
            args.push_back({
                arg.name, 
                type, 
                default_value, 
                index, 
                arg.is_reference, 
                false,
                arg.is_immutable
            });
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

    if (this->is_reserved_function(fn)) {
        ERROR(expr->start, "Function name '{0}' is reserved", fn);
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

            func->scope->variables[arg.name] = Variable::from_alloca(arg.name, inst, arg.is_immutable);
        } else {
            func->scope->variables[arg.name] = Variable::from_value(
                arg.name, argument, arg.is_immutable, true, false
            );
        }

        i++;
    }
    
    auto outer = this->current_function;
    this->current_function = func;

    if (!expr->body) {
        if (!func->ret->isVoidTy()) {
            ERROR(expr->start, "Function '{0}' expects a return value", func->name);
        }

        this->builder->CreateRetVoid();
    } else {
        expr->body->accept(*this);

        if (!func->has_return()) {
            if (func->ret->isVoidTy() || func->is_entry) {
                if (func->is_entry) {
                    this->builder->CreateRet(this->builder->getInt32(0));
                } else {
                    this->builder->CreateRetVoid();
                }
            } else {
                ERROR(expr->start, "Function '{0}' expects a return value", func->name);
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
        if (val.is_reference && val.is_stack_allocated) {
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

        this->builder->CreateBr(func->return_block);
        this->ctx = nullptr;

        return nullptr;
    } else {
        if (!func->ret->isVoidTy()) {
            ERROR(expr->start, "Function '{0}' expects a return value", func->name);
        }

        func->branch->has_return = true;
        this->builder->CreateBr(func->return_block);
        
        return nullptr;
    }
}

Value Visitor::visit(ast::DeferExpr* expr) {
    auto func = this->current_function;
    if (!func) {
        ERROR(expr->start, "Defer statement outside of function");
    }

    TODO("Fix defer statement");
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
        callable.self = instance; // `self` parameter for the constructor
    
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

    if (callable.self) {
        argc++;
    }

    llvm::Function* function = (llvm::Function*)callable.value;

    std::string name = function->getName().str();
    if (function->getName() == this->entry) {
        ERROR(expr->start, "Cannot call the main entry function");
    }

    if (func) {
        name = func->name;
        if (this->current_function) {
            this->current_function->calls.push_back(function);
        }
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

    std::vector<llvm::Value*> args;
    uint32_t i = callable.self ? 1 : 0;

    if (func) {
        args = this->handle_function_arguments(
            expr->start,
            func, 
            callable.self, 
            std::move(expr->args), 
            std::move(expr->kwargs)
        );
    } else {
        if (argc > ftype->getNumParams() && !ftype->isVarArg()) {
            ERROR(expr->start, "Function expects at most {0} arguments but got {1}", ftype->getNumParams(), argc);
        } else if (argc < ftype->getNumParams()) {
            ERROR(expr->start, "Function expects at least {0} arguments but got {1}", ftype->getNumParams(), argc);
        }

        args.reserve(argc);
        for (auto& arg : expr->args) {
            if (i < argc) this->ctx = ftype->getParamType(i);
            llvm::Value* value = arg->accept(*this).unwrap(arg->start);

            if (i < ftype->getNumParams()) {
                if (!this->is_compatible(ftype->getParamType(i), value->getType())) {
                    ERROR(
                        arg->start, 
                        "Cannot pass value of type '{0}' to parameter of type '{1}'", 
                        this->get_type_name(value->getType()), this->get_type_name(ftype->getParamType(i))
                    );
                } else {
                    value = this->cast(value, ftype->getParamType(i));
                }
            }

            args.push_back(value); i++;
        }
    }

    if (!this->current_function) {
        this->constructors.push_back(FunctionCall {
            function, args, nullptr
        });

        return Value::as_early_function_call();
    }


    return this->call(
        function, args, callable.self, is_constructor, ftype
    );
}