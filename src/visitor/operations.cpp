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

            type = type->getNonOpaquePointerElementType();
            if (type->isVoidTy()) {
                ERROR(expr->start, "Cannot dereference a void pointer");
            }

            return this->load(value, type);
        }
        case TokenKind::BinaryAnd: {
            return Value::as_reference(this->as_reference(expr->value.get()).value);
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
        } else if (expr->left->kind() == ast::ExprKind::UnaryOp) {
            auto* unary = expr->left->cast<ast::UnaryOpExpr>();
            if (unary->op == TokenKind::Mul) { // *a = b
                llvm::Value* value = expr->right->accept(*this).unwrap(expr->start);
                llvm::Value* parent = unary->value->accept(*this).unwrap(expr->start);

                llvm::Type* type = parent->getType();
                if (!type->isPointerTy()) {
                    ERROR(unary->value->start, "Unsupported unary operator '*' for type '{0}'", this->get_type_name(type));
                }

                type = type->getNonOpaquePointerElementType();
                if (!this->is_compatible(type, value->getType())) {
                    ERROR(expr->right->start, "Cannot assign value of type '{0}' to variable of type '{1}'", this->get_type_name(value->getType()), this->get_type_name(type));
                }

                value = this->cast(value, type);
                this->builder->CreateStore(value, parent);

                return nullptr;
            }
        }

        auto local = this->as_reference(expr->left.get());
        if (local.is_constant) {
            ERROR(expr->start, "Cannot assign to constant");
        } 

        if (!local.value) {
            ERROR(expr->left->start, "Left hand side of assignment must be a variable, struct field or array element");
        }

        llvm::Value* inst = local.value;
        llvm::Value* value = expr->right->accept(*this).unwrap(expr->start);

        llvm::Type* type = inst->getType()->getNonOpaquePointerElementType();
        if (!this->is_compatible(type, value->getType())) {
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

    if (this->is_struct(ltype)) {
        auto structure = this->get_struct(ltype);
        if (STRUCT_OP_MAPPING.find(expr->op) == STRUCT_OP_MAPPING.end()) {
            TODO("Not implemented");
        }

        auto method = structure->scope->functions[STRUCT_OP_MAPPING[expr->op]];
        if (!method) {
            ERROR(
                expr->start, "Unsupported binary operation '{0}' between types '{1}' and '{2}'.", 
                Token::getTokenTypeValue(expr->op),
                this->get_type_name(ltype), this->get_type_name(rtype)
            );
        }

        llvm::Value* self = left;
        if (!ltype->isPointerTy()) {
            self = this->as_reference(expr->left.get()).value;
        }

        return this->call(method->value, { right }, self);
    }

    if (!this->is_compatible(ltype, rtype)) {
        ERROR(
            expr->start, "Unsupported binary operation '{0}' between types '{1}' and '{2}'.", 
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
            result = this->builder->CreateAnd(
                this->cast(left, this->builder->getInt1Ty()), 
                this->cast(right, this->builder->getInt1Ty())
            );
            break;
        case TokenKind::Or:
            result = this->builder->CreateOr(
                this->cast(left, this->builder->getInt1Ty()), 
                this->cast(right, this->builder->getInt1Ty())
            );
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
    auto local = this->as_reference(expr->left.get());
    if (local.is_constant) {
        ERROR(expr->start, "Cannot assign to constant");
    }

    llvm::Value* parent = local.value;
    llvm::Value* lhs = this->load(parent, local.type);

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
