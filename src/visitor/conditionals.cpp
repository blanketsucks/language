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

    FunctionRef function = this->current_function;

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
    if (!expr->ebody) {
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
        expr->ebody->accept(*this);

        return nullptr;
    }

    llvm::BasicBlock* merge = llvm::BasicBlock::Create(*this->context);
    this->builder->CreateBr(merge);

    this->set_insert_point(else_);
    expr->ebody->accept(*this);

    if (function->current_block->getTerminator()) {
        this->set_insert_point(merge);
        return nullptr;
    }

    this->builder->CreateBr(merge);
    this->set_insert_point(merge);

    return nullptr;
}

Value Visitor::visit(ast::TernaryExpr* expr) {
    Value condition = expr->condition->accept(*this);
    if (condition.is_empty_value()) ERROR(expr->condition->span, "Expected a value");

    quart::Type* boolean = this->registry->create_int_type(1, true);
    if (!Type::can_safely_cast_to(condition.type, boolean)) {
        ERROR(expr->condition->span, "Expected a boolean expression in the condition of a ternary expression");
    }

    condition = this->cast(condition, boolean);

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