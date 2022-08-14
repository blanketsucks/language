#include "visitor.h"

bool Visitor::is_iterable(llvm::Value* value) {
    llvm::Type* type = value->getType();
    if (!type->isPointerTy()) {
        return false;
    }

    llvm::Type* elementType = type->getPointerElementType();
    if (!elementType->isArrayTy()) {
        return false;
    }

    if (elementType->isStructTy()) {
        std::string name = elementType->getStructName().str();
        Struct* structure = this->structs[name];

        return structure->has_method("next");
    }

    return true;
}

Value Visitor::visit(ast::WhileExpr* expr) {
    llvm::Value* condition = expr->condition->accept(*this).unwrap(this, expr->condition->start);
    llvm::Function* function = this->builder->GetInsertBlock()->getParent();
    Function* func = this->current_function;

    llvm::BasicBlock* loop = llvm::BasicBlock::Create(this->context, "", function);
    llvm::BasicBlock* end = llvm::BasicBlock::Create(this->context);

    Branch* branch = func->branch;
    this->builder->CreateCondBr(condition, loop, end);

    function->getBasicBlockList().push_back(loop);
    this->builder->SetInsertPoint(loop);

    func->branch = func->create_branch("while.loop");
    expr->body->accept(*this);

    if (func->branch->has_return) {
        function->getBasicBlockList().push_back(end);
        this->builder->SetInsertPoint(end);

        func->branch = branch;
        return this->constants["null"];
    }

    loop = this->builder->GetInsertBlock();

    condition = expr->condition->accept(*this).unwrap(this, expr->condition->start);
    this->builder->CreateCondBr(condition, loop, end);

    function->getBasicBlockList().push_back(end);
    this->builder->SetInsertPoint(end);

    func->branch = branch;
    return this->constants["null"];
}

Value Visitor::visit(ast::ForExpr* expr) {
    llvm::Function* function = this->builder->GetInsertBlock()->getParent();
    Function* func = this->current_function;

    llvm::Value* iterator = expr->iterator->accept(*this).unwrap(this, expr->iterator->start);

    Struct* structure = nullptr;
    size_t elements = SIZE_MAX;
    
    if (!iterator->getType()->isPointerTy()) {
        Type* type = Type::from_llvm_type(iterator->getType());
        ERROR(expr->iterator->start, "Object of type '{s}' is not iterable", type->name());
    }

    llvm::Type* element = iterator->getType()->getPointerElementType();
    llvm::Type* alloca_type = nullptr;

    if (element->isStructTy()) {
        std::string name = element->getStructName().str();
        structure = this->structs[name];

        if (!structure->has_method("iter") && !structure->has_method("next")) {
            std::string fmt = utils::fmt::format("Object of type '{s}' is not iterable", name);
            utils::error(expr->iterator->start, fmt, false);

            NOTE(expr->iterator->start, "Structures must implement either a `iter` or `next` method.");
            exit(1);
        }

        if (structure->has_method("iter")) {
            Function* method = structure->methods["iter"];
            method->used = true;

            iterator = this->builder->CreateCall(method->value, {iterator});

            element = iterator->getType()->getPointerElementType();
            if (!this->is_iterable(iterator)) {
                ERROR(expr->iterator->start, "Object of type '{s}' is not iterable", name);
            }

            if (element->isStructTy()) {
                std::string name = element->getStructName().str();
                structure = this->structs[name];

                Function* next = structure->methods["next"];
                alloca_type = next->value->getFunctionType()->getReturnType();

                next->used = true;
            } else {
                alloca_type = element->getArrayElementType();
            }
        } else {
            Function* next = structure->methods["next"];
            alloca_type = next->value->getFunctionType()->getReturnType();

            next->used = true;
        } 
    } else if (element->isArrayTy()) {
        elements = element->getArrayNumElements();
        alloca_type = element->getArrayElementType();
    } else {
        Type* type = Type::from_llvm_type(iterator->getType());
        ERROR(expr->iterator->start, "Object of type '{s}' is not iterable", type->name());
    }

    llvm::AllocaInst* inst = this->builder->CreateAlloca(alloca_type);

    llvm::BasicBlock* loop = llvm::BasicBlock::Create(this->context, "", function);
    llvm::BasicBlock* end = llvm::BasicBlock::Create(this->context);

    Branch* branch = func->branch;

    func->branch = func->create_branch("for.loop");
    func->locals[expr->name] = inst;

    llvm::Value* value = nullptr;
    llvm::AllocaInst* index = nullptr;

    if (structure) {
        Function* method = structure->methods["next"];
        value = this->builder->CreateCall(method->value, {iterator});

        llvm::Value* is_null = this->builder->CreateIsNull(value);
        this->builder->CreateStore(value, inst);

        this->builder->CreateCondBr(is_null, end, loop);
    } else {
        llvm::Value* elm_count = this->builder->getInt32(elements);
        llvm::Value* cond = this->builder->CreateICmpEQ(elm_count, this->builder->getInt32(0));

        index = this->create_alloca(function, this->builder->getInt32Ty());
        this->builder->CreateStore(this->builder->getInt32(0), index);

        llvm::Value* pointer = this->builder->CreateGEP(element, iterator, this->builder->CreateLoad(this->builder->getInt32Ty(), index));
        value = this->builder->CreateLoad(alloca_type, pointer);

        this->builder->CreateStore(value, inst);
        this->builder->CreateCondBr(cond, end, loop);
    }

    this->builder->SetInsertPoint(loop);
    expr->body->accept(*this);

    if (func->branch->has_return) {
        function->getBasicBlockList().push_back(end);
        this->builder->SetInsertPoint(end);

        func->branch = branch;
        return this->constants["null"];
    }

    if (structure) {
        Function* method = structure->methods["next"];

        value = this->builder->CreateCall(method->value, {iterator});
        this->builder->CreateStore(value, inst);

        llvm::Value* is_null = this->builder->CreateIsNull(value);
        this->builder->CreateCondBr(is_null, end, loop);
    } else {
        // Increment the `index` by 1
        llvm::Value* index_value = this->builder->CreateLoad(this->builder->getInt32Ty(), index);
        llvm::Value* next_index = this->builder->CreateAdd(index_value, this->builder->getInt32(1));

        this->builder->CreateStore(next_index, index);

        // Get the next element in the array
        llvm::Value* pointer = this->builder->CreateGEP(element, iterator, this->builder->CreateLoad(this->builder->getInt32Ty(), index));
        value = this->builder->CreateLoad(alloca_type, pointer);

        this->builder->CreateStore(value, inst);

        llvm::Value* cond = this->builder->CreateICmpEQ(next_index, this->builder->getInt32(elements));
        this->builder->CreateCondBr(cond, end, loop);
    }

    function->getBasicBlockList().push_back(end);   
    this->builder->SetInsertPoint(end);

    func->branch = branch;
    return nullptr; 
}