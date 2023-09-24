#include <quart/visitor.h>

using namespace quart;

Value Visitor::visit(ast::VariableExpr* expr) {
    if (expr->name == "null") {
        quart::Type* type = this->registry->create_int_type(1, true);
        if (this->inferred) type = this->inferred;

        return Value(
            llvm::Constant::getNullValue(type->to_llvm_type()),
            type, 
            Value::Constant
        );
    }

    Scope* scope = this->scope;
    auto local = scope->get_local(expr->name, bool(this->current_function));
    if (local.value) {
        if (!this->current_function) {
            uint16_t flags = local.flags & ScopeLocal::Constant ? Value::Constant : Value::None;
            return Value(local.value, local.type, flags);
        }

        return Value(this->load(local.value), local.type);
    }

    if (scope->has_struct(expr->name)) {
        auto structure = scope->get_struct(expr->name);
        return Value(nullptr, Value::Struct, structure.get());
    } else if (scope->has_enum(expr->name)) {
        auto enumeration = scope->get_enum(expr->name);
        return Value(nullptr, Value::Scope, enumeration->scope);
    } else if (scope->has_function(expr->name)) {
        auto function = scope->get_function(expr->name);
        return Value(function->value, function->type, Value::Function | Value::Constant, function.get());
    } else if (scope->has_module(expr->name)) {
        auto module = scope->get_module(expr->name);
        return Value(nullptr, Value::Scope, module->scope);
    }

    if (this->builtins.count(expr->name)) {
        return Value(nullptr, Value::Builtin, this->builtins[expr->name]);
    }

    ERROR(expr->span, "Undefined variable '{0}'", expr->name);
}

Value Visitor::visit(ast::VariableAssignmentExpr* expr) {
    quart::Type* type = nullptr;
    if (expr->external) {
        std::string name = expr->names[0].value;

        type = expr->type->accept(*this);
        this->module->getOrInsertGlobal(name, type->to_llvm_type());

        llvm::GlobalVariable* global = this->module->getGlobalVariable(name);
        global->setLinkage(llvm::GlobalValue::ExternalLinkage);

        return EMPTY_VALUE;
    }

    Value value = EMPTY_VALUE;

    bool is_constant_value = false;
    bool has_initializer = !!expr->value;

    if (!expr->value) {
        type = expr->type->accept(*this);
        llvm::Type* ltype = type->to_llvm_type();

        if (type->is_aggregate()) {
            value = llvm::ConstantAggregateZero::get(ltype);
        } else if (type->is_pointer()) {
            value = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ltype));
        } else {
            value = llvm::Constant::getNullValue(ltype);
        }

        is_constant_value = true;
    } else {
        if (expr->type) {
            type = expr->type->accept(*this);
            this->inferred = type;
        }

        value = expr->value->accept(*this); 
        quart::Type* vtype = nullptr;

        if (!(value.flags & Value::EarlyFunctionCall)) {
            if (value.is_empty_value()) {
                ERROR(expr->value->span, "Expected an expression");
            }

            vtype = value.type;
            if (!type) type = vtype;
        } else {
            llvm::Type* return_type = this->early_function_calls.back().function->getReturnType();
            vtype = this->registry->wrap(return_type);

            if (!type) type = vtype;
            value = llvm::Constant::getNullValue(return_type);
        }

        if (type->is_void()) {
            ERROR(expr->value->span, "Cannot store value of type 'void'");
        }

        if (!Type::can_safely_cast_to(vtype, type)) {
            ERROR(
                expr->value->span, 
                "Expected expression of type '{0}' but got '{1}' instead", 
                type->get_as_string(), vtype->get_as_string()
            );
        } else {
            // TODO: Somehow be able to cast in this case
            if ((value.flags & Value::EarlyFunctionCall) && (type != vtype)) {
                ERROR(
                    expr->value->span, 
                    "Expected expression of type '{0}' but got '{1}' instead", 
                    type->get_as_string(), vtype->get_as_string()
                );
            } else {
                value = this->cast(value, type);
            }
        }

        this->inferred = nullptr;
    }

    if (!expr->is_multiple_variables) {
        ast::Ident& ident = expr->names[0];
        if (!this->current_function) {
            if (!llvm::isa<llvm::Constant>(value.inner)) {
                ERROR(expr->value->span, "Cannot store non-constant value in a global variable");
            }

            std::string name = FORMAT("__global.{0}", ident.value);
            this->module->getOrInsertGlobal(name, type->to_llvm_type());

            llvm::GlobalVariable* global = this->module->getGlobalVariable(name);
            llvm::Constant* constant = llvm::cast<llvm::Constant>(value.inner);

            global->setInitializer(constant);
            if (!(value.flags & Value::EarlyFunctionCall)) {
                global->setLinkage(llvm::GlobalValue::PrivateLinkage);
            } else {
                auto& call = this->early_function_calls.back();
                call.store = global;
            }

            uint8_t flags = ident.is_mutable ? Variable::Mutable : Variable::None;
            this->scope->variables[ident.value] = Variable { 
                ident.value, 
                type, 
                global, 
                constant, 
                flags,
                expr->span
            };

            return nullptr;
        }

        // We only want to check for immutability if the type is a reference/pointer while ignoring aggregates
        if ((ident.is_mutable && type->is_mutable()) && (type->is_reference() || type->is_pointer()) && !(value.flags & Value::Aggregate)) {
            ERROR(expr->value->span, "Cannot assign immutable value to mutable variable '{0}'", ident.value);
        }

        if (type->is_reference()) {
            uint8_t flags = ident.is_mutable ? Variable::Mutable : Variable::None;
            this->scope->variables[ident.value] = Variable::from_value(
                ident.value, value, type, flags, ident.span
            );

            return nullptr;
        }

        llvm::Type* ltype = type->to_llvm_type();
        llvm::Value* alloca = this->alloca(ltype);

        if (is_constant_value && type->is_aggregate() && has_initializer) {
            std::string name = FORMAT("__const.{0}.{1}", this->current_function->name, ident.value);
            this->module->getOrInsertGlobal(name, ltype);

            llvm::GlobalVariable* global = this->module->getGlobalVariable(name);
            
            global->setLinkage(llvm::GlobalVariable::PrivateLinkage);
            global->setInitializer(llvm::cast<llvm::Constant>(value.inner));

            this->builder->CreateMemCpy(
                alloca, llvm::MaybeAlign(0),
                global, llvm::MaybeAlign(0),
                this->getsizeof(ltype)
            );
        } else {
            if (!has_initializer) {
                this->builder->CreateMemSet(
                    alloca, this->builder->getInt8(0), 
                    this->getsizeof(ltype), llvm::MaybeAlign(0)
                );
            } else if (value.flags & Value::Aggregate) {
                alloca = value;
            } else {
                this->builder->CreateStore(value, alloca);
            }

            if (value.flags & Value::EarlyFunctionCall) {
                auto& call = this->early_function_calls.back();
                call.store = alloca;
            }
        }

        uint8_t flags = ident.is_mutable ? Variable::Mutable : Variable::None;
        flags |= Variable::StackAllocated;

        this->scope->variables[ident.value] = Variable {
            .name = ident.value,
            .type = type,
            .value = alloca,
            .constant = is_constant_value ? llvm::cast<llvm::Constant>(value.inner) : nullptr,
            .flags = flags,
            .span = ident.span
        };
    } else {
        this->store_tuple(expr->value->span, this->current_function, value, expr->names, expr->consume_rest);
    }
    
    return value;
}

Value Visitor::visit(ast::ConstExpr* expr) {
    quart::Type* type = nullptr;
    if (expr->type) {
        type = expr->type->accept(*this);
        this->inferred = type;
    }

    Value value = expr->value->accept(*this);
    if (!type && value.type) type = value.type;

    llvm::Type* ltype = nullptr;
    this->inferred = nullptr;
    if (!(value.flags & Value::EarlyFunctionCall)) {
        if (value.is_empty_value()) {
            ERROR(expr->value->span, "Expected an expression");
        }

        ltype = type->to_llvm_type();
    } else {
        llvm::Type* return_type = this->early_function_calls.back().function->getReturnType();
        type = this->registry->wrap(return_type);

        ltype = return_type;
    }

    std::string name = FORMAT("__const.{0}", this->format_symbol(expr->name));

    this->module->getOrInsertGlobal(name, ltype);
    llvm::GlobalVariable* global = this->module->getNamedGlobal(name);

    llvm::Constant* constant = nullptr;
    if (!(value.flags & Value::EarlyFunctionCall)) {
        constant = llvm::cast<llvm::Constant>(value.inner);
    } else {
        constant = llvm::Constant::getNullValue(ltype);
    }

    global->setInitializer(constant);
    global->setLinkage(llvm::GlobalVariable::PrivateLinkage);

    if (value.flags & Value::EarlyFunctionCall) {
        auto& call = this->early_function_calls.back();
        call.store = global;
    }

    this->scope->constants[expr->name] = Constant {
        name, type, global, constant, expr->span
    };
    
    return EMPTY_VALUE;
}