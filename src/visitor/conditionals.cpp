#include <quart/visitor.h>

using namespace quart;

llvm::BasicBlock* Visitor::create_if_statement(llvm::Value* condition) {
    llvm::BasicBlock* block = this->builder->GetInsertBlock();
    llvm::Function* function = block->getParent();

    llvm::BasicBlock* then = llvm::BasicBlock::Create(*this->context, "", function);
    llvm::BasicBlock* merge = llvm::BasicBlock::Create(*this->context);

    this->builder->CreateCondBr(condition, then, merge);
    this->builder->SetInsertPoint(then);

    return merge;
}

Value Visitor::visit(ast::IfExpr* expr) {
    Value condition = expr->condition->accept(*this);
    if (condition.is_empty_value()) ERROR(expr->condition->span, "Expected a value");

    RefPtr<Function> function = this->current_function;

    llvm::BasicBlock* then = llvm::BasicBlock::Create(*this->context, "", function->value);
    llvm::BasicBlock* else_ = llvm::BasicBlock::Create(*this->context);

    if (!condition->getType()->isIntegerTy(1)) {
        condition = {this->builder->CreateIsNotNull(condition), this->registry->create_int_type(1, true)};
    }

    this->builder->CreateCondBr(condition, then, else_);
    this->set_insert_point(then, false);

    expr->body->accept(*this);

    /*

    There are a couple of cases to take in consideration:

    1. There is an if body and no else body:
        1.1 The if body contains a jump. 
            - In this case, we push the else block and set it as the insert point.
        1.2 The if body doesn't contain a jump. 
            - In this case, we branch to the else block and set it as the insert point.
    2. There is an else body:
        2.1 The if body contains a jump. 
            - In this case we push the else block and set it as the insert point and generate code for the else body.
        2.2 The if body doesn't contain a jump.
            - In this case, we branch to a merge block and then set the else block as the insert point.
        2.3 The else body doesn't contain a jump.
            - In this case, we branch to the merge block and set it as the insert point.
        2.4 The else body contains a jump.
            - In this case, unlike 2.3 we don't branch to the merge block, we just set it as the insert point.

    A jump can be either a `return`, `break` or a `continue` since these statements cause a branch in the LLVM IR.
    
    */
    if (!expr->else_body) {
        if (!function->current_block->getTerminator()) {
            this->builder->CreateBr(else_);
            this->set_insert_point(else_);

            return nullptr; 
        }

        this->set_insert_point(else_);
        return nullptr;
    }

    if (function->current_block->getTerminator()) {
        this->set_insert_point(else_);
        expr->else_body->accept(*this);

        return nullptr;
    }

    llvm::BasicBlock* merge = llvm::BasicBlock::Create(*this->context);
    this->builder->CreateBr(merge);

    this->set_insert_point(else_);
    expr->else_body->accept(*this);

    if (function->current_block->getTerminator()) {
        this->set_insert_point(merge);
        return nullptr;
    }

    this->builder->CreateBr(merge);
    this->set_insert_point(merge);

    return nullptr;
}

static Value parse_branchless_ternary(
    const Value& condition,
    OwnPtr<ast::Expr>& true_expr,
    OwnPtr<ast::Expr>& false_expr,
    Visitor& visitor
) {
    Value true_value = true_expr->accept(visitor);
    if (true_value.is_empty_value()) ERROR(true_expr->span, "Expected a value");

    Value false_value = false_expr->accept(visitor);
    if (false_value.is_empty_value()) ERROR(false_expr->span, "Expected a value");

    if (!Type::can_safely_cast_to(false_value.type, true_value.type)) {
        ERROR(false_expr->span, "The true and false expressions of a ternary expression must have the same type");
    }

    false_value = visitor.cast(false_value, true_value.type);
    return {
        visitor.builder->CreateSelect(condition, true_value, false_value),
        true_value.type
    };
}

Value Visitor::visit(ast::TernaryExpr* expr) {
    Value condition = expr->condition->accept(*this);
    if (condition.is_empty_value()) ERROR(expr->condition->span, "Expected a value");

    quart::Type* boolean = this->registry->create_int_type(1, true);
    if (!Type::can_safely_cast_to(condition.type, boolean)) {
        ERROR(expr->condition->span, "Expected a boolean expression in the condition of a ternary expression");
    }

    condition = this->cast(condition, boolean);

    // If either the true or false expression is a function call, we can't use a select since it will execute the function call
    // before giving the result.

    bool can_use_select = expr->true_expr->kind() != ast::ExprKind::Call && expr->false_expr->kind() != ast::ExprKind::Call;
    if (can_use_select) {
        return parse_branchless_ternary(condition, expr->true_expr, expr->false_expr, *this);
    }

    llvm::BasicBlock* then = llvm::BasicBlock::Create(*this->context);
    llvm::BasicBlock* else_ = llvm::BasicBlock::Create(*this->context);
    llvm::BasicBlock* merge = llvm::BasicBlock::Create(*this->context);

    this->builder->CreateCondBr(condition, then, else_);
    this->set_insert_point(then);

    Value true_value = expr->true_expr->accept(*this);
    if (true_value.is_empty_value()) ERROR(expr->true_expr->span, "Expected a value");

    this->builder->CreateBr(merge);
    this->set_insert_point(else_);

    Value false_value = expr->false_expr->accept(*this);
    if (false_value.is_empty_value()) ERROR(expr->false_expr->span, "Expected a value");

    this->builder->CreateBr(merge);
    this->set_insert_point(merge);

    if (!Type::can_safely_cast_to(true_value.type, false_value.type)) {
        ERROR(expr->false_expr->span, "The true and false expressions of a ternary expression must have the same type");
    }

    llvm::PHINode* phi = this->builder->CreatePHI(true_value->getType(), 2);
    
    phi->addIncoming(true_value, then);
    phi->addIncoming(false_value, else_);

    return {phi, true_value.type};
}