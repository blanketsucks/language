#include "visitor.h"

Value Visitor::visit(ast::WhileExpr* expr) {
    llvm::Value* condition = expr->condition->accept(*this).unwrap(expr->condition->start);
    llvm::Function* function = this->builder->GetInsertBlock()->getParent();

    Function* func = this->current_function;

    llvm::BasicBlock* loop = llvm::BasicBlock::Create(this->context, "", function);
    llvm::BasicBlock* end = llvm::BasicBlock::Create(this->context);

    Branch* branch = func->branch;

    this->builder->CreateCondBr(this->cast(condition, BooleanType), loop, end);
    this->builder->SetInsertPoint(loop);

    func->branch = func->create_branch("while.loop", loop, end);
    expr->body->accept(*this);

    if (func->branch->has_jump()) {
        function->getBasicBlockList().push_back(end);
        this->builder->SetInsertPoint(end);

        func->branch = branch;
        return this->constants["null"];
    }

    condition = expr->condition->accept(*this).unwrap(expr->condition->start);
    this->builder->CreateCondBr(this->cast(condition, BooleanType), loop, end);

    function->getBasicBlockList().push_back(end);
    this->builder->SetInsertPoint(end);

    func->branch = branch;
    return this->constants["null"];
}

Value Visitor::visit(ast::ForExpr* expr) {
    llvm::Function* function = this->builder->GetInsertBlock()->getParent();
    Function* func = this->current_function;

    llvm::BasicBlock* loop = llvm::BasicBlock::Create(this->context, "", function);
    llvm::BasicBlock* stop = llvm::BasicBlock::Create(this->context);

    Branch* branch = func->branch;

    expr->start->accept(*this).unwrap(expr->start->start);
    llvm::Value* end = expr->end->accept(*this).unwrap(expr->end->start);

    this->builder->CreateCondBr(this->cast(end, BooleanType), loop, stop);
    this->builder->SetInsertPoint(loop);

    func->branch = func->create_branch("for.loop", loop, stop); 
    expr->body->accept(*this);

    if (func->branch->has_jump()) {
        function->getBasicBlockList().push_back(stop);
        this->builder->SetInsertPoint(stop);

        func->branch = branch;
        return this->constants["null"];
    }
    
    expr->step->accept(*this).unwrap(expr->step->start);
    end = expr->end->accept(*this).unwrap(expr->end->start);

    this->builder->CreateCondBr(this->cast(end, BooleanType), loop, stop);

    function->getBasicBlockList().push_back(stop);
    this->builder->SetInsertPoint(stop);

    func->branch = branch;
    return this->constants["null"];
}

Value Visitor::visit(ast::BreakExpr* expr) {
    UNUSED(expr);

    Function* func = this->current_function;
    func->branch->has_break = true;

    this->builder->CreateBr(func->branch->end);
    return this->constants["null"];
}

Value Visitor::visit(ast::ContinueExpr* expr) {
    UNUSED(expr);

    Function* func = this->current_function;
    func->branch->has_continue = true;

    this->builder->CreateBr(func->branch->loop);
    return this->constants["null"];
}