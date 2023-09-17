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
                expr->left->as<ast::AttributeExpr>(), std::move(expr->right)
            );
        case ast::ExprKind::Index:
            return this->evaluate_subscript_assignment(
                expr->left->as<ast::IndexExpr>(), std::move(expr->right)
            );
        case ast::ExprKind::Tuple:
            return this->evaluate_tuple_assignment(expr);
        case ast::ExprKind::UnaryOp: {
            auto* unary = expr->left->as<ast::UnaryOpExpr>();
            if (unary->op != TokenKind::Mul) {
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

    auto ref = this->as_reference(expr->left);
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

Value Visitor::evaluate_float_operation(const Value& lhs, TokenKind op, const Value& rhs) {
    switch (op) {
        case TokenKind::Add:
            return {this->builder->CreateFAdd(lhs, rhs), lhs.type};
        case TokenKind::Minus:
            return {this->builder->CreateFSub(lhs, rhs), lhs.type};
        case TokenKind::Mul:
            return {this->builder->CreateFMul(lhs, rhs), lhs.type};
        case TokenKind::Div:
            return {this->builder->CreateFDiv(lhs, rhs), lhs.type};
        case TokenKind::Mod:
            return {this->builder->CreateFRem(lhs, rhs), lhs.type};
        case TokenKind::Eq:
            return {this->builder->CreateFCmpOEQ(lhs, rhs), lhs.type};
        case TokenKind::Neq:
            return {this->builder->CreateFCmpONE(lhs, rhs), lhs.type};
        case TokenKind::Gt:
            return {this->builder->CreateFCmpOGT(lhs, rhs), lhs.type};
        case TokenKind::Lt:
            return {this->builder->CreateFCmpOLT(lhs, rhs), lhs.type};
        case TokenKind::Gte:
            return {this->builder->CreateFCmpOGE(lhs, rhs), lhs.type};
        case TokenKind::Lte:
            return {this->builder->CreateFCmpOLE(lhs, rhs), lhs.type};
        default: __builtin_unreachable();
    }
}

Value Visitor::visit(ast::UnaryOpExpr* expr) {
    Value value = expr->value->accept(*this);
    if (value.is_empty_value()) ERROR(expr->span, "Expected a value");

    quart::Type* type = value.type;

    bool is_floating_point = type->is_floating_point();
    bool is_numeric = type->is_int() || is_floating_point;

    switch (expr->op) {
        case TokenKind::Add:
            if (!is_numeric) {
                ERROR(expr->value->span, "Unsupported unary operator '+' for type '{0}'", type->get_as_string());
            }

            return value;
        case TokenKind::Minus:
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
        case TokenKind::Not:
            return {this->builder->CreateIsNull(value), type};
        case TokenKind::BinaryNot:
            return {this->builder->CreateNot(value), type};
        case TokenKind::Mul: {
            if (!type->is_pointer()) {
                ERROR(expr->span, "Unsupported unary operator '*' for type '{0}'", type->get_as_string());
            }

            type = type->get_pointee_type();
            if (type->is_void() || type->is_function()) {
                ERROR(expr->span, "Cannot dereference a value of type '{0}'", type->get_as_string());
            }

            return Value(this->load(value), type);
        }
        case TokenKind::BinaryAnd: {
            auto ref = this->as_reference(expr->value);
            if (!ref.value) {
                llvm::AllocaInst* alloca = this->alloca(value->getType());
                return Value(alloca, type->get_reference_to(true));
            }

            uint16_t flags = ref.flags & ScopeLocal::StackAllocated ? Value::StackAllocated : Value::None;
            return Value(ref.value, type->get_reference_to(ref.is_mutable()), flags);
        }
        case TokenKind::Inc: {
            if (!is_numeric) {
                ERROR(expr->span, "Unsupported unary operator '++' for type '{0}'", type->get_as_string());
            }

            auto ref = this->as_reference(expr->value);
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
        case TokenKind::Dec: {
            if (!is_numeric) {
                ERROR(expr->span, "Unsupported unary operator '--' for type '{0}'", type->get_as_string());
            }

            auto ref = this->as_reference(expr->value);
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
        default: __builtin_unreachable();
    }

    return nullptr;
}

Value Visitor::evaluate_binary_operation(const Value& lhs, TokenKind op, const Value& rhs) {
    quart::Type* boolean = this->registry->create_int_type(1, true);
    switch (op) {
        case TokenKind::And:
            return {
                this->builder->CreateAnd(
                    this->cast(lhs, boolean), 
                    this->cast(rhs, boolean)
                ), 
                boolean
            };
        case TokenKind::Or: 
            return {
                this->builder->CreateOr(
                    this->cast(lhs, boolean), 
                    this->cast(rhs, boolean)
                ), 
                boolean
            };
        case TokenKind::BinaryAnd:
            return {this->builder->CreateAnd(lhs, rhs), lhs.type};
        case TokenKind::BinaryOr:
            return {this->builder->CreateOr(lhs, rhs), lhs.type};
        case TokenKind::Xor:
            return {this->builder->CreateXor(lhs, rhs), lhs.type};
        case TokenKind::Lsh:
            return {this->builder->CreateShl(lhs, rhs), lhs.type};
        case TokenKind::Rsh:
            return {this->builder->CreateLShr(lhs, rhs), lhs.type};
        default: break;
    }

    if (lhs.type->is_floating_point()) {
        return this->evaluate_float_operation(lhs, op, rhs);
    }

    bool is_unsigned = lhs.type->is_int_unsigned();
    switch (op) {
        case TokenKind::Add:
            return {this->builder->CreateAdd(lhs, rhs), lhs.type};
        case TokenKind::Minus:
            return {this->builder->CreateSub(lhs, rhs), lhs.type};
        case TokenKind::Mul:
            return {this->builder->CreateMul(lhs, rhs), lhs.type};
        case TokenKind::Div:
            if (is_unsigned) {
                return {this->builder->CreateUDiv(lhs, rhs), lhs.type};
            }
            
            return {this->builder->CreateSDiv(lhs, rhs), lhs.type};
        case TokenKind::Mod:
            if (is_unsigned) {
                return {this->builder->CreateURem(lhs, rhs), lhs.type};
            }

            return {this->builder->CreateSRem(lhs, rhs), lhs.type};
        case TokenKind::Eq:
            return {this->builder->CreateICmpEQ(lhs, rhs), boolean};
        case TokenKind::Neq:
            return {this->builder->CreateICmpNE(lhs, rhs), boolean};
        case TokenKind::Gt:
            if (is_unsigned) {
                return {this->builder->CreateICmpUGT(lhs, rhs), boolean};
            }

            return {this->builder->CreateICmpSGT(lhs, rhs), boolean};
        case TokenKind::Lt:
            if (is_unsigned) {
                return {this->builder->CreateICmpULT(lhs, rhs), boolean};
            }

            return {this->builder->CreateICmpSLT(lhs, rhs), boolean};
        case TokenKind::Gte:
            if (is_unsigned) {
                return {this->builder->CreateICmpUGE(lhs, rhs), boolean};
            }

            return {this->builder->CreateICmpSGE(lhs, rhs), boolean};
        case TokenKind::Lte:
            if (is_unsigned) {
                return {this->builder->CreateICmpULE(lhs, rhs), boolean};
            }

            return {this->builder->CreateICmpSLE(lhs, rhs), boolean};
        default: __builtin_unreachable();
    }
}

Value Visitor::visit(ast::BinaryOpExpr* expr) {
    if (expr->op == TokenKind::Assign) return this->evaluate_assignment(expr);

    Value lhs = expr->left->accept(*this);
    if (lhs.is_empty_value()) ERROR(expr->left->span, "Expected a value");

    Value rhs = expr->right->accept(*this);
    if (rhs.is_empty_value()) ERROR(expr->right->span, "Expected a value");

    if (!Type::can_safely_cast_to(rhs.type, lhs.type)) {
        ERROR(
            expr->span, 
            "Unsupported binary operation '{0}' between types '{1}' and '{2}'", 
            Token::get_type_value(expr->op), rhs.type->get_as_string(), lhs.type->get_as_string()
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

    Value rhs = expr->right->accept(*this);
    if (rhs.is_empty_value()) ERROR(expr->right->span, "Expected a value");

    if (!Type::can_safely_cast_to(rhs.type, lhs.type)) {
        ERROR(
            expr->span, 
            "Unsupported binary operation '{0}' between types '{1}' and '{2}'", 
            Token::get_type_value(expr->op), rhs.type->get_as_string(), lhs.type->get_as_string()
        );
    } else {
        rhs = this->cast(rhs, lhs.type);
    }

    auto ref = this->as_reference(expr->left);
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
