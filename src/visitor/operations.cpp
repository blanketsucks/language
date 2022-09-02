#include "visitor.h"

Value Visitor::visit(ast::UnaryOpExpr* expr) {
    llvm::Value* value = expr->value->accept(*this).unwrap(expr->start);

    bool is_floating_point = value->getType()->isFloatingPointTy();
    bool is_numeric = value->getType()->isIntegerTy() || is_floating_point;

    std::string name = Type::from_llvm_type(value->getType())->name();
    switch (expr->op) {
        case TokenKind::Add:
            if (!is_numeric) {
                ERROR(expr->start, "Unsupported unary operator '+' for type: '{s}'", name);
            }

            return value;
        case TokenKind::Minus:
            if (!is_numeric) {
                ERROR(expr->start, "Unsupported unary operator '-' for type: '{s}'", name);
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
            if (!value->getType()->isPointerTy()) {
                ERROR(expr->start, "Unsupported unary operator '*' for type: '{s}'", name);
            }

            llvm::Type* type = value->getType()->getNonOpaquePointerElementType();
            // if (type->isAggregateType()) {
            //     ERROR(expr->start, "Unsupported unary operator '*' for type: '{s}'", name);
            // }

            return this->builder->CreateLoad(type, value);
        }
        case TokenKind::BinaryAnd: {
            llvm::AllocaInst* alloca_inst = this->builder->CreateAlloca(value->getType());
            this->builder->CreateStore(value, alloca_inst);

            return alloca_inst;
        }
        case TokenKind::Inc: {
            if (!is_numeric) {
                ERROR(expr->start, "Unsupported unary operator '++' for type: '{s}'", name);
            }

            llvm::Value* one = llvm::ConstantInt::get(value->getType(), llvm::APInt(value->getType()->getIntegerBitWidth(), 1, true));
            llvm::Value* result = this->builder->CreateAdd(value, one);

            this->builder->CreateStore(result, value);
            return result;
        }
        case TokenKind::Dec: {
            if (!is_numeric) {
                ERROR(expr->start, "Unsupported unary operator '--' for type: '{s}'", name);
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
            auto pair = this->get_struct_field(expr->left->cast<ast::AttributeExpr>());

            llvm::Value* value = expr->right->accept(*this).unwrap(expr->start);
            if (pair.second < 0) {
                this->builder->CreateStore(value, pair.first);
            } else {
                this->builder->CreateInsertValue(pair.first, value, pair.second);
            }

            return value;
        } else if (expr->left->kind() == ast::ExprKind::Element) {
            auto pair = this->get_array_element(expr->left->cast<ast::ElementExpr>());

            llvm::Value* value = expr->right->accept(*this).unwrap(expr->start);
            if (pair.second < 0) {
                this->builder->CreateStore(value, pair.first);
            } else {
                this->builder->CreateInsertValue(pair.first, value, pair.second);
            }

            return value;
        }

        // We directly use `dynamic_cast` to avoid the assert in Expr::cast.
        ast::VariableExpr* variable = dynamic_cast<ast::VariableExpr*>(expr->left.get());
        if (!variable) {
            ERROR(expr->start, "Left side of assignment must be a variable");
        }

        auto pair = this->get_variable(variable->name);
        if (pair.second) {
            ERROR(expr->start, "Cannot assign to constant");
        }

        llvm::Value* value = expr->right->accept(*this).unwrap(expr->start);
        this->builder->CreateStore(value, pair.first);

        return value;
    }

    Value lhs = expr->left->accept(*this);
    Value rhs = expr->right->accept(*this);

    bool is_constant = lhs.is_constant && rhs.is_constant;

    llvm::Value* left = lhs.unwrap(expr->start);
    llvm::Value* right = rhs.unwrap(expr->start);

    llvm::Type* ltype = left->getType();
    llvm::Type* rtype = right->getType();

    if (ltype != rtype) {
        Type* lhst = Type::from_llvm_type(ltype);
        Type* rhst = Type::from_llvm_type(rtype);

        if (!(lhst->isPointer() && rhst->isInteger())) {
            if (lhst->is_compatible(rtype)) {
                right = this->cast(right, ltype);
            } else {
                std::string lname = lhst->name();
                std::string rname = rhst->name();

                std::string operation = Token::getTokenTypeValue(expr->op);
                ERROR(expr->start, "Unsupported binary operator '{s}' for types '{s}' and '{s}'", operation, lname, rname);
            }
        } else {
            left = this->builder->CreatePtrToInt(left, rtype);
        }
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
    llvm::Value* parent = nullptr;
    int index = -1;

    if (expr->left->kind() == ast::ExprKind::Attribute) { 
        auto pair = this->get_struct_field(expr->left->cast<ast::AttributeExpr>());
        parent = pair.first; index = pair.second;
    } else if (expr->left->kind() == ast::ExprKind::Element) {
        auto pair = this->get_array_element(expr->left->cast<ast::ElementExpr>());
        parent = pair.first; index = pair.second;
    } else {
        ast::VariableExpr* variable = dynamic_cast<ast::VariableExpr*>(expr->left.get());
        if (!variable) {
            ERROR(expr->start, "Left side of assignment must be a variable");
        }

        auto pair = this->get_variable(variable->name);
        if (pair.second) {
            ERROR(expr->start, "Cannot assign to constant");
        }

        parent = pair.first;
    }

    llvm::Value* lhs = expr->left->accept(*this).unwrap(expr->start);
    llvm::Value* rhs = expr->right->accept(*this).unwrap(expr->start);

    llvm::Value* value;
    switch (expr->op) {
        case TokenKind::Add:
            value = this->builder->CreateAdd(lhs, rhs); break;
        case TokenKind::Minus:
            value = this->builder->CreateSub(lhs, rhs); break;
        case TokenKind::Mul:
            value = this->builder->CreateMul(lhs, rhs); break;
        case TokenKind::Div:
            value = this->builder->CreateSDiv(lhs, rhs); break;
        default:
            _UNREACHABLE
    }


    if (index >= 0) {
        this->builder->CreateInsertValue(parent, value, index);
    } else {
        this->builder->CreateStore(value, parent);
    }

    return value;
}
