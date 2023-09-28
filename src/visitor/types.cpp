#include <quart/visitor.h>

#include <llvm/IR/Instructions.h>

#include <functional>

#define MATCH_TYPE(type, method, ...) case ast::BuiltinType::type: return this->registry->method(__VA_ARGS__)

using namespace quart;

static std::map<std::string, uint32_t> TYPE_SIZES = {
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

uint32_t Visitor::getsizeof(llvm::Value* value) {
    return this->getsizeof(value->getType());
}

uint32_t Visitor::getsizeof(quart::Type* type) {
    return this->getsizeof(type->to_llvm_type());
}

uint32_t Visitor::getallocsize(llvm::Type* type) {
    llvm::TypeSize tsize = this->module->getDataLayout().getTypeAllocSize(type);
    return tsize.getFixedSize();
}

uint32_t Visitor::getsizeof(llvm::Type* type) {
    if (type->isPointerTy() || type->isStructTy()) {
        return this->getallocsize(type);
    } else if (type->isArrayTy()) {
        return type->getArrayNumElements() * this->getsizeof(type->getArrayElementType());
    }

    return type->getPrimitiveSizeInBits() / 8;
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

    return Value(result, to);
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

            return {result, to};
        } else if (to->is_int()) {
            uint32_t bits = from->get_int_bit_width();
            if (bits < to->get_int_bit_width()) {
                return {this->builder->CreateZExt(value, type), to};
            } else if (bits > to->get_int_bit_width()) {
                return {this->builder->CreateTrunc(value, type), to};
            }

            return {value, to};
        } else if (to->is_pointer()) {
            return {this->builder->CreateIntToPtr(value, type), to};
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

            return {result, to};
        }
            
        ERROR(expr->span, err, from->get_as_string(), to->get_as_string());
    } else if (from->is_pointer()) {
        if (to->is_int()) {
            return {this->builder->CreatePtrToInt(value, type), to};
        } else if (to->is_pointer()) {
            return {this->builder->CreateBitCast(value, type), to};
        }
        
        ERROR(expr->span, err, from->get_as_string(), to->get_as_string());
    } else if (from->is_reference()) {
        if (!to->is_pointer()) {
            ERROR(expr->span, err, from->get_as_string(), to->get_as_string());
        }

        return {this->builder->CreateBitCast(value, type), to};
    }

    if (from->is_enum()) {
        quart::Type* inner = from->get_inner_enum_type();
        if (inner == to) {
            return {value, to};
        }

        return this->cast(value, inner);
    }

    ERROR(expr->span, err, from->get_as_string(), to->get_as_string());
}

Value Visitor::visit(ast::SizeofExpr* expr) {
    uint32_t size = 0;

    if (expr->value->kind() == ast::ExprKind::Variable) {
        ast::VariableExpr* id = expr->value->as<ast::VariableExpr>();
        if (TYPE_SIZES.find(id->name) != TYPE_SIZES.end()) {
            return Value(this->builder->getInt32(TYPE_SIZES[id->name]), true);
        }
    } 

    Value value = expr->value->accept(*this);
    if (value.flags & Value::Struct) {
        quart::Struct* structure = value.as<quart::Struct*>();
        size = this->getsizeof(structure->type);
    } else {
        if (value.is_empty_value()) {
            ERROR(expr->value->span, "Expected an expression");
        }

        size = this->getsizeof(value.inner);
    }

    return Value(
        this->builder->getInt32(size),
        this->registry->create_int_type(32, true),
        Value::Constant
    );
}

quart::Type* Visitor::visit(ast::BuiltinTypeExpr* expr) {
    return this->get_builtin_type(expr->value);
}

quart::Type* Visitor::visit(ast::IntegerTypeExpr* expr) {
    Value value = expr->size->accept(*this);
    if (value.is_empty_value()) {
        ERROR(expr->size->span, "Expected an expression");
    }

    llvm::ConstantInt* constant = llvm::dyn_cast<llvm::ConstantInt>(value.inner);
    if (!constant) {
        ERROR(expr->size->span, "Integer type size must be a constant");
    }

    int64_t size = constant->getSExtValue();
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
            ERROR(expr->span, "Unrecognised namespace '{0}'", name);
        }
    }

    if (scope->has_struct(expr->name)) {
        auto structure = scope->get_struct(expr->name);
        return structure->type;
    } else if (scope->has_type_alias(expr->name)) {
        return scope->get_type_alias(expr->name)->type;
    } else if (scope->has_enum(expr->name)) {
        return scope->get_enum(expr->name)->type;
    }

    ERROR(expr->span, "Unrecognised type '{0}'", expr->name);
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

    llvm::ConstantInt* constant = llvm::dyn_cast<llvm::ConstantInt>(value.inner);
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

Value Visitor::visit(ast::TypeAliasExpr* expr) {
    quart::Type* type = expr->type->accept(*this);
    this->scope->type_aliases[expr->name] = quart::TypeAlias(expr->name, type, expr->span);

    return EMPTY_VALUE;
}