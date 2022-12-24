#include "visitor.h"

Value Visitor::visit(ast::IfExpr* expr) {
    llvm::Value* condition = expr->condition->accept(*this).unwrap(expr->condition->start);

    auto func = this->current_function;
    llvm::Function* function = this->builder->GetInsertBlock()->getParent();

    llvm::BasicBlock* then = llvm::BasicBlock::Create(*this->context, "", function);
    llvm::BasicBlock* else_ = llvm::BasicBlock::Create(*this->context);

    if (!condition->getType()->isIntegerTy(1)) {
        condition = this->builder->CreateIsNotNull(condition);
    }

    this->builder->CreateCondBr(condition, then, else_);
    this->set_insert_point(then, false);

    Branch* branch = func->branch;
    func->branch = func->create_branch("if.then", branch->loop, branch->end);

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
        if (!func->branch->has_jump()) {
            this->builder->CreateBr(else_);
            this->set_insert_point(else_);

            func->branch = branch;
            return nullptr; 
        } else {
            this->set_insert_point(else_);
            func->branch = branch;

            return nullptr;
        }
    }

    if (func->branch->has_jump()) {
        this->set_insert_point(else_);
        expr->ebody->accept(*this);

        func->branch = branch;
        return nullptr;
    }

    llvm::BasicBlock* merge = llvm::BasicBlock::Create(*this->context);
    this->builder->CreateBr(merge);

    this->set_insert_point(else_);
    
    func->branch = func->create_branch("if.else", branch->loop, branch->end);
    expr->ebody->accept(*this);

    if (func->branch->has_jump()) {
        this->set_insert_point(merge);
        func->branch = branch;

        return nullptr;
    }

    this->builder->CreateBr(merge);
    this->set_insert_point(merge);

    func->branch = branch;
    return nullptr;
}

Value Visitor::visit(ast::TernaryExpr* expr) {
    llvm::Value* condition = expr->condition->accept(*this).unwrap(expr->condition->start);
    if (!this->is_compatible(this->builder->getInt1Ty(), condition->getType())) {
        ERROR(expr->condition->start, "Expected a boolean expression in the condition of a ternary expression");
    }

    llvm::BasicBlock* then = llvm::BasicBlock::Create(*this->context);
    llvm::BasicBlock* else_ = llvm::BasicBlock::Create(*this->context);
    llvm::BasicBlock* merge = llvm::BasicBlock::Create(*this->context);

    this->builder->CreateCondBr(condition, then, else_);
    this->set_insert_point(then);

    llvm::Value* true_value = expr->true_expr->accept(*this).unwrap(expr->true_expr->start);

    this->builder->CreateBr(merge);
    this->set_insert_point(else_);

    llvm::Value* false_value = expr->false_expr->accept(*this).unwrap(expr->false_expr->start);

    this->builder->CreateBr(merge);
    this->set_insert_point(merge);

    if (!this->is_compatible(true_value->getType(), false_value->getType())) {
        ERROR(expr->start, "The true and false expressions of a ternary expression must have the same type");
    }

    llvm::PHINode* phi = this->builder->CreatePHI(true_value->getType(), 2);
    
    phi->addIncoming(true_value, then);
    phi->addIncoming(false_value, else_);

    return phi;
}