#include <quart/visitor.h>

using namespace quart;

Value Visitor::visit(ast::WhileExpr* expr) {
    Value condition = expr->condition->accept(*this);
    if (condition.is_empty_value()) ERROR(expr->condition->span, "Expected a value");

    if (!condition->getType()->isIntegerTy(1)) {
        condition = this->builder->CreateIsNotNull(condition);
    }

    FunctionRef function = this->current_function;

    llvm::BasicBlock* loop = llvm::BasicBlock::Create(*this->context, "", function->value);
    llvm::BasicBlock* end = llvm::BasicBlock::Create(*this->context);

    Loop current_loop = function->loop;
    function->loop = Loop{loop, end};

    this->builder->CreateCondBr(condition, loop, end);
    this->set_insert_point(loop, false);

    expr->body->accept(*this);
    if (function->current_block->getTerminator()) {
        this->set_insert_point(end);
        function->loop = current_loop;

        return nullptr;
    }

    condition = expr->condition->accept(*this);
    if (!condition->getType()->isIntegerTy(1)) {
        condition = this->builder->CreateIsNotNull(condition);
    }

    this->builder->CreateCondBr(condition, loop, end);
    this->set_insert_point(end);

    function->loop = current_loop;
    return nullptr;
}

Value Visitor::visit(ast::RangeForExpr* expr) {
    FunctionRef function = this->current_function;

    llvm::BasicBlock* loop = llvm::BasicBlock::Create(*this->context, "", function->value);
    llvm::BasicBlock* stop = llvm::BasicBlock::Create(*this->context);

    Loop current_loop = function->loop;
    function->loop = Loop{loop, stop};

    Value start = expr->start->accept(*this);
    if (!start.type->is_int()) {
        ERROR(expr->start->span, "Expected integer type, found '{0}'", start.type->get_as_string());
    }

    llvm::AllocaInst* alloca = this->alloca(start->getType());
    this->builder->CreateStore(start, alloca);

    u8 flags = expr->name.is_mutable ? Variable::Mutable : Variable::None;
    this->scope->variables[expr->name.value] = Variable::from_alloca(
        expr->name.value, alloca, start.type, flags, expr->name.span
    );

    Value end = EMPTY_VALUE;
    if (expr->end) {
        end = expr->end->accept(*this);
        if (!end.type->is_int()) {
            ERROR(expr->end->span, "Expected integer type, found '{0}'", end.type->get_as_string());
        }

        this->builder->CreateCondBr(this->builder->CreateICmpSLT(start, end), loop, stop);
    } else {
        this->builder->CreateBr(loop);
    }

    this->set_insert_point(loop, false);
    expr->body->accept(*this);

    if (function->current_block->getTerminator()) {
        this->set_insert_point(stop);
        function->loop = current_loop;

        return nullptr;
    }
    
    llvm::Value* value = this->builder->CreateAdd(this->load(alloca), this->to_int(1));
    this->builder->CreateStore(value, alloca);
    
    if (end) {
        this->builder->CreateCondBr(this->builder->CreateICmpSLT(value, end), loop, stop);
    } else {
        this->builder->CreateBr(loop);
    }

    function->loop = current_loop;
    this->set_insert_point(stop);

    return nullptr;
}

Value Visitor::visit(ast::BreakExpr*) {
    auto func = this->current_function;
    this->builder->CreateBr(func->loop.end);

    return nullptr;
}

Value Visitor::visit(ast::ContinueExpr*) {
    auto func = this->current_function;
    this->builder->CreateBr(func->loop.start);

    return nullptr;
}

Value Visitor::visit(ast::ForExpr* expr) {
    TODO("Rewrite");
    return EMPTY_VALUE;
}