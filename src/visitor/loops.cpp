#include <quart/visitor.h>

Value Visitor::visit(ast::WhileExpr* expr) {
    llvm::Value* condition = expr->condition->accept(*this).unwrap(expr->condition->span);
    if (!condition->getType()->isIntegerTy(1)) {
        condition = this->builder->CreateIsNotNull(condition);
    }

    llvm::Function* function = this->builder->GetInsertBlock()->getParent();

    auto func = this->current_function;

    llvm::BasicBlock* loop = llvm::BasicBlock::Create(*this->context, "", function);
    llvm::BasicBlock* end = llvm::BasicBlock::Create(*this->context);

    Branch* branch = func->current_branch;

    this->builder->CreateCondBr(condition, loop, end);
    this->set_insert_point(loop, false);

    func->current_branch = func->create_branch(loop, end);
    expr->body->accept(*this);

    if (func->current_branch->has_jump()) {
        this->set_insert_point(end);
        func->current_branch = branch;

        return nullptr;
    }

    condition = expr->condition->accept(*this).unwrap(expr->condition->span);
    if (!condition->getType()->isIntegerTy(1)) {
        condition = this->builder->CreateIsNotNull(condition);
    }

    this->builder->CreateCondBr(condition, loop, end);

    this->set_insert_point(end);
    func->current_branch = branch;

    return nullptr;
}

Value Visitor::visit(ast::RangeForExpr* expr) {
    llvm::Function* function = this->builder->GetInsertBlock()->getParent();
    auto func = this->current_function;

    llvm::BasicBlock* loop = llvm::BasicBlock::Create(*this->context, "", function);
    llvm::BasicBlock* stop = llvm::BasicBlock::Create(*this->context);

    Branch* branch = func->current_branch;

    llvm::Value* start = expr->start->accept(*this).unwrap(expr->start->span);
    if (!start->getType()->isIntegerTy()) {
        ERROR(expr->start->span, "Expected integer type, found '{0}'", this->get_type_name(start->getType()));
    }

    llvm::AllocaInst* alloca = this->alloca(start->getType());
    this->builder->CreateStore(start, alloca);

    this->scope->variables[expr->name.value] = Variable::from_alloca(
        expr->name.value, alloca, expr->name.is_immutable, expr->name.span
    );

    llvm::Value* end = nullptr;
    if (expr->end) {
        end = expr->end->accept(*this).unwrap(expr->end->span);
        if (!end->getType()->isIntegerTy()) {
            ERROR(expr->end->span, "Expected integer type, found '{0}'", this->get_type_name(end->getType()));
        }

        this->builder->CreateCondBr(this->builder->CreateICmpSLT(this->load(alloca), end), loop, stop);
    } else {
        this->builder->CreateBr(loop);
    }

    this->set_insert_point(loop, false);

    func->current_branch = func->create_branch(loop, stop); 
    expr->body->accept(*this);

    if (func->current_branch->has_jump()) {
        this->set_insert_point(stop);
        func->current_branch = branch;

        return nullptr;
    }
    
    llvm::Value* value = this->builder->CreateAdd(this->load(alloca), this->to_int(1));
    this->builder->CreateStore(value, alloca);
    
    if (end) {
        this->builder->CreateCondBr(this->builder->CreateICmpSLT(value, end), loop, stop);
    } else {
        this->builder->CreateBr(loop);
    }
    
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

Value Visitor::visit(ast::ForExpr* expr) {
    auto ref = this->as_reference(expr->iterable);
    if (ref.is_null()) {
        llvm::Value* value = expr->iterable->accept(*this).unwrap(expr->iterable->span);

        ref.value = value;
        ref.type = value->getType();

        if (!ref.type->isPointerTy()) {
            ref.value = this->alloca(ref.type);
            this->builder->CreateStore(value, ref.value);

            ref.type = ref.type->getPointerTo();
        }
    } else {
        ref.type = ref.value->getType();

        if (this->get_pointer_depth(ref.type) > 1) {
            ref.value = this->load(ref.value);
            ref.type = ref.value->getType();
        }
    }

    if (!this->is_struct(ref.type)) {
        ERROR(expr->iterable->span, "Cannot iterate over value of type '{0}'", this->get_type_name(ref.type));
    }

    auto structure = this->get_struct(ref.type);
    if (structure->has_method("iter")) {
        auto method = structure->get_method("iter");
        if (!method->is_operator) {
            ERROR(expr->iterable->span, "Cannot iterate over value of type '{0}'", this->get_type_name(ref.type));
        }

        FunctionArgument& self = method->args[0];
        if (!self.is_immutable && ref.is_immutable) {
            ERROR(expr->iterable->span, "Cannot pass immutable reference to mutable argument 'self' to call to 'iter'");
        }

        if (!ref.is_immutable) {
            this->mark_as_mutated(ref);
        }

        Value value = this->call(method, {}, ref.value);
        llvm::Value* iterable = value.unwrap(expr->iterable->span);

        if (!this->is_struct(iterable->getType())) {
            ERROR(expr->iterable->span, "Cannot iterate over value of type '{0}'", this->get_type_name(ref.type));
        }

        ref.type = iterable->getType();
        ref.is_immutable = value.is_immutable;

        if (!ref.type->isPointerTy()) {
            ref.value = this->alloca(ref.type);
            this->builder->CreateStore(iterable, ref.value);

            ref.type = ref.type->getPointerTo();
        } else {
            ref.value = iterable;
        }

        structure = this->get_struct(ref.type);
    }

    if (!structure->has_method("next")) {
        ERROR(expr->iterable->span, "Cannot iterate over value of type '{0}'", this->get_type_name(ref.type));
    }

    auto next = structure->get_method("next");
    if (!next->ret->isStructTy()) {
        ERROR(next->span, "Return value of next() must be a tuple of T and bool");
    }

    auto name = next->ret->getStructName();
    if (!name.startswith("__tuple")) {
        ERROR(next->span, "Return value of next() must be a tuple of T and bool");
    }

    FunctionArgument& self = next->args[0];
    if (!self.is_immutable && ref.is_immutable) {
        ERROR(expr->iterable->span, "Cannot pass immutable reference to mutable argument 'self' to call to 'next'");
    }

    if (!ref.is_immutable) {
        this->mark_as_mutated(ref);
    }

    llvm::Type* type = next->ret->getStructElementType(0);
    
    llvm::Value* tuple = this->call(next->value, {}, ref.value);

    llvm::Value* ok = this->builder->CreateExtractValue(tuple, 1);
    llvm::Value* value = this->builder->CreateExtractValue(tuple, 0);

    llvm::AllocaInst* alloca = this->alloca(type);
    this->builder->CreateStore(value, alloca);

    llvm::Function* function = this->builder->GetInsertBlock()->getParent();

    auto func = this->current_function;

    llvm::BasicBlock* loop = llvm::BasicBlock::Create(*this->context, "", function);
    llvm::BasicBlock* stop = llvm::BasicBlock::Create(*this->context);

    Branch* branch = func->current_branch;

    this->builder->CreateCondBr(ok, loop, stop);
    this->set_insert_point(loop, false);

    func->current_branch = func->create_branch(loop, stop);

    this->scope->variables[expr->name.value] = Variable::from_alloca(
        expr->name.value, alloca, expr->name.is_immutable, expr->name.span
    );

    expr->body->accept(*this);

    if (func->current_branch->has_jump()) {
        this->set_insert_point(stop);
        func->current_branch = branch;

        return nullptr;
    }

    tuple = this->call(next->value, {}, ref.value);

    ok = this->builder->CreateExtractValue(tuple, 1);
    value = this->builder->CreateExtractValue(tuple, 0);

    this->builder->CreateStore(value, alloca);

    this->builder->CreateCondBr(ok, loop, stop);
    this->set_insert_point(stop);

    func->current_branch = branch;
    return nullptr;
}