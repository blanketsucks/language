#include <quart/visitor.h>

Value Visitor::visit(ast::EnumExpr* expr) {
    llvm::Type* type = nullptr;
    if (!expr->type) {
        type = this->builder->getInt32Ty();
    } else {
        type = expr->type->accept(*this).value;
    }
    
    auto enumeration = utils::make_ref<Enum>(expr->name, type);
    enumeration->span = expr->span;

    this->scope->enums[expr->name] = enumeration;
    enumeration->scope = this->create_scope(expr->name, ScopeType::Enum);

    if (type->isIntegerTy()) {
        int64_t counter = 0;
        for (auto& field : expr->fields) {
            llvm::Constant* constant = nullptr;
            if (field.value) {
                llvm::Value* value = field.value->accept(*this).unwrap(field.value->span);
                if (!llvm::isa<llvm::ConstantInt>(value)) {
                    ERROR(field.value->span, "Expected a constant integer");
                }

                llvm::ConstantInt* val = llvm::cast<llvm::ConstantInt>(value);
                if (val->getBitWidth() > type->getIntegerBitWidth()) {
                    val = static_cast<llvm::ConstantInt*>(
                        llvm::ConstantInt::get(type, val->getSExtValue(), true)
                    );
                }

                counter = val->getSExtValue();
                constant = val;
            } else {
                constant = llvm::ConstantInt::get(type, counter, true);
            }

            enumeration->add_field(field.name, constant);
            counter++;
        }

        this->scope->exit(this);
        return nullptr;
    }

    for (auto& field : expr->fields) {
        if (!field.value) {
            ERROR(expr->span, "Expected a value");
        }

        Value value = field.value->accept(*this);
        if (!value.is_constant) {
            ERROR(field.value->span, "Expected a constant value");
        }

        llvm::Constant* constant = llvm::cast<llvm::Constant>(value.unwrap(field.value->span));
        enumeration->add_field(field.name, constant);
    }

    this->scope->exit(this);
    return nullptr;
}

struct MatchBlock {
    llvm::BasicBlock* block;
    llvm::Value* result;
    Span span;
};

Value Visitor::visit(ast::MatchExpr* expr) {
    llvm::Value* value = expr->value->accept(*this).unwrap(expr->value->span);
    llvm::Type* type = value->getType();

    if (!type->isIntegerTy()) {
        ERROR(expr->value->span, "Expected an integer");
    }

    llvm::BasicBlock* original_block = this->builder->GetInsertBlock();
    llvm::BasicBlock* merge = llvm::BasicBlock::Create(*this->context, "");

    llvm::SwitchInst* instruction = this->builder->CreateSwitch(
        value, merge, expr->arms.size()
    );

    llvm::Type* rtype = nullptr; 
    llvm::Value* alloca = nullptr;

    std::vector<MatchBlock> blocks;
    bool is_first = true;

    for (auto& arm : expr->arms) {
        llvm::BasicBlock* block = llvm::BasicBlock::Create(*this->context, "");

        for (auto& pattern : arm.pattern.values) {
            Value value = pattern->accept(*this);
            if (!value.is_constant) {
                ERROR(pattern->span, "Expected a constant value");
            }

            llvm::Value* v = value.unwrap(pattern->span);
            if (!llvm::isa<llvm::ConstantInt>(v)) {
                ERROR(pattern->span, "Expected a constant integer");
            }

            llvm::ConstantInt* constant = llvm::cast<llvm::ConstantInt>(v);
            if (constant->getBitWidth() != type->getIntegerBitWidth()) {
                constant = static_cast<llvm::ConstantInt*>(
                    llvm::ConstantInt::get(type, constant->getSExtValue(), true)
                );
            }

            instruction->addCase(constant, block);
        }

        this->set_insert_point(block);
        llvm::Value* result = arm.body->accept(*this).value;

        if (is_first) {
            rtype = result ? result->getType() : nullptr;
            if (rtype) {
                this->set_insert_point(original_block, false);
                alloca = this->alloca(rtype);
            }
        }

        blocks.push_back({ block, result, arm.pattern.span() });
        is_first = false;
    }

    for (auto& block : blocks) {
        this->set_insert_point(block.block, false);

        if (!block.result && rtype) {
            ERROR(block.span, "Expected a value of type '{0}' from match arm", this->get_type_name(rtype));
        }

        if (block.result && rtype) {
            llvm::Type* type = block.result->getType();
            if (!this->is_compatible(rtype, type)) {
                ERROR(block.span, "Expected a value of type '{0}' from match arm but got '{1}' instead", this->get_type_name(rtype), this->get_type_name(type));
            }

            block.result = this->cast(block.result, rtype);
            this->builder->CreateStore(block.result, alloca);
        }

        if (!block.block->getTerminator()) {
            this->builder->CreateBr(merge);
        }
    }

    this->set_insert_point(merge);
    return this->load(alloca);
}