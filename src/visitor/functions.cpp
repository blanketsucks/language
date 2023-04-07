#include <quart/parser/ast.h>
#include <quart/visitor.h>

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
    Span span,
    utils::Ref<Function> function,
    llvm::Value* self,
    std::vector<utils::Scope<ast::Expr>>& args,
    std::map<std::string, utils::Scope<ast::Expr>>& kwargs
) {
    uint32_t argc = args.size() + kwargs.size() + (self ? 1 : 0);
    bool is_variadic = function->is_variadic() || function->is_c_variadic();

    if (function->has_any_default_value()) {
        if (argc + function->get_default_arguments_count() < function->argc()) {
            ERROR(span, "Function expects at least {0} arguments but got {1}", function->argc(), argc);
        } else if (argc > function->argc() && !is_variadic) {
            ERROR(span, "Function expects at most {0} arguments but got {1}", function->argc(), argc);
        }
    } else {
        if (argc < function->argc()) {
            ERROR(span, "Function expects at least {0} arguments but got {1}", function->argc(), argc);
        } else if (argc > function->argc() && !is_variadic) {
            ERROR(span, "Function expects at most {0} arguments but got {1}", function->argc(), argc);
        }
    }

    if (!this->current_function && function->noreturn) {
        ERROR(span, "Cannot call noreturn function '{0}' from global scope", function->name);
    }

    std::map<int64_t, llvm::Value*> values;

    std::vector<llvm::Value*> c_variadic_values; // For the ...
    std::vector<llvm::Value*> variadic_values;   // For the *args

    uint32_t i = self ? 1 : 0;
    auto params = function->params();

    bool encountered_variadic = false;
    llvm::Type* variadic_type = nullptr;

    // Name is only used for kwargs when they are not in order
    const auto& visit = [&](
        utils::Scope<ast::Expr>& expr, const std::string& name = ""
    ) {
        llvm::Value* value = nullptr;
        if (encountered_variadic) {
            value = expr->accept(*this).unwrap(expr->span);
            if (!this->is_compatible(variadic_type, value->getType())) {
                ERROR(
                    expr->span, 
                    "Cannot pass value of type '{0}' to variadic parameter of type '{1}'", 
                    this->get_type_name(value->getType()), 
                    this->get_type_name(variadic_type)
                );
            }

            variadic_values.push_back(this->cast(value, variadic_type)); return;
        }

        if (i < params.size()) {
            FunctionArgument param;
            if (!name.empty()) {
                param = function->kwargs[name]; i = param.index;
            } else {
                param = params[i];
            }

            this->ctx = param.type.value;
            if (param.is_reference()) {
                auto ref = this->as_reference(expr);
                if (ref.is_immutable && !param.is_immutable) {
                    ERROR(expr->span, "Cannot pass immutable reference to mutable reference parameter '{0}'", param.name);
                }

                value = ref.value;
                if (!this->is_compatible(param.type, value->getType())) {
                    ERROR(
                        expr->span, 
                        "Cannot pass reference value of type '{0}' to reference parameter of type '{1}'", 
                        this->get_type_name(value->getType()->getPointerElementType()), 
                        this->get_type_name(param.type->getPointerElementType())
                    );
                }

                values[i] = value;
                this->ctx = nullptr;
            } else if (param.is_variadic) {
                encountered_variadic = true;
                variadic_type = param.type->getStructElementType(1)->getPointerElementType();

                value = expr->accept(*this).unwrap(expr->span);
                if (!this->is_compatible(variadic_type, value->getType())) {
                    ERROR(
                        expr->span, 
                        "Cannot pass value of type '{0}' to variadic parameter of type '{1}'", 
                        this->get_type_name(value->getType()), 
                        this->get_type_name(variadic_type)
                    );
                }

                variadic_values.push_back(this->cast(value, variadic_type)); return;
            } else {
                Value val = expr->accept(*this);
                value = val.unwrap(expr->span);

                if (!this->is_compatible(param.type, value->getType())) {
                    ERROR(
                        expr->span, 
                        "Cannot pass value of type '{0}' to parameter of type '{1}'", 
                        this->get_type_name(value->getType()), 
                        this->get_type_name(param.type)
                    );
                }

                value = this->cast(value, param.type.value);
        
                values[i] = value;
                this->ctx = nullptr;
            }
        } else {
            if (!is_variadic) {
                ERROR(
                    expr->span, 
                    "Function expects {0} arguments but got {1}", 
                    function->value->size(), i
                );
            }

            value = expr->accept(*this).unwrap(expr->span);
            c_variadic_values.push_back(value);
        }

        i++; 
    };

    for (auto& arg : args) {
        visit(arg);
    }

    if (encountered_variadic) {
        FunctionArgument param = params[i];

        // Refers to the `data` field in the struct
        llvm::Type* type = param.type->getStructElementType(1)->getPointerElementType();
        size_t count = variadic_values.size();

        llvm::Type* array = llvm::ArrayType::get(type, count);
        llvm::Value* alloca = this->alloca(array);

        for (size_t i = 0; i < count; i++) {
            llvm::Value* ptr = this->builder->CreateGEP(
                array, alloca, { this->builder->getInt32(0), this->builder->getInt32(i) }
            );

            this->builder->CreateStore(variadic_values[i], ptr);
        }

        llvm::Value* ptr = this->builder->CreateBitCast(alloca, type->getPointerTo());

        llvm::Value* value = llvm::Constant::getNullValue(param.type.value);

        value = this->builder->CreateInsertValue(value, this->builder->getInt32(count), 0);
        value = this->builder->CreateInsertValue(value, ptr, 1);

        values[i] = value;
        encountered_variadic = false;
    }

    for (auto& entry : kwargs) {
        if (!function->has_kwarg(entry.first)) {
            ERROR(entry.second->span, "Function does not have a keyword parameter named '{0}'", entry.first);
        }

        visit(entry.second, entry.first);
    }

    std::vector<llvm::Value*> ret;
    ret.reserve(argc);

    for (auto& param : params) {
        if (param.is_self) {
            continue;
        }

        bool found = values.find(param.index) != values.end();;
        llvm::Value* value = found ? values[param.index] : param.default_value;

        ret.push_back(value);
    }

    ret.insert(ret.end(), c_variadic_values.begin(), c_variadic_values.end());
    return ret;
}

Value Visitor::call(
    utils::Ref<Function> function, 
    std::vector<llvm::Value*> args, 
    llvm::Value* self, 
    bool is_constructor,
    llvm::FunctionType* type
) {
    llvm::Value* ret = this->call(function->value, args, self, is_constructor, type);
    if (function->ret.is_reference()) {
        return Value::as_reference(ret, function->ret.type.is_immutable);
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
    std::string name;

    bool is_anonymous = false;
    bool is_llvm_intrinsic = false;

    std::map<std::string, std::string> link;
    if (expr->attributes.has(Attribute::Link)) {
        link = expr->attributes.get(Attribute::Link).as<std::map<std::string, std::string>>();
    }

    std::string export_s = link["export"];
    if (expr->name.empty()) {
        name = FORMAT("__anon.{0}", this->id);
        this->id++;

        is_anonymous = true;
    } else {
        if (
            expr->linkage == ast::ExternLinkageSpecifier::C || 
            this->options.opts.mangle_style == MangleStyle::None
        ) {
            name = expr->name;
        } else {
            name = this->format_name(expr->name);
        }
    }

    if (link.find("name") != link.end()) {
        this->options.add_library(link["name"]);
    }

    if (expr->attributes.has(Attribute::LLVMIntrinsic)) {
        is_llvm_intrinsic = true;
        name = expr->attributes.get(Attribute::LLVMIntrinsic).as<std::string>();
    }

    switch (expr->linkage) {
        case ast::ExternLinkageSpecifier::C:
            if (!is_anonymous) name = expr->name;
        default: break;
    }
    
    Type ret;
    if (!expr->return_type) {
        ret = this->builder->getVoidTy();
    } else {
        ret = expr->return_type->accept(*this).type;
        if (ret->isVoidTy()) {
            NOTE(expr->return_type->span, "Redundant return type. Function return types default to 'void'");
        }
    }

    if (name == this->options.entry && !this->options.standalone) {
        if (!ret->isVoidTy() && !ret->isIntegerTy()) {
            ERROR(expr->span, "Entry point function must either return void or an integer");
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
        Type type = nullptr;
        bool is_immutable = arg.is_immutable;

        if (arg.is_self) {
            llvm::Type* self = this->self;
            if (!self) {
                utils::Ref<Struct> structure = this->current_struct;
                self = structure->type;
            }

            type = Type(self->getPointerTo(), true);
        } else if (arg.is_variadic) {
            type = this->create_variadic_struct(arg.type->accept(*this).type);
        } else {
            type = arg.type->accept(*this).type;
            if (type.is_reference) {
                if (type.is_immutable && !arg.is_immutable) {
                    ERROR(arg.span, "Cannot mark an immutable reference as mutable");
                }

                if (!type.is_immutable && !arg.is_immutable) {
                    NOTE(arg.span, "Parameter type was already marked as mutable. Redundant 'mut' keyword");
                }

                is_immutable = type.is_immutable;
            }
        }

        if (!this->is_valid_sized_type(type.value)) {
            ERROR(arg.type->span, "Cannot define a parameter of type '{0}'", this->get_type_name(type.value));
        }

        llvm::Value* default_value = nullptr;
        if (arg.default_value) {
            this->ctx = type.value;
            default_value = arg.default_value->accept(*this).unwrap(arg.default_value->span);

            if (!llvm::isa<llvm::Constant>(default_value)) {
                ERROR(arg.default_value->span, "Default values must be constants");
            }

            if (!this->is_compatible(type.value, default_value->getType())) {
                ERROR(
                    arg.default_value->span, 
                    "Default value of type '{0}' does not match expected type '{1}'",
                    this->get_type_name(default_value->getType()), this->get_type_name(type.value)
                );
            }

            default_value = this->cast(default_value, type.value);
            this->ctx = nullptr;
        }

        FunctionArgument argument = {
            arg.name, 
            type, 
            default_value, 
            index,
            arg.is_kwarg,
            is_immutable,
            arg.is_self,
            arg.is_variadic,
            arg.span
        };

        if (arg.is_kwarg) {
            kwargs[arg.name] = argument;
        } else {
            args.push_back(argument);
        }

        llvm_args.push_back(type.value); index++;
    }

    llvm::Function::LinkageTypes linkage = llvm::Function::LinkageTypes::ExternalLinkage;

    std::string fn = export_s.empty() ? name : export_s;
    if (this->module->getFunction(fn)) {
        ERROR(expr->span, "Function with the name '{0}' already defined", fn);
    }

    if (
        expr->linkage != ast::ExternLinkageSpecifier::C && 
        name != this->options.entry && 
        !is_llvm_intrinsic &&
        this->options.opts.mangle_style == MangleStyle::Full &&
        !is_anonymous &&
        !export_s.empty()
    ) {
        fn = Mangler::mangle(
            expr->name, 
            llvm_args, 
            expr->is_c_variadic, 
            ret.value, 
            this->current_namespace, 
            this->current_struct, 
            this->current_module
        );
    }

    if (this->is_reserved_function(fn)) {
        ERROR(expr->span, "Function name '{0}' is reserved", fn);
    }

    llvm::Function* function = this->create_function(
        fn, ret.value, llvm_args, expr->is_c_variadic, linkage
    );

    auto func = utils::make_ref<Function>(
        expr->name, 
        args,
        kwargs,
        ret, 
        function,
        (name == this->options.entry), 
        is_llvm_intrinsic,
        is_anonymous,
        expr->is_operator,
        expr->attributes
    );

    if (link.find("section") != link.end()) {
        function->setSection(link["section"]);
    }

    if (func->noreturn) {
        function->addFnAttr(llvm::Attribute::NoReturn);
    }

    if (this->current_struct) {
        func->parent = this->current_struct;
    }

    func->span = expr->span;
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
        NOTE(func->span, "Function '{0}' was previously defined here", func->name);
        ERROR(expr->span, "Function '{0}' is already defined", func->name);
    }

    if (func->is_intrinsic) {
        ERROR(expr->span, "Cannot define intrinsic function '{0}'", func->name);
    }

    auto outer = this->current_function;
    this->current_function = func;

    llvm::BasicBlock* block = llvm::BasicBlock::Create(*this->context, "", function);
    this->set_insert_point(block, false);

    Branch* branch = func->create_branch();
    func->current_branch = branch;

    if (!func->ret.type->isVoidTy()) {
        func->ret.value = this->builder->CreateAlloca(func->ret.type.value);
    }

    func->ret.block = llvm::BasicBlock::Create(*this->context);
    func->scope = this->create_scope(func->name, ScopeType::Function);

    uint32_t i = 0;
    for (auto& arg : func->params()) {
        llvm::Argument* argument = function->getArg(i);
        argument->setName(arg.name);

        if (!arg.is_reference()) {
            llvm::AllocaInst* alloca = this->alloca(*arg.type);
            this->builder->CreateStore(argument, alloca);

            if (arg.type->isStructTy()) {
                auto structure = this->get_struct(*arg.type);
                if (structure->has_method("destructor")) {
                    func->destructors.push_back({ alloca, structure.get() });
                }
            }
            
            func->scope->variables[arg.name] = Variable::from_alloca(
                arg.name, alloca, arg.is_immutable, arg.span
            );
        } else {
            func->scope->variables[arg.name] = Variable::from_value(
                arg.name, argument, arg.is_immutable, true, false, arg.span
            );
        }

        i++;
    }

    if (expr->body.empty()) {
        if (!func->ret->isVoidTy()) {
            ERROR(expr->span, "Function '{0}' expects a return value", func->name);
        }

        this->builder->CreateRetVoid();
    } else {
        for (auto& stmt : expr->body) {
            stmt->accept(*this);
        }

        if (!func->has_return()) {
            if (func->ret->isVoidTy() || func->is_entry) {
                if (func->is_entry) {
                    this->builder->CreateRet(this->builder->getInt32(0));
                } else {
                    this->builder->CreateRetVoid();
                }
            } else {
                ERROR(expr->span, "Function '{0}' expects a return value", func->name);
            }
        } else {
            this->set_insert_point(func->ret.block);
 
            if (func->ret->isVoidTy()) {
                this->builder->CreateRetVoid();
            } else {
                llvm::Value* value = this->builder->CreateLoad(function->getReturnType(), func->ret.value);
                this->builder->CreateRet(value);
            }
        }
    }

    bool error = llvm::verifyFunction(*function, &llvm::errs());
    assert((!error) && "Error while verifying function IR. Most likely a compiler bug.");

    if (this->options.opts.enable) {
        this->fpm->run(*function);
    }

    for (auto& entry : this->scope->variables) {
        Variable& variable = entry.second;
        if (!variable.is_used && !utils::startswith(variable.name, "_")) {
            NOTE(variable.span, "'{0}' is defined but never used", variable.name);
        }

        if (!variable.is_mutated && !variable.is_immutable) {
            NOTE(variable.span, "'{0}' is marked as 'mut' but is never mutated", variable.name);
        }
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
            ERROR(expr->span, "Cannot return a value from void function");
        }

        if (func->ret.type.is_reference) {
            auto ref = this->as_reference(expr->value);
            if (ref.is_null()) {
                ERROR(expr->value->span, "Expected a variable, array or struct member");
            }

            if (ref.is_stack_allocated && ref.is_scope_local) {
                ERROR(expr->value->span, "Cannot return a reference associated with local stack variable '{0}'", ref.name);
            }

            if (ref.is_immutable && !func->ret.type.is_immutable) {
                ERROR(
                    expr->value->span, 
                    "Cannot return immutable reference '{0}' from function expecting mutable reference",
                    ref.name
                );
            }

            llvm::Type* ret = func->ret.type.value->getPointerElementType();
            if (!this->is_compatible(ret, ref.type)) {
                ERROR(
                    expr->value->span,
                    "Cannot return reference value of type '{0}' from function expecting '{1}'",
                    this->get_type_name(ref.type), this->get_type_name(ret)
                );
            }

            func->destruct(*this);

            this->builder->CreateStore(ref.value, func->ret.value);
            this->builder->CreateBr(func->ret.block);

            func->current_branch->has_return = true;
            return nullptr;
        }

        this->ctx = func->ret.type.value;
        Value val = expr->value->accept(*this);
        if (val.is_reference && val.is_stack_allocated) {
            ERROR(expr->value->span, "Cannot return a reference associated with a local stack variable");
        }

        llvm::Value* value = val.unwrap(expr->value->span);
        if (val.is_aggregate) {
            value = this->load(value);
        }

        if (!this->is_compatible(func->ret.type, value->getType())) {
            ERROR(
                expr->span, "Cannot return value of type '{0}' from function expecting '{1}'", 
                this->get_type_name(value->getType()), this->get_type_name(func->ret.type)
            );
        } else {
            value = this->cast(value, func->ret.type);
        }

        func->destruct(*this);

        func->current_branch->has_return = true;
        this->builder->CreateStore(value, func->ret.value);

        this->builder->CreateBr(func->ret.block);
        this->ctx = nullptr;

        return nullptr;
    } else {
        if (!func->ret->isVoidTy()) {
            ERROR(expr->span, "Function '{0}' expects a return value", func->name);
        }

        func->current_branch->has_return = true;
        this->builder->CreateBr(func->ret.block);
        
        return nullptr;
    }
}

Value Visitor::visit(ast::DeferExpr* expr) {
    auto func = this->current_function;
    if (!func) {
        ERROR(expr->span, "Defer statement outside of function");
    }

    TODO("Fix defer statement");
    return nullptr;
}

Value Visitor::visit(ast::CallExpr* expr) {
    Value callable = expr->callee->accept(*this);
    if (callable.builtin) {
        return callable.builtin(*this, expr);
    }

    auto func = callable.function;

    if (!func && !expr->kwargs.empty()) {
        ERROR(expr->span, "Keyword arguments are not allowed in this context");
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
            type = type->getPointerElementType();
            if (!type->isFunctionTy()) {
                ERROR(expr->span, "Expected a function but got value of type '{0}'", this->get_type_name(type));
            }
        } else {
            if (!callable.function) {
                ERROR(expr->span, "Expected a function but got value of type '{0}'", this->get_type_name(type));
            }
        }
    }

    if (callable.self) {
        argc++;
    }

    llvm::Function* function = (llvm::Function*)callable.value;

    std::string name = function->getName().str();
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
        type = type->getPointerElementType();
        if (type->isFunctionTy()) {
            ftype = llvm::cast<llvm::FunctionType>(type);
        } else {
            ERROR(expr->span, "Expected a function but got value of type '{0}'", this->get_type_name(type));
        }
    }

    std::vector<llvm::Value*> args;
    uint32_t i = callable.self ? 1 : 0;

    if (func) {
        args = this->handle_function_arguments(
            expr->span,
            func, 
            callable.self,
            expr->args, 
            expr->kwargs
        );

        if (!this->current_function) {
            this->constructors.push_back(FunctionCall {
                function, args, nullptr
            });

            return Value::as_early_function_call();
        }

        return this->call(function, args, callable.self, is_constructor, ftype);
    }

    if (argc > ftype->getNumParams() && !ftype->isVarArg()) {
        ERROR(expr->span, "Function expects at most {0} arguments but got {1}", ftype->getNumParams(), argc);
    } else if (argc < ftype->getNumParams()) {
        ERROR(expr->span, "Function expects at least {0} arguments but got {1}", ftype->getNumParams(), argc);
    }

    args.reserve(argc);
    for (auto& arg : expr->args) {
        if (i < argc) this->ctx = ftype->getParamType(i);
        llvm::Value* value = arg->accept(*this).unwrap(arg->span);

        if (i < ftype->getNumParams()) {
            if (!this->is_compatible(ftype->getParamType(i), value->getType())) {
                ERROR(
                    arg->span, 
                    "Cannot pass value of type '{0}' to parameter of type '{1}'", 
                    this->get_type_name(value->getType()), this->get_type_name(ftype->getParamType(i))
                );
            } else {
                value = this->cast(value, ftype->getParamType(i));
            }
        }

        args.push_back(value); i++;
    }

    if (!this->current_function) {
        this->constructors.push_back(FunctionCall {
            function, args, nullptr
        });

        return Value::as_early_function_call();
    }


    return this->call(function, args, callable.self, is_constructor, ftype);
}