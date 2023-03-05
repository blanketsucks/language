#include "visitor.h"

Value Visitor::visit(ast::WhileExpr* expr) {
    llvm::Value* condition = expr->condition->accept(*this).unwrap(expr->condition->span);
    llvm::Function* function = this->builder->GetInsertBlock()->getParent();

    auto func = this->current_function;

    llvm::BasicBlock* loop = llvm::BasicBlock::Create(*this->context, "", function);
    llvm::BasicBlock* end = llvm::BasicBlock::Create(*this->context);

    Branch* branch = func->current_branch;

    this->builder->CreateCondBr(this->cast(condition, this->builder->getInt1Ty()), loop, end);
    this->set_insert_point(loop, false);

    func->current_branch = func->create_branch(loop, end);
    expr->body->accept(*this);

    if (func->current_branch->has_jump()) {
        this->set_insert_point(end);
        func->current_branch = branch;

        return nullptr;
    }

    condition = expr->condition->accept(*this).unwrap(expr->condition->span);
    this->builder->CreateCondBr(this->cast(condition, this->builder->getInt1Ty()), loop, end);

    this->set_insert_point(end);
    func->current_branch = branch;

    return nullptr;
}

Value Visitor::visit(ast::ForExpr* expr) {
    llvm::Function* function = this->builder->GetInsertBlock()->getParent();
    auto func = this->current_function;

    llvm::BasicBlock* loop = llvm::BasicBlock::Create(*this->context, "", function);
    llvm::BasicBlock* stop = llvm::BasicBlock::Create(*this->context);

    Branch* branch = func->current_branch;

    expr->start->accept(*this).unwrap(expr->start->span);
    llvm::Value* end = expr->end->accept(*this).unwrap(expr->end->span);

    this->builder->CreateCondBr(this->cast(end, this->builder->getInt1Ty()), loop, stop);
    this->set_insert_point(loop, false);

    func->current_branch = func->create_branch(loop, stop); 
    expr->body->accept(*this);

    if (func->current_branch->has_jump()) {
        this->set_insert_point(stop);
        func->current_branch = branch;

        return nullptr;
    }
    
    expr->step->accept(*this).unwrap(expr->step->span);
    end = expr->end->accept(*this).unwrap(expr->end->span);

    this->builder->CreateCondBr(this->cast(end, this->builder->getInt1Ty()), loop, stop);
    this->set_insert_point(stop);

    func->current_branch = branch;
    return nullptr;
}

Value Visitor::visit(ast::BreakExpr*) {
    auto func = this->current_function;
    func->current_branch->has_break = true;

    this->builder->CreateBr(func->current_branch->end);
    return nullptr;
}

Value Visitor::visit(ast::ContinueExpr*) {
    auto func = this->current_function;
    func->current_branch->has_continue = true;

    this->builder->CreateBr(func->current_branch->loop);
    return nullptr;
}

Value Visitor::visit(ast::ForeachExpr* expr) {
    llvm::Value* iterable = expr->iterable->accept(*this).unwrap(expr->iterable->span);
    std::string err = FORMAT("Cannot iterate over value of type '{0}'", this->get_type_name(iterable->getType()));

    if (!this->is_struct(iterable->getType())) {
        utils::error(expr->iterable->span, err);
    }

    llvm::Value* self = iterable;
    llvm::Type* itype = iterable->getType();

    if (!itype->isPointerTy()) {
        self = this->as_reference(self);
        if (!self) {
            self = this->create_alloca(itype);
            this->builder->CreateStore(iterable, self);
        }
    }

    auto structure = this->get_struct(itype);
    if (structure->has_method("iter")) {
        auto iter = structure->get_method("iter");
        if (!iter->is_operator) {
            utils::error(expr->iterable->span, err);
        }

        iterable = this->call(iter->value, {}, self);
        itype = iterable->getType();

        if (!this->is_struct(itype)) {
            utils::error(expr->iterable->span, err);
        }

        self = iterable;
        if (!itype->isPointerTy()) {
            self = this->as_reference(self);
            if (!self) {
                self = this->create_alloca(itype);
                this->builder->CreateStore(iterable, self);
            }
        }

        structure = this->get_struct(itype);
    }

    if (!structure->has_method("next")) {
        utils::error(expr->iterable->span, err);
    }

    auto next = structure->get_method("next");
    if (!next->ret->isStructTy()) {
        ERROR(next->span, "Return value of next() must be a tuple of T and bool");
    }

    auto name = next->ret->getStructName();
    if (!name.startswith("__tuple")) {
        ERROR(next->span, "Return value of next() must be a tuple of T and bool");
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

    Branch* branch = func->current_branch;

    this->builder->CreateCondBr(ok, loop, stop);
    this->set_insert_point(loop, false);

    func->current_branch = func->create_branch(loop, stop);

    this->scope->variables[expr->name] = Variable::from_alloca(expr->name, alloca);
    expr->body->accept(*this);

    if (func->current_branch->has_jump()) {
        this->set_insert_point(stop);
        func->current_branch = branch;

        return nullptr;
    }

    tuple = this->call(next->value, {}, self);

    ok = this->builder->CreateExtractValue(tuple, 1);
    value = this->builder->CreateExtractValue(tuple, 0);

    this->builder->CreateStore(value, alloca);

    this->builder->CreateCondBr(ok, loop, stop);
    this->set_insert_point(stop);

    func->current_branch = branch;
    return nullptr;
}