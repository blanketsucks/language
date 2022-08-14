#include "visitor.h"

Value Visitor::visit(ast::UnaryOpExpr* expr) {
    llvm::Value* value = expr->value->accept(*this).unwrap(this, expr->start);

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

            llvm::Type* type = value->getType()->getPointerElementType();
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
        if (expr->left->kind == ast::ExprKind::Attribute) {
            llvm::Value* pointer = this->get_struct_field((ast::AttributeExpr*)expr->left.get());
            llvm::Value* value = expr->right->accept(*this).unwrap(this, expr->start);

            this->builder->CreateStore(value, pointer);
            return value;
        } else if (expr->left->kind == ast::ExprKind::Element) {
            llvm::Value* pointer = this->get_array_element((ast::ElementExpr*)expr->left.get());
            llvm::Value* value = expr->right->accept(*this).unwrap(this, expr->start);

            this->builder->CreateStore(value, pointer);
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

        llvm::Value* value = expr->right->accept(*this).unwrap(this, expr->start);
        this->builder->CreateStore(value, pair.first);

        return value;
    }

    llvm::Value* left = expr->left->accept(*this).unwrap(this, expr->start);
    llvm::Value* right = expr->right->accept(*this).unwrap(this, expr->start);

    llvm::Type* ltype = left->getType();
    llvm::Type* rtype = right->getType();

    if (ltype != rtype) {
        Type* lhs = Type::from_llvm_type(ltype);
        Type* rhs = Type::from_llvm_type(rtype);

        if (!(lhs->isPointer() && rhs->isInteger())) {
            if (lhs->is_compatible(rtype)) {
                right = this->cast(right, ltype);
            } else {
                std::string lname = lhs->name();
                std::string rname = rhs->name();

                std::string operation = Token::getTokenTypeValue(expr->op);
                ERROR(expr->start, "Unsupported operation '{s}' for types '{s}' and '{s}'", operation, lname, rname);
            }
        } else {
            left = this->builder->CreatePtrToInt(left, rtype);
        }
    }

    bool is_floating_point = ltype->isFloatingPointTy();
    switch (expr->op) {
        case TokenKind::Add:
            if (is_floating_point) {
                return this->builder->CreateFAdd(left, right);
            } else {
                return this->builder->CreateAdd(left, right);
            }
        case TokenKind::Minus:
            if (is_floating_point) {
                return this->builder->CreateFSub(left, right);
            } else {
                return this->builder->CreateSub(left, right);
            }
        case TokenKind::Mul:
            if (is_floating_point) {
                return this->builder->CreateFMul(left, right);
            } else {
                return this->builder->CreateMul(left, right);
            }
        case TokenKind::Div:
            if (is_floating_point) {
                return this->builder->CreateFDiv(left, right);
            } else {
                return this->builder->CreateSDiv(left, right);
            }
        case TokenKind::Eq:
            if (is_floating_point) {
                return this->builder->CreateFCmpOEQ(left, right);
            } else {
                return this->builder->CreateICmpEQ(left, right);
            }
        case TokenKind::Neq:
            if (is_floating_point) {
                return this->builder->CreateFCmpONE(left, right);
            } else {
                return this->builder->CreateICmpNE(left, right);
            }
        case TokenKind::Gt:
            if (is_floating_point) {
                return this->builder->CreateFCmpOGT(left, right);
            } else {
                return this->builder->CreateICmpSGT(left, right);
            }
        case TokenKind::Lt:
            if (is_floating_point) {
                return this->builder->CreateFCmpOLT(left, right);
            } else {
                return this->builder->CreateICmpSLT(left, right);
            }
        case TokenKind::Gte:
            if (is_floating_point) {
                return this->builder->CreateFCmpOGE(left, right);
            } else {
                return this->builder->CreateICmpSGE(left, right);
            }
        case TokenKind::Lte:
            if (is_floating_point) {
                return this->builder->CreateFCmpOLE(left, right);
            } else {
                return this->builder->CreateICmpSLE(left, right);
            }
        case TokenKind::And:
            return this->builder->CreateAnd(this->cast(left, BooleanType), this->cast(right, BooleanType));
        case TokenKind::Or:
            return this->builder->CreateOr(this->cast(left, BooleanType), this->cast(right, BooleanType));
        case TokenKind::BinaryAnd:
            return this->builder->CreateAnd(left, right);
        case TokenKind::BinaryOr:
            return this->builder->CreateOr(left, right);
        case TokenKind::Xor:
            return this->builder->CreateXor(left, right);
        case TokenKind::Lsh:
            return this->builder->CreateShl(left, right);
        case TokenKind::Rsh:
            return this->builder->CreateLShr(left, right);
        default:
            _UNREACHABLE
    }

    return nullptr;
}

Value Visitor::visit(ast::InplaceBinaryOpExpr* expr) {
    llvm::Value* parent = nullptr;
    if (expr->left->kind == ast::ExprKind::Attribute) { 
        parent = this->get_struct_field((ast::AttributeExpr*)expr->left.get());
    } else if (expr->left->kind == ast::ExprKind::Element) {
        parent = this->get_array_element((ast::ElementExpr*)expr->left.get());
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

    llvm::Value* lhs = expr->left->accept(*this).unwrap(this, expr->start);
    llvm::Value* rhs = expr->right->accept(*this).unwrap(this, expr->start);

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

    this->builder->CreateStore(value, parent);
    return value;
}
