#include "parser/ast.h"
#include "visitor.h"

Value Visitor::visit(ast::WhileExpr* expr) {
    llvm::Value* condition = expr->condition->accept(*this).unwrap(expr->condition->start);
    llvm::Function* function = this->builder->GetInsertBlock()->getParent();

    auto func = this->current_function;

    llvm::BasicBlock* loop = llvm::BasicBlock::Create(*this->context, "", function);
    llvm::BasicBlock* end = llvm::BasicBlock::Create(*this->context);

    Branch* branch = func->branch;

    this->builder->CreateCondBr(this->cast(condition, this->builder->getInt1Ty()), loop, end);
    this->set_insert_point(loop, false);

    func->branch = func->create_branch("while.loop", loop, end);
    expr->body->accept(*this);

    if (func->branch->has_jump()) {
        this->set_insert_point(end);
        func->branch = branch;

        return nullptr;
    }

    condition = expr->condition->accept(*this).unwrap(expr->condition->start);
    this->builder->CreateCondBr(this->cast(condition, this->builder->getInt1Ty()), loop, end);

    this->set_insert_point(end);
    func->branch = branch;

    return nullptr;
}

Value Visitor::visit(ast::ForExpr* expr) {
    llvm::Function* function = this->builder->GetInsertBlock()->getParent();
    auto func = this->current_function;

    llvm::BasicBlock* loop = llvm::BasicBlock::Create(*this->context, "", function);
    llvm::BasicBlock* stop = llvm::BasicBlock::Create(*this->context);

    Branch* branch = func->branch;

    expr->start->accept(*this).unwrap(expr->start->start);
    llvm::Value* end = expr->end->accept(*this).unwrap(expr->end->start);

    this->builder->CreateCondBr(this->cast(end, this->builder->getInt1Ty()), loop, stop);
    this->set_insert_point(loop, false);

    func->branch = func->create_branch("for.loop", loop, stop); 
    expr->body->accept(*this);

    if (func->branch->has_jump()) {
        this->set_insert_point(stop);
        func->branch = branch;

        return nullptr;
    }
    
    expr->step->accept(*this).unwrap(expr->step->start);
    end = expr->end->accept(*this).unwrap(expr->end->start);

    this->builder->CreateCondBr(this->cast(end, this->builder->getInt1Ty()), loop, stop);
    this->set_insert_point(stop);

    func->branch = branch;
    return nullptr;
}

Value Visitor::visit(ast::BreakExpr* expr) {
    UNUSED(expr);

    auto func = this->current_function;
    func->branch->has_break = true;

    this->builder->CreateBr(func->branch->end);
    return nullptr;
}

Value Visitor::visit(ast::ContinueExpr* expr) {
    UNUSED(expr);

    auto func = this->current_function;
    func->branch->has_continue = true;

    this->builder->CreateBr(func->branch->loop);
    return nullptr;
}

Value Visitor::visit(ast::ForeachExpr* expr) {
    llvm::Value* iterable = expr->iterable->accept(*this).unwrap(expr->iterable->start);
    if (!this->is_struct(iterable->getType())) {
        ERROR(expr->iterable->start, "Cannot iterate over non-struct type");
    }

    llvm::Value* self = iterable;
    llvm::Type* itype = iterable->getType();

    if (!itype->isPointerTy()) {
        self = this->get_pointer_from_expr(expr->iterable.get()).first;

        if (!self) {
            self = this->create_alloca(itype);
            this->builder->CreateStore(iterable, self);
        }
    }

    auto structure = this->get_struct(itype);
    if (structure->has_method("iter")) {
        auto iter = structure->get_method("iter");
        iterable = this->call(iter->value, {}, self);
        itype = iterable->getType();

        if (!this->is_struct(itype)) {
            ERROR(expr->iterable->start, "Cannot iterate over non-struct type");
        }

        self = iterable;
        if (!itype->isPointerTy()) {
            self = this->create_alloca(itype);
            this->builder->CreateStore(iterable, self);
        }

        structure = this->get_struct(itype);
    }

    if (!structure->has_method("next")) {
        ERROR(expr->iterable->start, "Cannot iterate over non-iterable type");
    }

    auto next = structure->get_method("next");
    if (!next->ret->isStructTy()) {
        ERROR(next->start, "Return value of next() must be a tuple of T and bool");
    }

    auto name = next->ret->getStructName();
    if (!name.startswith("__tuple")) {
        ERROR(next->start, "Return value of next() must be a tuple of T and bool");
    }

    llvm::Type* type = next->ret->getStructElementType(0);
    
    llvm::Value* tuple = this->call(next->value, {}, self);

    llvm::Value* ok = this->builder->CreateExtractValue(tuple, 1);
    llvm::Value* value = this->builder->CreateExtractValue(tuple, 0);

    llvm::AllocaInst* alloca = this->create_alloca(type);
    this->builder->CreateStore(value, alloca);

    llvm::Function* function = this->builder->GetInsertBlock()->getParent();

    auto func = this->current_function;

    llvm::BasicBlock* loop = llvm::BasicBlock::Create(*this->context, "", function);
    llvm::BasicBlock* stop = llvm::BasicBlock::Create(*this->context);

    Branch* branch = func->branch;

    this->builder->CreateCondBr(ok, loop, stop);
    this->set_insert_point(loop, false);

    func->branch = func->create_branch("foreach.loop", loop, stop);

    this->scope->variables[expr->name] = alloca;
    expr->body->accept(*this);

    if (func->branch->has_jump()) {
        this->set_insert_point(stop);
        func->branch = branch;

        return nullptr;
    }

    tuple = this->call(next->value, {}, self);

    ok = this->builder->CreateExtractValue(tuple, 1);
    value = this->builder->CreateExtractValue(tuple, 0);

    this->builder->CreateStore(value, alloca);

    this->builder->CreateCondBr(ok, loop, stop);
    this->set_insert_point(stop);

    func->branch = branch;
    return nullptr;
}