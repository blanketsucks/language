#include "visitor.h"

Value Visitor::visit(ast::UnaryOpExpr* expr) {
    llvm::Value* value = expr->value->accept(*this).unwrap(expr->start);
    llvm::Type* type = value->getType();

    bool is_floating_point = type->isFloatingPointTy();
    bool is_numeric = type->isIntegerTy() || is_floating_point;

    switch (expr->op) {
        case TokenKind::Add:
            if (!is_numeric) {
                ERROR(expr->start, "Unsupported unary operator '+' for type '{0}'", this->get_type_name(type));
            }

            return value;
        case TokenKind::Minus:
            if (!is_numeric) {
                ERROR(expr->start, "Unsupported unary operator '-' for type '{0}'", this->get_type_name(type));
            }

            if (is_floating_point) {
                return this->builder->CreateFNeg(value);
            } else {
                return this->builder->CreateNeg(value);
            }
        case TokenKind::Not:
            return this->builder->CreateIsNull(value);
        case TokenKind::BinaryNot:
            return this->builder->CreateNot(value);
        case TokenKind::Mul: {
            if (!type->isPointerTy()) {
                ERROR(expr->start, "Unsupported unary operator '*' for type '{0}'", this->get_type_name(type));
            }

            llvm::Type* type = value->getType()->getNonOpaquePointerElementType();
            return this->builder->CreateLoad(type, value);
        }
        case TokenKind::BinaryAnd: {
            return this->get_pointer_from_expr(std::move(expr->value)).first;
        }
        case TokenKind::Inc: {
            if (!is_numeric) {
                ERROR(expr->start, "Unsupported unary operator '++' for type '{0}'", this->get_type_name(type));
            }

            llvm::Value* one = llvm::ConstantInt::get(value->getType(), llvm::APInt(value->getType()->getIntegerBitWidth(), 1, true));
            llvm::Value* result = this->builder->CreateAdd(value, one);

            this->builder->CreateStore(result, value);
            return result;
        }
        case TokenKind::Dec: {
            if (!is_numeric) {
                ERROR(expr->start, "Unsupported unary operator '--' for type '{0}'", this->get_type_name(type));
            }

            llvm::Value* one = llvm::ConstantInt::get(value->getType(), llvm::APInt(value->getType()->getIntegerBitWidth(), 1, true));
            return this->builder->CreateSub(value, one);
        }
        default:
            _UNREACHABLE
    }

    return nullptr;
}

Value Visitor::visit(ast::BinaryOpExpr* expr) {
    // Assingment is a special case.
    if (expr->op == TokenKind::Assign) {
        if (expr->left->kind() == ast::ExprKind::Attribute) {
            this->store_struct_field(expr->left->cast<ast::AttributeExpr>(), std::move(expr->right));
            return nullptr;
        } else if (expr->left->kind() == ast::ExprKind::Element) {
            this->store_array_element(expr->left->cast<ast::ElementExpr>(), std::move(expr->right));
            return nullptr;
        }

        auto local = this->get_pointer_from_expr(std::move(expr->left));
        if (local.second) {
            utils::error(expr->start, "Cannot assign to constant");
        }

        llvm::Value* inst = local.first;
        llvm::Value* value = expr->right->accept(*this).unwrap(expr->start);

        llvm::Type* type = inst->getType()->getNonOpaquePointerElementType();
        if (!this->is_compatible(value->getType(), type)) {
            ERROR(
                expr->start, 
                "Cannot assign variable of type '{0}' to value of type '{1}'", 
                this->get_type_name(type), this->get_type_name(value->getType())
            );
        }

        this->builder->CreateStore(value, inst);
        return value;
    }

    Value lhs = expr->left->accept(*this);
    Value rhs = expr->right->accept(*this);

    bool is_constant = lhs.is_constant && rhs.is_constant;

    llvm::Value* left = lhs.unwrap(expr->start);
    llvm::Value* right = rhs.unwrap(expr->start);

    llvm::Type* ltype = left->getType();
    llvm::Type* rtype = right->getType();

    if (!this->is_compatible(ltype, rtype)) {
        ERROR(
            expr->start, "Unsupported binary operator '{s}' between types '{s}' and '{s}'.", 
            Token::getTokenTypeValue(expr->op),
            this->get_type_name(ltype), this->get_type_name(rtype)
        );
    } else {
        right = this->cast(right, ltype);
    }

    bool is_floating_point = ltype->isFloatingPointTy();
    llvm::Value* result = nullptr;

    switch (expr->op) {
        case TokenKind::Add:
            if (is_floating_point) {
                result = this->builder->CreateFAdd(left, right); break;
            } else {
                result = this->builder->CreateAdd(left, right); break;
            }
        case TokenKind::Minus:
            if (is_floating_point) {
                result = this->builder->CreateFSub(left, right); break;
            } else {
                result = this->builder->CreateSub(left, right); break;
            }
        case TokenKind::Mul:
            if (is_floating_point) {
                result = this->builder->CreateFMul(left, right); break;
            } else {
                result = this->builder->CreateMul(left, right); break;
            }
        case TokenKind::Div:
            if (is_floating_point) {
                result = this->builder->CreateFDiv(left, right); break;
            } else {
                result = this->builder->CreateSDiv(left, right); break;
            }
        case TokenKind::Mod:
            if (is_floating_point) {
                result = this->builder->CreateFRem(left, right); break;
            } else {
                result = this->builder->CreateSRem(left, right); break;
            }
        case TokenKind::Eq:
            if (is_floating_point) {
                result = this->builder->CreateFCmpOEQ(left, right); break;
            } else {
                result = this->builder->CreateICmpEQ(left, right); break;
            }
        case TokenKind::Neq:
            if (is_floating_point) {
                result = this->builder->CreateFCmpONE(left, right); break;
            } else {
                result = this->builder->CreateICmpNE(left, right); break;
            }
        case TokenKind::Gt:
            if (is_floating_point) {
                result = this->builder->CreateFCmpOGT(left, right); break;
            } else {
                result = this->builder->CreateICmpSGT(left, right); break;
            }
        case TokenKind::Lt:
            if (is_floating_point) {
                result = this->builder->CreateFCmpOLT(left, right); break;
            } else {
                result = this->builder->CreateICmpSLT(left, right); break;
            }
        case TokenKind::Gte:
            if (is_floating_point) {
                result = this->builder->CreateFCmpOGE(left, right); break;
            } else {
                result = this->builder->CreateICmpSGE(left, right); break;
            }
        case TokenKind::Lte:
            if (is_floating_point) {
                result = this->builder->CreateFCmpOLE(left, right); break;
            } else {
                result = this->builder->CreateICmpSLE(left, right); break;
            }
        case TokenKind::And:
            result = this->builder->CreateAnd(this->cast(left, BooleanType), this->cast(right, BooleanType));
            break;
        case TokenKind::Or:
            result = this->builder->CreateOr(this->cast(left, BooleanType), this->cast(right, BooleanType));
            break;
        case TokenKind::BinaryAnd:
            result = this->builder->CreateAnd(left, right); break;
        case TokenKind::BinaryOr:
            result = this->builder->CreateOr(left, right); break;
        case TokenKind::Xor:
            result = this->builder->CreateXor(left, right); break;
        case TokenKind::Lsh:
            result = this->builder->CreateShl(left, right); break;
        case TokenKind::Rsh:
            result = this->builder->CreateLShr(left, right); break;
        default:
            _UNREACHABLE
    }

    return Value(result, is_constant);
}

Value Visitor::visit(ast::InplaceBinaryOpExpr* expr) {
    llvm::Value* rhs = expr->right->accept(*this).unwrap(expr->start);

    auto local = this->get_pointer_from_expr(std::move(expr->left));
    if (local.second) {
        utils::error(expr->start, "Cannot assign to constant");
    }

    llvm::Value* parent = local.first;
    llvm::Value* lhs = this->load(parent);

    llvm::Value* result = nullptr;
    switch (expr->op) {
        case TokenKind::Add:
            result = this->builder->CreateAdd(lhs, rhs); break;
        case TokenKind::Minus:
            result = this->builder->CreateSub(lhs, rhs); break;
        case TokenKind::Mul:
            result = this->builder->CreateMul(lhs, rhs); break;
        case TokenKind::Div:
            result = this->builder->CreateSDiv(lhs, rhs); break;
        default:
            _UNREACHABLE
    }

    this->builder->CreateStore(result, parent);
    return result;
}
