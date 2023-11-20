#include <quart/visitor.h>

using namespace quart;

Value Visitor::visit(ast::EnumExpr* expr) {
    quart::Type* inner = nullptr;
    if (!expr->type) {
        inner = this->registry->create_int_type(32, true);
    } else {
        inner = expr->type->accept(*this);
    }
    
    quart::EnumType* type = this->registry->create_enum_type(expr->name, inner);
    auto enumeration = make_ref<Enum>(expr->name, type);

    this->scope->enums[expr->name] = enumeration;
    enumeration->scope = Scope::create(expr->name, ScopeType::Enum, this->scope);

    this->push_scope(enumeration->scope);
    if (inner->is_int()) {
        i64 counter = 0;
        for (auto& field : expr->fields) {
            if (field.value) {
                Value value = field.value->accept(*this);
                if (value.is_empty_value()) ERROR(field.value->span, "Expected a constant value");

                auto constant = llvm::dyn_cast<llvm::ConstantInt>(value.inner);
                if (!constant) {
                    ERROR(field.value->span, "Expected a constant integer");
                }

                if (constant->getBitWidth() != inner->get_int_bit_width()) {
                    constant = llvm::cast<llvm::ConstantInt>(
                        llvm::ConstantInt::get(
                            inner->to_llvm_type(), 
                            constant->getSExtValue(), 
                            !inner->is_int_unsigned()
                        )
                    );
                }

                counter = constant->getSExtValue();
                enumeration->add_enumerator(field.name, constant, field.value->span);
            } else {
                llvm::Constant* constant = llvm::ConstantInt::get(inner->to_llvm_type(), counter, true);
                enumeration->add_enumerator(field.name, constant, expr->span);
            }

            counter++;
        }

        this->pop_scope();
        return nullptr;
    }

    for (auto& field : expr->fields) {
        if (!field.value) {
            ERROR(expr->span, "Expected a value");
        }

        Value value = field.value->accept(*this);
        if (value.is_empty_value()) ERROR(field.value->span, "Expected a constant value");

        auto constant = llvm::dyn_cast<llvm::Constant>(value.inner);
        if (!constant) {
            ERROR(field.value->span, "Expected a constant value");
        }

        enumeration->add_enumerator(field.name, constant, field.value->span);
    }

    this->pop_scope();
    return nullptr;
}

struct MatchBlock {
    llvm::BasicBlock* block = nullptr;
    Value result;
    Span span;
};

Value Visitor::visit(ast::MatchExpr* expr) {
    Value value = expr->value->accept(*this);
    if (value.is_empty_value()) ERROR(expr->value->span, "Expected a value");

    quart::Type* type = value.type;
    if (!type->is_int()) {
        ERROR(expr->value->span, "Expected an integer");
    }

    llvm::BasicBlock* original_block = this->builder->GetInsertBlock();
    llvm::BasicBlock* merge = llvm::BasicBlock::Create(*this->context, "");

    llvm::BasicBlock* next = nullptr;
    
    bool is_first = true;
    quart::Type* rtype = nullptr; 
    llvm::Value* alloca = nullptr;

    std::vector<MatchBlock> blocks;

    const auto& verify_arm_return = [&](
        const Value& result, llvm::BasicBlock* block, const ast::MatchArm& arm
    ) {
        if (arm.index == 0) {
            rtype = result.is_empty_value() ? result.type : nullptr;
            if (rtype) {
                this->set_insert_point(original_block, false);
                alloca = this->alloca(rtype->to_llvm_type());
            }
        }

        blocks.push_back({ block, result, arm.pattern.span });
    };

    size_t i = 0;
    for (auto& arm : expr->arms) {
        if (!arm.pattern.is_conditional) {
            if (!is_first) is_first = false;
            continue;
        }

        next = llvm::BasicBlock::Create(*this->context, "");
        llvm::BasicBlock* block = llvm::BasicBlock::Create(*this->context, "");

        auto& expr = arm.pattern.values[0];
        Value condition = expr->accept(*this);
        if (condition.is_empty_value()) ERROR(expr->span, "Expected a value");

        this->builder->CreateCondBr(condition, block, next);
        this->set_insert_point(block);

        Value result = arm.body->accept(*this);
        verify_arm_return(result, block, arm);

        this->set_insert_point(next);
        i++;
    }

    llvm::SwitchInst* instruction = this->builder->CreateSwitch(
        value, merge, expr->arms.size() - i
    );

    for (auto& arm : expr->arms) {
        if (arm.pattern.is_conditional) continue;

        llvm::BasicBlock* block = llvm::BasicBlock::Create(*this->context, "");
        if (arm.is_wildcard()) {
            instruction->setDefaultDest(block);
        } else {
            for (auto& pattern : arm.pattern.values) {
                Value value = pattern->accept(*this);
                if (!(value.flags & Value::Constant)) {
                    ERROR(pattern->span, "Expected a constant value");
                }

                auto constant = llvm::dyn_cast<llvm::ConstantInt>(value.inner);
                if (!constant) {
                    ERROR(pattern->span, "Expected a constant integer");
                }

                if (constant->getBitWidth() != type->get_int_bit_width()) {
                    constant = static_cast<llvm::ConstantInt*>(
                        llvm::ConstantInt::get(
                            this->builder->getIntNTy(type->get_int_bit_width()), 
                            constant->getSExtValue(), 
                            !type->is_int_unsigned()
                        )
                    );
                }

                if (instruction->findCaseValue(constant) != instruction->case_default()) {
                    ERROR(pattern->span, "Duplicate match arm");
                }

                instruction->addCase(constant, block);
            }
        }

        this->set_insert_point(block);
        Value result = arm.body->accept(*this);

        verify_arm_return(result, block, arm);
    }

    for (auto& block : blocks) {
        this->set_insert_point(block.block, false);

        if (!block.result && rtype) {
            ERROR(block.span, "Expected a value of type '{0}' from match arm", rtype->get_as_string());
        }

        if (block.result && rtype) {
            quart::Type* type = block.result.type;
            if (!Type::can_safely_cast_to(type, rtype)) {
                ERROR(
                    block.span, 
                    "Expected a value of type '{0}' from match arm but got '{1}' instead", 
                    rtype->get_as_string(), type->get_as_string()
                );
            }

            block.result = this->cast(block.result, rtype);
            this->builder->CreateStore(block.result, alloca);
        }

        if (!block.block->getTerminator()) {
            this->builder->CreateBr(merge);
        }
    }

    this->set_insert_point(merge);
    if (alloca) {
        return { this->load(alloca), rtype };
    }

    return EMPTY_VALUE;
}