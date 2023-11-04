#include <quart/visitor.h>

using namespace quart;

static std::map<TokenKind, std::string> STRUCT_OP_MAPPING = {
    {TokenKind::Add, "add"},
    {TokenKind::Minus, "sub"},
    {TokenKind::Mul, "mul"},
    {TokenKind::Div, "div"},
    {TokenKind::Mod, "mod"},
    {TokenKind::Not, "bool"},
    // TODO: add more
};

Value Visitor::evaluate_tuple_assignment(ast::BinaryOpExpr* expr) {
    // Parse and evaluate `(a, b, c) = (1, 2, 3)` where a, b and c are mutable variables
    auto tuple = expr->left->as<ast::TupleExpr>();
    bool check = std::all_of(
        tuple->elements.begin(),
        tuple->elements.end(),
        [](auto& e) { return e->kind() == ast::ExprKind::Variable; }
    );

    if (!check) {
        ERROR(expr->left->span, "Expected a tuple of identifiers");
    }

    Value rhs = expr->right->accept(*this);
    if (rhs.is_empty_value()) ERROR(expr->right->span, "Expected a value");

    if (!rhs.type->is_tuple()) {
        ERROR(expr->right->span, "Expected a tuple but got '{0}'", rhs.type->get_as_string());
    }

    std::vector<Value> values = this->unpack(rhs, tuple->elements.size(), expr->right->span);
    size_t i = 0;

    for (auto& element : tuple->elements) {
        ast::VariableExpr* variable = element->as<ast::VariableExpr>();
        auto ref = this->scope->get_local(variable->name, true);

        if (ref.is_null()) {
            ERROR(variable->span, "Variable '{0}' is not defined", variable->name);
        } 
        
        if (ref.flags & ScopeLocal::Constant) {
            ERROR(variable->span, "Cannot assign to constant '{0}'", ref.name);
        } else if (!(ref.flags & ScopeLocal::Mutable)) {
            ERROR(variable->span, "Cannot assign to immutable variable '{0}'", ref.name);
        }

        Value& value = values[i];
        if (!Type::can_safely_cast_to(value.type, ref.type)) {
            ERROR(
                variable->span, 
                "Cannot assign variable of type '{0}' to value of type '{1}'", 
                value.type->get_as_string(), ref.type->get_as_string()
            );
        }

        value = this->cast(value, ref.type);
        this->builder->CreateStore(value, ref.value);

        this->mark_as_mutated(ref);
    }

    return EMPTY_VALUE;
} 

Value Visitor::evaluate_assignment(ast::BinaryOpExpr* expr) {
    switch (expr->left->kind()) {
        case ast::ExprKind::Attribute:
            return this->evaluate_attribute_assignment(
                expr->left->as<ast::AttributeExpr>(), *expr->right
            );
        case ast::ExprKind::Index:
            return this->evaluate_subscript_assignment(
                expr->left->as<ast::IndexExpr>(), *expr->left
            );
        case ast::ExprKind::Tuple:
            return this->evaluate_tuple_assignment(expr);
        case ast::ExprKind::UnaryOp: {
            auto* unary = expr->left->as<ast::UnaryOpExpr>();
            if (unary->op != UnaryOp::Mul) {
                ERROR(unary->value->span, "Expected a variable, struct field or array element");
            }

            Value value = unary->value->accept(*this);
            if (value.is_empty_value()) {
                ERROR(unary->value->span, "Expected a value");
            } else if (!value.is_mutable()) {
                ERROR(unary->value->span, "Cannot assign to immutable value");
            }

            if (!value.type->is_pointer()) {
                ERROR(expr->left->span, "Unsupported unary operator '*' for type '{0}'", value.type->get_as_string());
            }

            Value rhs = expr->right->accept(*this);
            if (rhs.is_empty_value()) {
                ERROR(expr->right->span, "Expected a value");
            }

            quart::Type* type = value.type->get_pointee_type();
            if (!Type::can_safely_cast_to(rhs.type, type)) {
                ERROR(
                    expr->span, 
                    "Cannot assign pointer of type '{0}' to value of type '{1}'", 
                    value.type->get_as_string(), rhs.type->get_as_string()
                );
            }

            this->builder->CreateStore(this->cast(rhs, type), value);
            return EMPTY_VALUE;
        }
        default: break;
    }

    auto ref = this->as_reference(*expr->left);
    if (ref.flags & ScopeLocal::Constant) {
        ERROR(expr->span, "Cannot assign to constant");
    } else if (!(ref.flags & ScopeLocal::Mutable)) {
        ERROR(expr->span, "Cannot assign to immutable variable '{0}'", ref.name);
    }

    if (!ref.value) {
        ERROR(expr->left->span, "Left hand side of assignment must be a variable, struct field or array element");
    }

    Value rhs = expr->right->accept(*this);
    if (rhs.is_empty_value()) ERROR(expr->right->span, "Expected a value");

    if (!Type::can_safely_cast_to(rhs.type, ref.type)) {
        ERROR(
            expr->span, 
            "Cannot assign variable of type '{0}' to value of type '{1}'", 
            ref.type->get_as_string(), rhs.type->get_as_string()
        );
    }

    rhs = this->cast(rhs, ref.type);
    this->mark_as_mutated(ref);

    this->builder->CreateStore(rhs, ref.value);
    return rhs;
}

Value Visitor::evaluate_float_operation(const Value& lhs, BinaryOp op, const Value& rhs) {
    switch (op) {
        case BinaryOp::Add:
            return {this->builder->CreateFAdd(lhs, rhs), lhs.type};
        case BinaryOp::Sub:
            return {this->builder->CreateFSub(lhs, rhs), lhs.type};
        case BinaryOp::Mul:
            return {this->builder->CreateFMul(lhs, rhs), lhs.type};
        case BinaryOp::Div:
            return {this->builder->CreateFDiv(lhs, rhs), lhs.type};
        case BinaryOp::Mod:
            return {this->builder->CreateFRem(lhs, rhs), lhs.type};
        case BinaryOp::Eq:
            return {this->builder->CreateFCmpOEQ(lhs, rhs), lhs.type};
        case BinaryOp::Neq:
            return {this->builder->CreateFCmpONE(lhs, rhs), lhs.type};
        case BinaryOp::Gt:
            return {this->builder->CreateFCmpOGT(lhs, rhs), lhs.type};
        case BinaryOp::Lt:
            return {this->builder->CreateFCmpOLT(lhs, rhs), lhs.type};
        case BinaryOp::Gte:
            return {this->builder->CreateFCmpOGE(lhs, rhs), lhs.type};
        case BinaryOp::Lte:
            return {this->builder->CreateFCmpOLE(lhs, rhs), lhs.type};
        // Handled in `evaluate_binary_operation`
        case BinaryOp::Or:
        case BinaryOp::And:
        case BinaryOp::BinaryOr:
        case BinaryOp::BinaryAnd:
        case BinaryOp::Xor:
        case BinaryOp::Rsh:
        case BinaryOp::Lsh:
        case BinaryOp::Assign:
            break;
        }

    UNREACHABLE();
    return nullptr;
}

Value Visitor::visit(ast::UnaryOpExpr* expr) {
    Value value = expr->value->accept(*this);
    if (value.is_empty_value()) ERROR(expr->span, "Expected a value");

    quart::Type* type = value.type;

    bool is_floating_point = type->is_floating_point();
    bool is_numeric = type->is_int() || is_floating_point;

    switch (expr->op) {
        case UnaryOp::Add:
            if (!is_numeric) {
                ERROR(expr->value->span, "Unsupported unary operator '+' for type '{0}'", type->get_as_string());
            }

            return value;
        case UnaryOp::Sub:
            if (!is_numeric) {
                ERROR(expr->value->span, "Unsupported unary operator '-' for type '{0}'", type->get_as_string());
            }

            if (type->is_int()) {
                if (type->is_int_unsigned()) {
                    ERROR(expr->value->span, "Unsupported unary operator '-' for type '{0}'", type->get_as_string());
                }
            }

            if (is_floating_point) {
                return Value(this->builder->CreateFNeg(value), type);
            } else {
                return Value(this->builder->CreateNeg(value), type);
            }
        case UnaryOp::Not:
            return {this->builder->CreateIsNull(value), type};
        case UnaryOp::BinaryNot:
            return {this->builder->CreateNot(value), type};
        case UnaryOp::Mul: {
            if (!type->is_pointer()) {
                ERROR(expr->span, "Unsupported unary operator '*' for type '{0}'", type->get_as_string());
            }

            type = type->get_pointee_type();
            if (type->is_void() || type->is_function()) {
                ERROR(expr->span, "Cannot dereference a value of type '{0}'", type->get_as_string());
            }

            return Value(this->load(value), type);
        }
        case UnaryOp::BinaryAnd: {
            auto ref = this->as_reference(*expr->value);
            if (!ref.value) {
                llvm::AllocaInst* alloca = this->alloca(value->getType());
                return Value(alloca, type->get_reference_to(true));
            }

            u16 flags = ref.flags & ScopeLocal::StackAllocated ? Value::StackAllocated : Value::None;
            return Value(ref.value, type->get_reference_to(ref.is_mutable()), flags);
        }
        case UnaryOp::Inc: {
            if (!is_numeric) {
                ERROR(expr->span, "Unsupported unary operator '++' for type '{0}'", type->get_as_string());
            }

            auto ref = this->as_reference(*expr->value);
            if (ref.is_null()) {
                ERROR(expr->span, "Expected a variable, struct member or array element");
            }

            if (!ref.is_mutable()) {
                ERROR(expr->span, "Cannot increment immutable variable");
            }

            llvm::Value* one = this->builder->getIntN(ref.type->get_int_bit_width(), 1);
            llvm::Value* result = this->builder->CreateAdd(value, one);

            this->builder->CreateStore(result, ref.value);
            return {result, type};
        }
        case UnaryOp::Dec: {
            if (!is_numeric) {
                ERROR(expr->span, "Unsupported unary operator '--' for type '{0}'", type->get_as_string());
            }

            auto ref = this->as_reference(*expr->value);
            if (ref.is_null()) {
                ERROR(expr->span, "Expected a variable, struct member or array element");
            }

            if (!ref.is_mutable()) {
                ERROR(expr->span, "Cannot decrement immutable variable");
            }

            llvm::Value* one = this->builder->getIntN(ref.type->get_int_bit_width(), 1);
            llvm::Value* result = this->builder->CreateSub(value, one);

            this->builder->CreateStore(result, ref.value);
            return {result, type};
        }
    }

    UNREACHABLE();
    return nullptr;
}

Value Visitor::evaluate_binary_operation(const Value& lhs, BinaryOp op, const Value& rhs) {
    quart::Type* boolean = this->registry->create_int_type(1, true);
    switch (op) {
        case BinaryOp::And:
            return {
                this->builder->CreateAnd(
                    this->cast(lhs, boolean), 
                    this->cast(rhs, boolean)
                ), 
                boolean
            };
        case BinaryOp::Or: 
            return {
                this->builder->CreateOr(
                    this->cast(lhs, boolean), 
                    this->cast(rhs, boolean)
                ), 
                boolean
            };
        case BinaryOp::BinaryAnd:
            return {this->builder->CreateAnd(lhs, rhs), lhs.type};
        case BinaryOp::BinaryOr:
            return {this->builder->CreateOr(lhs, rhs), lhs.type};
        case BinaryOp::Xor:
            return {this->builder->CreateXor(lhs, rhs), lhs.type};
        case BinaryOp::Lsh:
            return {this->builder->CreateShl(lhs, rhs), lhs.type};
        case BinaryOp::Rsh:
            return {this->builder->CreateLShr(lhs, rhs), lhs.type};
        default: break;
    }

    if (lhs.type->is_floating_point()) {
        return this->evaluate_float_operation(lhs, op, rhs);
    }

    bool is_unsigned = lhs.type->is_int_unsigned();
    switch (op) {
        case BinaryOp::Add:
            return {this->builder->CreateAdd(lhs, rhs), lhs.type};
        case BinaryOp::Sub:
            return {this->builder->CreateSub(lhs, rhs), lhs.type};
        case BinaryOp::Mul:
            return {this->builder->CreateMul(lhs, rhs), lhs.type};
        case BinaryOp::Div:
            if (is_unsigned) {
                return {this->builder->CreateUDiv(lhs, rhs), lhs.type};
            }
            
            return {this->builder->CreateSDiv(lhs, rhs), lhs.type};
        case BinaryOp::Mod:
            if (is_unsigned) {
                return {this->builder->CreateURem(lhs, rhs), lhs.type};
            }

            return {this->builder->CreateSRem(lhs, rhs), lhs.type};
        case BinaryOp::Eq:
            return {this->builder->CreateICmpEQ(lhs, rhs), boolean};
        case BinaryOp::Neq:
            return {this->builder->CreateICmpNE(lhs, rhs), boolean};
        case BinaryOp::Gt:
            if (is_unsigned) {
                return {this->builder->CreateICmpUGT(lhs, rhs), boolean};
            }

            return {this->builder->CreateICmpSGT(lhs, rhs), boolean};
        case BinaryOp::Lt:
            if (is_unsigned) {
                return {this->builder->CreateICmpULT(lhs, rhs), boolean};
            }

            return {this->builder->CreateICmpSLT(lhs, rhs), boolean};
        case BinaryOp::Gte:
            if (is_unsigned) {
                return {this->builder->CreateICmpUGE(lhs, rhs), boolean};
            }

            return {this->builder->CreateICmpSGE(lhs, rhs), boolean};
        case BinaryOp::Lte:
            if (is_unsigned) {
                return {this->builder->CreateICmpULE(lhs, rhs), boolean};
            }

            return {this->builder->CreateICmpSLE(lhs, rhs), boolean};
        // These are handled above and are just here just to make the compiler shut up
        case BinaryOp::Or:
        case BinaryOp::And:
        case BinaryOp::BinaryOr:
        case BinaryOp::BinaryAnd:
        case BinaryOp::Xor:
        case BinaryOp::Rsh:
        case BinaryOp::Lsh:
        case BinaryOp::Assign:
            break;
        }

    UNREACHABLE();
    return nullptr;
}

Value Visitor::visit(ast::BinaryOpExpr* expr) {
    if (expr->op == BinaryOp::Assign) return this->evaluate_assignment(expr);

    Value lhs = expr->left->accept(*this);
    if (lhs.is_empty_value()) ERROR(expr->left->span, "Expected a value");

    this->inferred = lhs.type;

    Value rhs = expr->right->accept(*this);
    if (rhs.is_empty_value()) ERROR(expr->right->span, "Expected a value");

    this->inferred = nullptr;
    if (!Type::can_safely_cast_to(rhs.type, lhs.type)) {
        ERROR(
            expr->span, 
            "Unsupported binary operation '{0}' between types '{1}' and '{2}'", 
            quart::get_binary_op_value(expr->op), lhs.type->get_as_string(), rhs.type->get_as_string()
        );
    } else {
        rhs = this->cast(rhs, lhs.type);
    }

    return this->evaluate_binary_operation(lhs, expr->op, rhs);
}

Value Visitor::visit(ast::InplaceBinaryOpExpr* expr) {
    // foo = foo <op> bar (foo <op>= bar)
    Value lhs = expr->left->accept(*this);
    if (lhs.is_empty_value()) ERROR(expr->left->span, "Expected a value");

    this->inferred = lhs.type;

    Value rhs = expr->right->accept(*this);
    if (rhs.is_empty_value()) ERROR(expr->right->span, "Expected a value");

    this->inferred = nullptr;
    if (!Type::can_safely_cast_to(rhs.type, lhs.type)) {
        ERROR(
            expr->span, 
            "Unsupported binary operation '{0}' between types '{1}' and '{2}'", 
            quart::get_binary_op_value(expr->op), lhs.type->get_as_string(), rhs.type->get_as_string()
        );
    } else {
        rhs = this->cast(rhs, lhs.type);
    }

    auto ref = this->as_reference(*expr->left);
    if (ref.is_constant()) {
        ERROR(expr->span, "Cannot assign to constant '{0}'", ref.name);
    } else if (!ref.is_mutable()) {
        ERROR(expr->span, "Cannot assign to immutable variable '{0}'", ref.name);
    }

    Value result = this->evaluate_binary_operation(lhs, expr->op, rhs);
    this->builder->CreateStore(result, ref.value);
    
    this->mark_as_mutated(ref);
    return result;
}
