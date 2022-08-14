#include "visitor.h"

Value Visitor::visit(ast::IfExpr* expr) {
    llvm::Value* condition = expr->condition->accept(*this).unwrap(this, expr->condition->start);

    Function* func = this->current_function;
    llvm::Function* function = this->builder->GetInsertBlock()->getParent();

    llvm::BasicBlock* then = llvm::BasicBlock::Create(this->context, "", function);
    llvm::BasicBlock* else_ = llvm::BasicBlock::Create(this->context);

    this->builder->CreateCondBr(this->cast(condition, BooleanType), then, else_);
    this->builder->SetInsertPoint(then);

    Branch* branch = func->branch;
    func->branch = func->create_branch("if.then");

    expr->body->accept(*this);

    /*

    There are a couple of cases to take in consideration:

    1. There is an if body and no else body:
        1.1 The if body contains a return statement. 
            - In this case, we push the else block and set it as the insert point.
        1.2 The if body doesn't contain a return statement. 
            - In this case, we branch to the else block and set it as the insert point.
    2. There is an else body:
        2.1 The if body contains a return statement. 
            - In this case we push the else block and set it as the insert point and generate code for the else body.
        2.2 The if body doesn't contain a return statement.
            - In this case, we branch to a merge block and then set the else block as the insert point.
        2.3 The else body doesn't contain a return statement.
            - In this case, we branch to the merge block and set it as the insert point.
        2.4 The else body contains a return statement.
            - In this case, unlike 2.3 we don't branch to the merge block, we just set it as the insert point.

    */
    if (!expr->ebody) {
        if (!func->branch->has_return) {
            this->builder->CreateBr(else_);

            function->getBasicBlockList().push_back(else_);
            this->builder->SetInsertPoint(else_);

            func->branch = branch;
            return nullptr; 
        } else {
            function->getBasicBlockList().push_back(else_);
            this->builder->SetInsertPoint(else_);
        
            func->branch = branch;
            return nullptr;
        }
    }

    if (func->branch->has_return) {
        function->getBasicBlockList().push_back(else_);
        this->builder->SetInsertPoint(else_);

        expr->ebody->accept(*this);
        func->branch = branch;

        return nullptr;
    }

    llvm::BasicBlock* merge = llvm::BasicBlock::Create(this->context);
    this->builder->CreateBr(merge);

    function->getBasicBlockList().push_back(else_);
    this->builder->SetInsertPoint(else_);
    
    func->branch = func->create_branch("if.else");
    expr->ebody->accept(*this);

    if (func->branch->has_return) {
        function->getBasicBlockList().push_back(merge);
        this->builder->SetInsertPoint(merge);

        func->branch = branch;
        return nullptr;
    }

    this->builder->CreateBr(merge);

    function->getBasicBlockList().push_back(merge);
    this->builder->SetInsertPoint(merge);

    func->branch = branch;
    return nullptr;
}
