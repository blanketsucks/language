#include <quart/visitor.h>

using namespace quart;

static const std::vector<std::string> RESERVED_FUNCTION_NAMES = {
    "__global_constructors_init"
};

void Visitor::evaluate_current_scope_defers() {
    for (Scope* parent = this->scope->parent; parent; parent = parent->parent) {
        for (auto& defer : parent->defers) {
            defer->accept(*this);
        }
    }

    for (auto& defer : this->scope->defers) {
        defer->accept(*this);
    }
}

bool Visitor::is_reserved_function(const std::string& name) {
    return llvm::is_contained(RESERVED_FUNCTION_NAMES, name);
}

llvm::Function* Visitor::create_llvm_function(
    const std::string& name, 
    llvm::Type* ret, 
    const std::vector<llvm::Type*>& args, 
    bool is_variadic, 
    llvm::Function::LinkageTypes linkage
) {
    llvm::FunctionType* type = llvm::FunctionType::get(ret, args, is_variadic);
    return llvm::Function::Create(type, linkage, name, this->module.get());
}

static Value evaluate_function_argument(
    Visitor& visitor,
    OwnPtr<ast::Expr>& expr,
    const Parameter& param
) {
    visitor.inferred = param.type;
    if (param.is_reference()) {
        visitor.inferred = param.type->get_reference_type();
    }

    Value value = expr->accept(visitor);
    if (!Type::can_safely_cast_to(value.type, param.type)) {
        ERROR(
            expr->span, 
            "Cannot pass value of type '{0}' to parameter of type '{1}'", 
            value.type->get_as_string(), param.type->get_as_string()
        );
    } else {
        value = visitor.cast(value, param.type);
    }

    visitor.inferred = nullptr;
    return value;
}

std::vector<llvm::Value*> Visitor::handle_function_arguments(
    const Span& span,
    Function const& function,
    llvm::Value* self,
    std::vector<OwnPtr<ast::Expr>>& args,
    std::map<std::string, OwnPtr<ast::Expr>>& kwargs
) {
    u32 argc = args.size() + kwargs.size() + (self ? 1 : 0);
    bool is_variadic = function.is_variadic() || function.is_c_variadic();

    if (function.has_any_default_value()) {
        if (argc + function.get_default_arguments_count() < function.argc()) {
            ERROR(span, "Function expects at least {0} arguments but got {1}", function.argc(), argc);
        } else if (argc > function.argc() && !is_variadic) {
            ERROR(span, "Function expects at most {0} arguments but got {1}", function.argc(), argc);
        }
    } else {
        if (argc < function.argc() || (argc > function.argc() && !is_variadic)) {
            ERROR(span, "Function expects {0} arguments but got {1}", function.argc(), argc);
        }
    }

    if (!this->current_function && function.flags & Function::NoReturn) {
        ERROR(span, "Cannot call noreturn function '{0}' from global scope", function.name);
    }

    std::map<i64, llvm::Value*> values;
    std::vector<llvm::Value*> c_variadic_values;

    u32 i = self ? 1 : 0;

    std::vector<Parameter> params = function.params;
    for (auto& entry : function.kwargs) params.push_back(entry.second);

    for (auto& arg : args) {
        if (i >= function.params.size()) {
            if (!is_variadic) {
                ERROR(arg->span,  "Function expects {0} positional arguments but got {1}", function.params.size(), i + 1);
            }

            Value value = arg->accept(*this);
            if (value.is_empty_value()) ERROR(arg->span, "Expected a value");

            c_variadic_values.push_back(value);
            continue;
        }

        const Parameter& param = function.params[i];
        Value value = evaluate_function_argument(*this, arg, param);

        values[i] = value;
        i++;
    }

    for (auto& entry : kwargs) {
        if (!function.has_keyword_parameter(entry.first)) {
            ERROR(entry.second->span, "Function does not have a keyword parameter named '{0}'", entry.first);
        }

        const Parameter& param = function.kwargs.at(entry.first);
        Value value = evaluate_function_argument(*this, entry.second, param);

        values[param.index] = value;
    }

    std::vector<llvm::Value*> ret;
    ret.reserve(argc);

    for (auto& param : params) {
        if ((param.flags & Parameter::Self) && self) { // We want to allow calls like Foo::bar(a) to work
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
    Function const& function, 
    std::vector<llvm::Value*> args, 
    llvm::Value* self, 
    bool is_constructor,
    llvm::FunctionType* type
) {
    llvm::Value* result = this->call(function.value, args, self, is_constructor, type);
    if (function.get_return_type()->is_reference()) {
        return { result, function.get_return_type() };
    }

    return result;
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

Parameter Visitor::parse_function_parameter(ast::Argument& argument, u32 index) {
    quart::Type* type = nullptr;
    bool is_mutable = argument.is_mutable;

    if (argument.is_self) {
        quart::Type* self = this->self;
        if (!self) {
            RefPtr<Struct> structure = this->current_struct;
            self = structure->type;
        }

        type = self->get_pointer_to(is_mutable);
    } else if (argument.is_variadic) {
        TODO("Support for `*` variadics");
    } else {
        type = argument.type->accept(*this);
        if (!type->is_sized_type()) {
            ERROR(argument.type->span, "Cannot define a parameter of type '{0}'", type->get_as_string());
        }

        if (type->is_reference() || type->is_pointer()) {
            if (!type->is_mutable() && argument.is_mutable) {
                ERROR(argument.span, "Cannot mark an immutable type as mutable");
            }

            if (type->is_mutable() && argument.is_mutable) {
                NOTE(argument.span, "Parameter type was already marked as mutable. Redundant 'mut' keyword");
            }

            is_mutable = type->is_mutable();
        }
    }

    llvm::Value* default_value = nullptr;
    if (argument.default_value) {
        this->inferred = type;

        Value value = argument.default_value->accept(*this);
        if (value.is_empty_value()) {
            ERROR(argument.default_value->span, "Expected a constant value");
        }

        if (!llvm::isa<llvm::Constant>(value.inner)) {
            ERROR(argument.default_value->span, "Default values must be constants");
        }

        if (!Type::can_safely_cast_to(value.type, type)) {
            ERROR(
                argument.default_value->span, 
                "Cannot pass value of type '{0}' to parameter of type '{1}'", 
                value.type->get_as_string(), type->get_as_string()
            );
        }

        default_value = this->cast(value, type);
        this->inferred = nullptr;
    }

    u8 flags = Parameter::None;

    if (is_mutable) flags |= Parameter::Mutable;
    if (argument.is_self) flags |= Parameter::Self;
    if (argument.is_variadic) flags |= Parameter::Variadic;
    if (argument.is_kwarg) flags |= Parameter::Keyword;

    return {
        argument.name, 
        type, 
        default_value,
        flags,
        index,
        argument.span
    };
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
            name = this->format_symbol(expr->name);
        }
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
    
    quart::Type* ret = nullptr;
    if (!expr->return_type) {
        ret = this->registry->get_void_type();
    } else {
        ret = expr->return_type->accept(*this);
        if (ret->is_void()) {
            NOTE(expr->return_type->span, "Redundant return type. Function return types default to 'void'");
        }
    }

    std::vector<Parameter> params;
    std::map<std::string, Parameter> kwargs;

    std::vector<llvm::Type*> llvm_params;
    std::vector<quart::Type*> types;

    u32 index = 0;
    for (auto& argument : expr->args) {
        Parameter parameter = this->parse_function_parameter(argument, index);
        if (argument.is_kwarg) {
            kwargs[argument.name] = parameter;
        } else {
            params.push_back(parameter);
        }

        llvm::Type* ltype = parameter.type->to_llvm_type();

        llvm_params.push_back(ltype);
        types.push_back(parameter.type);

        index++;
    }

    if (link.find("name") != link.end()) {
        this->options.add_library_name(link["name"]);
    }

    if (name == this->options.entry) {
        if (!ret->is_void() && !ret->is_int()) {
            ERROR(expr->span, "Entry function must return an integer value");
        }

        if (ret->is_void()) ret = this->registry->create_int_type(32, true);
    }

    llvm::Function::LinkageTypes linkage = llvm::Function::LinkageTypes::ExternalLinkage;
    std::string fn = export_s.empty() ? name : export_s;

    if (this->module->getFunction(fn)) {
        auto function = this->functions[fn];

        ERROR_WITH_NOTE(
            expr->span, FORMAT("Function with the name '{0}' already defined", fn),
            function->span, FORMAT("'{0}' was previously defined here", fn)
        );
    }

    if (this->is_reserved_function(fn)) {
        ERROR(expr->span, "Function name '{0}' is reserved", fn);
    }

    llvm::Function* function = this->create_llvm_function(
        fn, ret->to_llvm_type(), llvm_params, expr->is_c_variadic, linkage
    );

    u16 flags = Function::None;

    if (name == this->options.entry) flags |= Function::Entry;
    if (expr->is_operator) flags |= Function::Operator;
    if (is_llvm_intrinsic) flags |= Function::LLVMIntrinsic;
    if (is_anonymous) flags |= Function::Anonymous;

    quart::Type* type = this->registry->create_function_type(ret, types)->get_pointer_to(false);
    RefPtr<Function> func = Function::create(
        function, type, expr->name, params, kwargs,
        ret, flags, expr->span, expr->attributes
    );

    if (link.find("section") != link.end()) {
        function->setSection(link["section"]);
    }

    if (func->flags & Function::NoReturn) {
        function->addFnAttr(llvm::Attribute::NoReturn);
    }

    this->scope->functions[expr->name] = func;
    this->functions[fn] = func;

    if (this->current_struct) func->parent = this->current_struct.get();

    return function;
}

Value Visitor::visit(ast::FunctionExpr* expr) {
    auto iterator = this->scope->functions.find(expr->prototype->name);
    RefPtr<Function> func = nullptr;

    if (iterator == this->scope->functions.end()) {
        expr->prototype->attributes.update(expr->attributes);

        expr->prototype->accept(*this);
        iterator = this->scope->functions.find(expr->prototype->name);
    }

    func = iterator->second;
    llvm::Function* function = func->value;

    if (func->flags & Function::LLVMIntrinsic) {
        ERROR(expr->span, "Cannot define intrinsic function '{0}'", func->name);
    }

    auto outer = this->current_function;
    this->current_function = func;

    llvm::BasicBlock* block = llvm::BasicBlock::Create(*this->context, "", function);
    this->set_insert_point(block, false);

    if (!func->ret->is_void()) {
        llvm::Type* type = func->ret->to_llvm_type();

        func->ret.value = this->builder->CreateAlloca(type);
        func->ret.block = llvm::BasicBlock::Create(*this->context, "ret");
    }

    func->scope = Scope::create(func->name, ScopeType::Function, this->scope);
    this->push_scope(func->scope);

    std::vector<Parameter> params = func->params;
    for (auto& entry : func->kwargs) params.push_back(entry.second);

    u32 i = 0;
    for (auto& param : params) {
        llvm::Argument* argument = function->getArg(i);
        argument->setName(param.name);

        u8 flags = param.flags & Parameter::Mutable ? Variable::Mutable : Variable::None;
        if (!param.is_reference()) {
            llvm::Type* type = param.type->to_llvm_type();
            llvm::AllocaInst* alloca = this->alloca(type);

            this->builder->CreateStore(argument, alloca);
            auto variable = Variable::from_alloca(
                param.name, alloca, param.type, flags, param.span
            );
            
            if (!(param.flags & Parameter::Self)) variable.flags &= ~Variable::StackAllocated;
            func->scope->variables[param.name] = variable;
        } else {
            flags |= Variable::Reference;
            func->scope->variables[param.name] = Variable::from_value(
                param.name, argument, param.type, flags, param.span
            );
        }

        i++;
    }

    if (expr->body.empty()) {
        if (!func->ret->is_void()) {
            ERROR(expr->span, "Function '{0}' expects a return value", func->name);
        }

        this->builder->CreateRetVoid();
    } else {
        for (auto& stmt : expr->body) {
            stmt->accept(*this);
        }

        for (auto& block : func->value->getBasicBlockList()) {
            if (block.getTerminator()) continue;

            if (func->flags & Function::Entry) {
                this->builder->SetInsertPoint(&block);
                this->evaluate_current_scope_defers();

                llvm::Type* result = function->getReturnType();
                this->builder->CreateRet(this->builder->getIntN(result->getIntegerBitWidth(), 0));

                continue;
            }

            if (!func->ret->is_void()) {
                ERROR(func->span, "Function '{0}' expects a return value from all branches", func->name);
            }

            this->builder->SetInsertPoint(&block);
            this->evaluate_current_scope_defers();

            this->builder->CreateRetVoid();
        }

        if (!func->ret->is_void()) {
            this->set_insert_point(func->ret.block);

            llvm::Value* value = this->builder->CreateLoad(function->getReturnType(), func->ret.value);
            this->builder->CreateRet(value);
        }
    }
    
    bool error = llvm::verifyFunction(*function, &llvm::errs());
    assert((!error) && "Error while verifying function IR. Most likely a compiler bug.");

    if (this->options.opts.enable) {
        this->fpm->run(*function);
    }

    for (auto& entry : this->scope->variables) {
        Variable& variable = entry.second;
        if (!variable.is_used() && !variable.can_ignore_usage()) {
            NOTE(variable.span, "'{0}' is defined but never used", variable.name);
        }

        if (!variable.is_mutated() && variable.is_mutable()) {
            NOTE(variable.span, "'{0}' is marked as 'mut' but is never mutated", variable.name);
        }
    }

    this->pop_scope();

    this->current_function = outer;
    if (outer) {
        this->builder->SetInsertPoint(outer->current_block);
    }

    return { function, func->type, Value::Constant };
}

Value Visitor::visit(ast::ReturnExpr* expr) {
    auto func = this->current_function;
    if (expr->value) {
        if (func->ret->is_void()) {
            ERROR(expr->span, "Cannot return a value from void function");
        }

        func->flags |= Function::HasReturn;
        this->inferred = func->ret.type;

        Value value = expr->value->accept(*this);
        if (value.is_empty_value()) ERROR(expr->value->span, "Expected a value");

        this->inferred = nullptr;
        if (value.is_reference() && value.flags & Value::StackAllocated) {
            ERROR(expr->value->span, "Cannot return a reference associated with a local stack variable");
        }

        if (value.flags & Value::Aggregate) {
            value = {this->load(value), value.type};
        }

        if (!Type::can_safely_cast_to(value.type, func->ret.type)) {
            ERROR(
                expr->span, "Cannot return value of type '{0}' from function expecting '{1}'", 
                value.type->get_as_string(), func->ret->get_as_string()
            );
        } else {
            value = this->cast(value, func->ret.type);
        }

        this->evaluate_current_scope_defers();

        this->builder->CreateStore(value, func->ret.value);
        this->builder->CreateBr(func->ret.block);

        return nullptr;
    } else {
        if (func->flags & Function::Entry) {
            this->evaluate_current_scope_defers();

            llvm::Type* result = func->value->getReturnType();
            this->builder->CreateRet(this->builder->getIntN(result->getIntegerBitWidth(), 0));

            return nullptr;
        }

        if (!func->ret->is_void()) {
            ERROR(expr->span, "Function '{0}' expects a return value", func->name);
        }

        this->evaluate_current_scope_defers();

        func->flags |= Function::HasReturn;
        this->builder->CreateRetVoid();
        
        return nullptr;
    }
}

Value Visitor::visit(ast::DeferExpr* expr) {
    auto func = this->current_function;
    if (!func) {
        ERROR(expr->span, "Defer statement outside of function");
    }

    this->scope->defers.push_back(expr->expr.get());
    return nullptr;
}

Value Visitor::visit(ast::CallExpr* expr) {
    Value callable = expr->callee->accept(*this);
    if (callable.flags & Value::Builtin) {
        auto builtin = callable.as<BuiltinFunction>();
        return builtin(*this, expr);
    }

    Function* func = nullptr;
    if (callable.isa<Function*>()) {
        func = callable.as<Function*>();
        if (!callable.type) {
            callable.type = func->type;
        }
    }

    if (!func && !expr->kwargs.empty()) {
        ERROR(expr->span, "Keyword arguments are not allowed in this context");
    }

    if (func) func->flags |= Function::Used;

    size_t argc = expr->args.size() + expr->kwargs.size();
    bool is_constructor = false;

    quart::Type* type = callable.type;
    if (type->is_pointer()) {
        type = type->get_pointee_type();
        if (!type->is_function()) {
            ERROR(expr->span, "Expected a function but got value of type '{0}'", type->get_as_string());
        }
    } else {
        if (!func) {
            ERROR(expr->span, "Expected a function but got value of type '{0}'", type->get_as_string());
        }
    }

    if (callable.self) argc++;

    auto function = static_cast<llvm::Function*>(callable.inner);
    std::string name = function->getName().str();

    llvm::FunctionType* ftype = nullptr;
    llvm::Type* ty = function->getType();

    if (ty->isPointerTy()) {
        ftype = llvm::cast<llvm::FunctionType>(ty->getPointerElementType());
    } else {
        ftype = llvm::cast<llvm::FunctionType>(ty);
    }

    if (func) {
        name = func->name;
        if (this->current_function) {
            this->current_function->calls.push_back(function);
        }
    }

    std::vector<llvm::Value*> args;
    u32 i = callable.self ? 1 : 0;

    if (func) {
        args = this->handle_function_arguments(
            expr->span,
            *func, 
            callable.self,
            expr->args, 
            expr->kwargs
        );

        if (!this->current_function) {
            this->early_function_calls.push_back(EarlyFunctionCall {
                function, args, callable.self, nullptr
            });
            
            return { nullptr, Value::EarlyFunctionCall };
        }

        llvm::Value* result = this->call(function, args, callable.self, is_constructor, ftype);
        return { result, func->get_return_type() };
    }

    if (argc > ftype->getNumParams() && !ftype->isVarArg()) {
        ERROR(expr->span, "Function expects at most {0} arguments but got {1}", ftype->getNumParams(), argc);
    } else if (argc < ftype->getNumParams()) {
        ERROR(expr->span, "Function expects at least {0} arguments but got {1}", ftype->getNumParams(), argc);
    }

    args.reserve(argc);
    for (auto& arg : expr->args) {
        if (i < argc) this->inferred = type->get_function_param(i);

        Value value = arg->accept(*this);
        if (value.is_empty_value()) ERROR(arg->span, "Expected a value");

        if (i < ftype->getNumParams()) {
            // TODO: This assumes that `arg->accept` doesn't change `this->inferred`
            if (!Type::can_safely_cast_to(value.type, this->inferred)) {
                ERROR(
                    arg->span, 
                    "Cannot pass value of type '{0}' to parameter of type '{1}'", 
                    value.type->get_as_string(), this->inferred->get_as_string()
                );
            } else {
                value = this->cast(value, this->inferred);
            }
        }

        args.push_back(value); i++;
        this->inferred = nullptr;
    }

    if (!this->current_function) {
        this->early_function_calls.push_back(EarlyFunctionCall {
            function, args, callable.self, nullptr
        });
        
        return { nullptr, Value::EarlyFunctionCall };
    }

    return {
        this->call(function, args, callable.self, is_constructor, ftype),
        type->get_function_return_type()
    };
}