#include <quart/visitor.h>

static std::map<TokenKind, std::string> STRUCT_OP_MAPPING = {
    {TokenKind::Add, "add"},
    {TokenKind::Minus, "sub"},
    {TokenKind::Mul, "mul"},
    {TokenKind::Div, "div"},
    {TokenKind::Mod, "mod"},
    {TokenKind::Not, "bool"},
    // TODO: add more
};

Value Visitor::visit(ast::UnaryOpExpr* expr) {
    llvm::Value* value = expr->value->accept(*this).unwrap(expr->span);
    llvm::Type* type = value->getType();

    bool is_floating_point = type->isFloatingPointTy();
    bool is_numeric = type->isIntegerTy() || is_floating_point;

    switch (expr->op) {
        case TokenKind::Add:
            if (!is_numeric) {
                ERROR(expr->span, "Unsupported unary operator '+' for type '{0}'", this->get_type_name(type));
            }

            return value;
        case TokenKind::Minus:
            if (!is_numeric) {
                ERROR(expr->span, "Unsupported unary operator '-' for type '{0}'", this->get_type_name(type));
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
                ERROR(expr->span, "Unsupported unary operator '*' for type '{0}'", this->get_type_name(type));
            }

            type = type->getPointerElementType();
            if (type->isVoidTy() || type->isFunctionTy()) {
                ERROR(expr->span, "Cannot dereference a value of type '{0}'", this->get_type_name(type));
            }

            return this->load(value, type);
        }
        case TokenKind::BinaryAnd: {
            auto ref = this->as_reference(expr->value);
            if (!ref.value) {
                llvm::Value* value = expr->value->accept(*this).unwrap(expr->span);
                return Value::as_reference(
                    this->alloca(value->getType()), false, true
                );
            }

            return Value::as_reference(ref.value, ref.is_immutable, ref.is_stack_allocated);
        }
        case TokenKind::Inc: {
            if (!is_numeric) {
                ERROR(expr->span, "Unsupported unary operator '++' for type '{0}'", this->get_type_name(type));
            }

            auto ref = this->as_reference(expr->value);
            if (ref.is_null()) {
                ERROR(expr->span, "Expected a variable, struct member or array element");
            }

            if (ref.is_immutable) {
                ERROR(expr->span, "Cannot increment immutable variable");
            }

            llvm::Value* one = this->builder->getIntN(ref.type->getIntegerBitWidth(), 1);
            llvm::Value* result = this->builder->CreateAdd(value, one);

            this->builder->CreateStore(result, ref.value);
            return result;
        }
        case TokenKind::Dec: {
            if (!is_numeric) {
                ERROR(expr->span, "Unsupported unary operator '--' for type '{0}'", this->get_type_name(type));
            }

            auto ref = this->as_reference(expr->value);
            if (ref.is_null()) {
                ERROR(expr->span, "Expected a variable, struct member or array element");
            }

            if (ref.is_immutable) {
                ERROR(expr->span, "Cannot decrement immutable variable");
            }

            llvm::Value* one = this->builder->getIntN(ref.type->getIntegerBitWidth(), 1);
            llvm::Value* result = this->builder->CreateSub(value, one);

            this->builder->CreateStore(result, ref.value);
            return result;
        }
        default: __UNREACHABLE
    }

    return nullptr;
}

Value Visitor::visit(ast::BinaryOpExpr* expr) {
    // Assingment is a special case.
    if (expr->op == TokenKind::Assign) {
        if (expr->left->kind() == ast::ExprKind::Attribute) {
            this->store_struct_field(expr->left->as<ast::AttributeExpr>(), std::move(expr->right));
            return nullptr;
        } else if (expr->left->kind() == ast::ExprKind::Element) {
            this->store_array_element(expr->left->as<ast::ElementExpr>(), std::move(expr->right));
            return nullptr;
        } else if (expr->left->kind() == ast::ExprKind::UnaryOp) {
            auto* unary = expr->left->as<ast::UnaryOpExpr>();
            if (unary->op == TokenKind::Mul) { // *a = b
                llvm::Value* value = expr->right->accept(*this).unwrap(expr->span);

                Value val = unary->value->accept(*this);
                llvm::Value* parent = val.unwrap(expr->span);

                llvm::Type* type = parent->getType();
                if (!type->isPointerTy()) {
                    ERROR(unary->value->span, "Unsupported unary operator '*' for type '{0}'", this->get_type_name(type));
                }

                type = type->getPointerElementType();
                if (!this->is_compatible(type, value->getType())) {
                    ERROR(expr->right->span, "Cannot assign value of type '{0}' to variable of type '{1}'", this->get_type_name(value->getType()), this->get_type_name(type));
                }

                if (val.is_immutable) {
                    ERROR(expr->span, "Cannot assign to immutable value");
                }

                value = this->cast(value, type);
                this->builder->CreateStore(value, parent);

                return nullptr;
            }
        } else if (expr->left->kind() == ast::ExprKind::Tuple) {
            auto* tuple = expr->left->as<ast::TupleExpr>();
            bool check = std::all_of(
                tuple->elements.begin(),
                tuple->elements.end(),
                [](auto& e) { return e->kind() == ast::ExprKind::Variable; }
            );

            if (!check) {
                ERROR(expr->span, "Expected a tuple of identifiers");
            }

            llvm::Value* right = expr->right->accept(*this).unwrap(expr->span);
            if (!this->is_tuple(right->getType())) {
                ERROR(expr->right->span, "Expected a tuple but got '{0}'", this->get_type_name(right->getType()));
            }

            std::vector<llvm::Value*> values = this->unpack(right, tuple->elements.size());
            for (size_t i = 0; i < tuple->elements.size(); i++) {
                auto* var = tuple->elements[i]->as<ast::VariableExpr>();
                auto ref = this->scope->get_local(var->name, true);
                
                if (ref.is_null()) {
                    ERROR(var->span, "Variable '{0}' is not defined", var->name);
                } else if (ref.is_constant) {
                    ERROR(var->span, "Cannot assign to constant");
                } else if (ref.is_immutable) {
                    ERROR(var->span, "Cannot assign to immutable variable '{0}'", ref.name);
                }

                llvm::Value* inst = ref.value;
                llvm::Value* value = values[i];

                if (!this->is_compatible(ref.type, value->getType())) {
                    ERROR(
                        var->span, 
                        "Cannot assign variable of type '{0}' to value of type '{1}'", 
                        this->get_type_name(ref.type), this->get_type_name(value->getType())
                    );
                }

                this->builder->CreateStore(this->cast(value, ref.type), inst);
                this->mark_as_mutated(ref);
            }

            return nullptr;
        }

        auto ref = this->as_reference(expr->left);
        if (ref.is_constant) {
            ERROR(expr->span, "Cannot assign to constant");
        } else if (ref.is_immutable) {
            ERROR(expr->span, "Cannot assign to immutable variable '{0}'", ref.name);
        }

        if (!ref.value) {
            ERROR(expr->left->span, "Left hand side of assignment must be a variable, struct field or array element");
        }

        llvm::Value* inst = ref.value;
        llvm::Value* value = expr->right->accept(*this).unwrap(expr->span);

        if (!this->is_compatible(ref.type, value->getType())) {
            ERROR(
                expr->span, 
                "Cannot assign variable of type '{0}' to value of type '{1}'", 
                this->get_type_name(ref.type), this->get_type_name(value->getType())
            );
        }

        this->mark_as_mutated(ref);
        this->builder->CreateStore(value, inst);

        return value;
    }

    llvm::Value* lhs = expr->left->accept(*this).unwrap(expr->left->span);
    this->ctx = lhs->getType();

    llvm::Value* rhs = expr->right->accept(*this).unwrap(expr->right->span);
    this->ctx = nullptr;

    llvm::Type* ltype = lhs->getType();
    llvm::Type* rtype = rhs->getType();

    std::string err = FORMAT(
        "Unsupported binary operation '{0}' between types '{1}' and '{2}'.", 
        Token::get_type_value(expr->op),
        this->get_type_name(ltype), this->get_type_name(rtype)
    );

    if (this->is_struct(ltype)) {
        auto structure = this->get_struct(ltype);
        if (STRUCT_OP_MAPPING.find(expr->op) != STRUCT_OP_MAPPING.end()) {
            auto method = structure->scope->functions[STRUCT_OP_MAPPING[expr->op]];
            if (!method) {
                utils::error(expr->span, err);
            }

            if (!method->is_operator) {
                utils::error(expr->span, err);
            }

            llvm::Value* self = lhs;
            if (!ltype->isPointerTy()) {
                self = this->as_reference(lhs); // TODO: Find a better way to do this too.
            }

            return this->call(method, {rhs}, self);
        }
    }

    if (!this->is_compatible(ltype, rtype)) {
        utils::error(expr->span, err);
    } else {
        rhs = this->cast(rhs, ltype);
    }

    bool is_floating_point = ltype->isFloatingPointTy();
    llvm::Value* result = nullptr;

    switch (expr->op) {
        case TokenKind::Add:
            if (is_floating_point) {
                result = this->builder->CreateFAdd(lhs, rhs); break;
            } else {
                result = this->builder->CreateAdd(lhs, rhs); break;
            }
        case TokenKind::Minus:
            if (is_floating_point) {
                result = this->builder->CreateFSub(lhs, rhs); break;
            } else {
                result = this->builder->CreateSub(lhs, rhs); break;
            }
        case TokenKind::Mul:
            if (is_floating_point) {
                result = this->builder->CreateFMul(lhs, rhs); break;
            } else {
                result = this->builder->CreateMul(lhs, rhs); break;
            }
        case TokenKind::Div:
            if (is_floating_point) {
                result = this->builder->CreateFDiv(lhs, rhs); break;
            } else {
                result = this->builder->CreateSDiv(lhs, rhs); break;
            }
        case TokenKind::Mod:
            if (is_floating_point) {
                result = this->builder->CreateFRem(lhs, rhs); break;
            } else {
                result = this->builder->CreateSRem(lhs, rhs); break;
            }
        case TokenKind::Eq:
            if (is_floating_point) {
                result = this->builder->CreateFCmpOEQ(lhs, rhs); break;
            } else {
                result = this->builder->CreateICmpEQ(lhs, rhs); break;
            }
        case TokenKind::Neq:
            if (is_floating_point) {
                result = this->builder->CreateFCmpONE(lhs, rhs); break;
            } else {
                result = this->builder->CreateICmpNE(lhs, rhs); break;
            }
        case TokenKind::Gt:
            if (is_floating_point) {
                result = this->builder->CreateFCmpOGT(lhs, rhs); break;
            } else {
                result = this->builder->CreateICmpSGT(lhs, rhs); break;
            }
        case TokenKind::Lt:
            if (is_floating_point) {
                result = this->builder->CreateFCmpOLT(lhs, rhs); break;
            } else {
                result = this->builder->CreateICmpSLT(lhs, rhs); break;
            }
        case TokenKind::Gte:
            if (is_floating_point) {
                result = this->builder->CreateFCmpOGE(lhs, rhs); break;
            } else {
                result = this->builder->CreateICmpSGE(lhs, rhs); break;
            }
        case TokenKind::Lte:
            if (is_floating_point) {
                result = this->builder->CreateFCmpOLE(lhs, rhs); break;
            } else {
                result = this->builder->CreateICmpSLE(lhs, rhs); break;
            }
        case TokenKind::And:
            result = this->builder->CreateAnd(
                this->cast(lhs, this->builder->getInt1Ty()), 
                this->cast(rhs, this->builder->getInt1Ty())
            );
            break;
        case TokenKind::Or:
            result = this->builder->CreateOr(
                this->cast(lhs, this->builder->getInt1Ty()), 
                this->cast(rhs, this->builder->getInt1Ty())
            );
            break;
        case TokenKind::BinaryAnd:
            result = this->builder->CreateAnd(lhs, rhs); break;
        case TokenKind::BinaryOr:
            result = this->builder->CreateOr(lhs, rhs); break;
        case TokenKind::Xor:
            result = this->builder->CreateXor(lhs, rhs); break;
        case TokenKind::Lsh:
            result = this->builder->CreateShl(lhs, rhs); break;
        case TokenKind::Rsh:
            result = this->builder->CreateLShr(lhs, rhs); break;
        default: __UNREACHABLE
    }

    return result;
}

Value Visitor::visit(ast::InplaceBinaryOpExpr* expr) {
    llvm::Value* rhs = expr->right->accept(*this).unwrap(expr->span);

    auto ref = this->as_reference(expr->left);
    if (ref.is_constant) {
        ERROR(expr->span, "Cannot assign to constant");
    } else if (ref.is_immutable) {
        ERROR(expr->span, "Cannot assign to immutable variable '{0}'", ref.name);
    }

    llvm::Value* parent = ref.value;
    llvm::Value* lhs = this->load(parent, ref.type);

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
        default: __UNREACHABLE
    }

    this->mark_as_mutated(ref);
    this->builder->CreateStore(result, parent);
    
    return result;
}
