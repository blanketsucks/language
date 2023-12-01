#include <quart/visitor.h>

#include <llvm/IR/Instructions.h>

#include <functional>

#define MATCH_TYPE(type, method, ...) case ast::BuiltinType::type: return this->registry->method(__VA_ARGS__)

using namespace quart;

static const std::map<std::string, u32> TYPE_SIZES = {
    {"i8", 1},
    {"i16", 2},
    {"i32", 4},
    {"i64", 8},
    {"i128", 16},
    {"f32", 4},
    {"f64", 8},
    {"bool", 1},
    {"char", 1},
    {"void", 0}
};

static TypeAlias* resolve_type_alias(
    Visitor& visitor, const std::string& name, std::deque<std::string> parents
) {
    Scope* scope = visitor.scope;
    while (!parents.empty()) {
        std::string name = parents.front();
        parents.pop_front();

        if (scope->has_module(name)) {
            scope = scope->get_module(name)->scope;
        } else {
            return nullptr;
        }
    }

    return scope->get_type_alias(name);
}

u32 Visitor::getsizeof(llvm::Value* value) {
    return this->getsizeof(value->getType());
}

u32 Visitor::getsizeof(quart::Type* type) {
    return this->getsizeof(type->to_llvm_type());
}

u32 Visitor::getallocsize(llvm::Type* type) {
    llvm::TypeSize tsize = this->module->getDataLayout().getTypeAllocSize(type);
    return tsize.getFixedSize();
}

u32 Visitor::getsizeof(llvm::Type* type) {
    if (type->isPointerTy() || type->isStructTy()) {
        return this->getallocsize(type);
    } else if (type->isArrayTy()) {
        return type->getArrayNumElements() * this->getsizeof(type->getArrayElementType());
    }

    return static_cast<u32>(type->getPrimitiveSizeInBits() / 8);
}

Value Visitor::cast(const Value& value, quart::Type* to) {
    if (value.type == to) {
        return value;
    }

    llvm::Value* result = nullptr;
    switch (to->kind()) {
        case TypeKind::Int:
            result = this->builder->CreateIntCast(value, to->to_llvm_type(), !to->is_int_unsigned()); break;
        case TypeKind::Float:
            result = this->builder->CreateFPCast(value, to->to_llvm_type()); break;
        default:
            result = this->builder->CreateBitCast(value, to->to_llvm_type()); break;
    }

    return { result, to };
}

quart::Type* Visitor::get_builtin_type(ast::BuiltinType value) {
    switch (value) {
        MATCH_TYPE(Void, get_void_type);
        MATCH_TYPE(f32, get_f32_type);
        MATCH_TYPE(f64, get_f64_type);

        MATCH_TYPE(Bool, create_int_type, 1, true);
        MATCH_TYPE(i8, create_int_type, 8, true);
        MATCH_TYPE(i16, create_int_type, 16, true);
        MATCH_TYPE(i32, create_int_type, 32, true);
        MATCH_TYPE(i64, create_int_type, 64, true);
        MATCH_TYPE(i128, create_int_type, 128, true);

        MATCH_TYPE(u8, create_int_type, 8, false);
        MATCH_TYPE(u16, create_int_type, 16, false);
        MATCH_TYPE(u32, create_int_type, 32, false);
        MATCH_TYPE(u64, create_int_type, 64, false);
        MATCH_TYPE(u128, create_int_type, 128, false);

        default: __builtin_unreachable();
    }
}

Value Visitor::visit(ast::CastExpr* expr) {
    Value value = expr->value->accept(*this);
    if (value.is_empty_value()) {
        ERROR(expr->value->span, "Expected an expression");
    }

    quart::Type* from = value.type;
    quart::Type* to = expr->to->accept(*this);

    if (from == to) {
        return value;
    }

    const char* err = "Invalid cast. Cannot cast value of type '{0}' to '{1}'";
    llvm::Type* type = to->to_llvm_type();

    if (from->is_int()) {
        if (to->is_floating_point()) {
            llvm::Value* result = nullptr;
            if (from->is_int_unsigned()) {
                result = this->builder->CreateUIToFP(value, type);
            } else {
                result = this->builder->CreateSIToFP(value, type);
            }

            return { result, to };
        } else if (to->is_int()) {
            u32 bits = from->get_int_bit_width();
            if (bits < to->get_int_bit_width()) {
                return {this->builder->CreateZExt(value, type), to};
            } else if (bits > to->get_int_bit_width()) {
                return {this->builder->CreateTrunc(value, type), to};
            }

            return { value, to };
        } else if (to->is_pointer()) {
            return { this->builder->CreateIntToPtr(value, type), to };
        }
            
        ERROR(expr->span, err, from->get_as_string(), to->get_as_string());
    } else if (from->is_floating_point()) {
        if (to->is_floating_point()) {
            return this->builder->CreateFPCast(value, type);
        } else if (to->is_int()) {
            llvm::Value* result = nullptr;
            if (to->is_int_unsigned()) {
                result = this->builder->CreateFPToUI(value, type);
            } else {
                result = this->builder->CreateFPToSI(value, type);
            }

            return { result, to };
        }
            
        ERROR(expr->span, err, from->get_as_string(), to->get_as_string());
    } else if (from->is_pointer()) {
        if (to->is_int()) {
            return { this->builder->CreatePtrToInt(value, type), to };
        } else if (to->is_pointer()) {
            return { this->builder->CreateBitCast(value, type), to };
        }
        
        ERROR(expr->span, err, from->get_as_string(), to->get_as_string());
    } else if (from->is_reference()) {
        if (!to->is_pointer()) {
            ERROR(expr->span, err, from->get_as_string(), to->get_as_string());
        }

        return { this->builder->CreateBitCast(value, type), to };
    }

    if (from->is_enum()) {
        quart::Type* inner = from->get_inner_enum_type();
        if (inner == to) {
            return { value, to };
        }

        return this->cast(value, inner);
    }

    ERROR(expr->span, err, from->get_as_string(), to->get_as_string());
}

Value Visitor::visit(ast::SizeofExpr* expr) {
    quart::Type* type = this->registry->create_int_type(sizeof(i32), true);
    u32 size = 0;

    if (expr->value->kind() == ast::ExprKind::Variable) {
        auto id = expr->value->as<ast::VariableExpr>();

        auto iterator = TYPE_SIZES.find(id->name);
        if (iterator != TYPE_SIZES.end()) {
            return { this->builder->getInt32(iterator->second), type };
        }
    } 

    Value value = expr->value->accept(*this);
    if (value.flags & Value::Struct) {
        auto structure = value.as<quart::Struct*>();
        size = this->getsizeof(structure->type);
    } else {
        if (value.is_empty_value()) {
            ERROR(expr->value->span, "Expected an expression");
        }

        size = this->getsizeof(value.inner);
    }

    return {
        this->builder->getInt32(size),
        this->registry->create_int_type(32, true),
        Value::Constant
    };
}

quart::Type* Visitor::visit(ast::BuiltinTypeExpr* expr) {
    return this->get_builtin_type(expr->value);
}

quart::Type* Visitor::visit(ast::IntegerTypeExpr* expr) {
    Value value = expr->size->accept(*this);
    if (value.is_empty_value()) {
        ERROR(expr->size->span, "Expected an expression");
    }

    auto constant = llvm::dyn_cast<llvm::ConstantInt>(value.inner);
    if (!constant) {
        ERROR(expr->size->span, "Integer type size must be a constant");
    }

    i64 size = constant->getSExtValue();
    if (size < 1 || size > quart::IntType::MAX_BITS) {
        ERROR(expr->size->span, "Integer type size must be between 1 and {0} bits", quart::IntType::MAX_BITS);
    }

    return this->registry->create_int_type(size, true);
}

quart::Type* Visitor::visit(ast::NamedTypeExpr* expr) {
    Scope* scope = this->scope;
    while (!expr->parents.empty()) {
        std::string name = expr->parents.front();
        expr->parents.pop_front();

        if (scope->has_module(name)) {
            scope = scope->get_module(name)->scope;
        } else {
            ERROR(expr->span, "Undefined namespace '{0}'", name);
        }
    }

    if (scope->has_struct(expr->name)) {
        auto structure = scope->get_struct(expr->name);
        return structure->type;
    } else if (scope->has_type_alias(expr->name)) {
        TypeAlias* alias = scope->get_type_alias(expr->name);
        if (alias->is_generic() && !alias->is_instantiable_without_args()) {
            ERROR(expr->span, "Expected type arguments for type '{0}'", expr->name);
        } else if (alias->is_generic() && alias->is_instantiable_without_args()) {
            return alias->instantiate(*this);
        }

        return alias->type;
    } else if (scope->has_enum(expr->name)) {
        return scope->get_enum(expr->name)->type;
    }

    ERROR(expr->span, "Undefined type '{0}'", expr->name);
}

quart::Type* Visitor::visit(ast::PointerTypeExpr* expr) {
    quart::Type* type = expr->type->accept(*this);
    if (type->is_pointer()) {
        // If the type that it's pointing to is a function, we don't want to double up on the pointer
        quart::Type* pointee = type->get_pointee_type();
        if (pointee->is_function()) {
            return type;
        }
    }

    return type->get_pointer_to(expr->is_mutable);
}

quart::Type* Visitor::visit(ast::ArrayTypeExpr* expr) {
    quart::Type* type = expr->type->accept(*this);
    if (type->is_void()) {
        ERROR(expr->span, "Cannot create an array of type 'void'");
    }

    Value value = expr->size->accept(*this);
    if (value.is_empty_value()) {
        ERROR(expr->size->span, "Expected an expression");
    }

    auto constant = llvm::dyn_cast<llvm::ConstantInt>(value.inner);
    if (!constant) {
        ERROR(expr->size->span, "Array size must be a constant integer");
    }

    return this->registry->create_array_type(type, constant->getSExtValue());
}

quart::Type* Visitor::visit(ast::TupleTypeExpr* expr) {
    std::vector<quart::Type*> types;
    for (auto& ty : expr->types) {
        quart::Type* type = ty->accept(*this);
        if (type->is_void()) {
            ERROR(ty->span, "Cannot create a tuple with a 'void' element");
        }

        types.push_back(type);
    }

    return this->registry->create_tuple_type(types);
}

quart::Type* Visitor::visit(ast::FunctionTypeExpr* expr) {
    quart::Type* return_type = this->registry->get_void_type();
    if (expr->ret) {
        return_type = expr->ret->accept(*this);
    }

    std::vector<quart::Type*> params;
    for (auto& param : expr->args) {
        quart::Type* type = param->accept(*this);
        if (type->is_void()) {
            ERROR(param->span, "Function parameter type cannot be of type 'void'");
        }

        params.push_back(type);
    }

    quart::FunctionType* f = this->registry->create_function_type(return_type, params);
    return f->get_pointer_to(false);
}

quart::Type* Visitor::visit(ast::ReferenceTypeExpr* expr) {
    quart::Type* type = expr->type->accept(*this);
    if (type->is_void()) {
        ERROR(expr->span, "Cannot create a reference to type 'void'");
    }

    return type->get_reference_to(expr->is_mutable);
}

quart::Type* Visitor::visit(ast::GenericTypeExpr* expr) {
    quart::TypeAlias* alias = resolve_type_alias(
        *this, expr->parent->name, std::move(expr->parent->parents)
    );

    if (!alias) {
        ERROR(expr->span, "Undefined type '{0}'", expr->parent->name);
    }

    if (!alias->is_generic()) {
        ERROR(expr->span, "Type '{0}' is not generic", expr->parent->name);
    }

    // TODO: Parse parents
    // TODO: Allow for struct types

    std::vector<quart::Type*> args;
    for (auto& arg : expr->args) {
        args.push_back(arg->accept(*this));
    }

    u32 i = 0;
    size_t size = args.size();

    for (auto& param : alias->parameters) {
        bool has_argument = i < size;

        if (!has_argument && !param.is_optional()) {
            ERROR(expr->span, "Expected a type argument for parameter '{0}'", param.name);
        } else if (!has_argument && param.is_optional()) {
            args.push_back(param.default_type);
        }

        i++;
    }

    if (args.size() != alias->parameters.size()) {
        ERROR(expr->span, "Expected {0} type parameters, got {1}", alias->parameters.size(), args.size());
    }


    return alias->instantiate(*this, args);
}

Value Visitor::visit(ast::TypeAliasExpr* expr) {
    if (!expr->is_generic_alias()) {
        quart::Type* type = expr->type->accept(*this);
        this->scope->type_aliases[expr->name] = quart::TypeAlias(expr->name, type, expr->span);

        return EMPTY_VALUE;
    }

    std::vector<GenericTypeParameter> parameters;
    for (auto& param : expr->parameters) {
        std::vector<quart::Type*> constraints;
        for (auto& constraint : param.constraints) {
            constraints.push_back(constraint->accept(*this));
        }

        quart::Type* default_type = nullptr;
        if (param.default_type) {
            default_type = param.default_type->accept(*this);
        }

        parameters.push_back({param.name, constraints, default_type, param.span});
    }


    this->scope->type_aliases[expr->name] = quart::TypeAlias(
        expr->name, parameters, std::move(expr->type), expr->span
    );

    return EMPTY_VALUE;
}