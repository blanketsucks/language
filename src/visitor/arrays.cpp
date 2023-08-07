#include <quart/visitor.h>

Value Visitor::visit(ast::ArrayExpr* expr) {
    std::vector<llvm::Value*> elements;
    bool is_const = true;

    for (auto& element : expr->elements) {
        if (!element) {
            continue;
        }

        Value value = element->accept(*this);
        is_const &= value.is_constant;

        elements.push_back(value.unwrap(element->span));
    }

    if (elements.empty()) {
        if (!this->ctx) {
            utils::note(expr->span, "The type of empty arrays defaults to [int; 0]");
             return llvm::ConstantAggregateZero::get(
                llvm::ArrayType::get(llvm::Type::getInt32Ty(*this->context), 0)
            );
        }

        if (!this->ctx->isArrayTy()) {
            ERROR(expr->span, "Expected an array type");
        }

        return llvm::ConstantAggregateZero::get(this->ctx);
    }

    llvm::Type* etype = elements[0]->getType();
    for (size_t i = 1; i < elements.size(); i++) {
        auto& element = elements[i];
        if (!this->is_compatible(etype, element->getType())) {
            ERROR(expr->span, "All elements of an array must be of the same type");
        } else {
            element = this->cast(element, etype);
        }
    }

    llvm::ArrayType* type = llvm::ArrayType::get(etype, elements.size());
    if (is_const) {
        std::vector<llvm::Constant*> constants;
        for (auto element : elements) {
            constants.push_back(llvm::cast<llvm::Constant>(element));
        }
        
        return Value(llvm::ConstantArray::get(type, constants), true);
    }

    llvm::Value* array = llvm::ConstantAggregateZero::get(type);
    for (uint32_t i = 0; i < elements.size(); i++) {
        array = this->builder->CreateInsertValue(array, elements[i], {0, i});
    }

    return array;
}

Value Visitor::visit(ast::ArrayFillExpr* expr) {
    llvm::Value* element = expr->element->accept(*this).unwrap(expr->element->span);
    llvm::Value* count = expr->count->accept(*this).unwrap(expr->count->span);

    if (!llvm::isa<llvm::ConstantInt>(count)) {
        ERROR(expr->count->span, "Expected a constant integer");
    }

    uint32_t size = llvm::cast<llvm::ConstantInt>(count)->getZExtValue();
    llvm::ArrayType* type = llvm::ArrayType::get(element->getType(), size);

    if (llvm::isa<llvm::Constant>(element)) {
        return llvm::ConstantArray::get(type, std::vector<llvm::Constant*>(size, llvm::cast<llvm::Constant>(element)));
    }

    llvm::Value* alloca = this->alloca(type);
    for (uint32_t i = 0; i < size; i++) {
        llvm::Value* ptr = this->builder->CreateGEP(
            type, alloca, {this->builder->getInt32(0), this->builder->getInt32(i)}
        );
        
        this->builder->CreateStore(element, ptr);
    }

    return Value::as_aggregate(alloca);
}

Value Visitor::visit(ast::ElementExpr* expr) {
    auto ref = this->as_reference(expr->value);

    llvm::Type* type = ref.type;
    llvm::Value* value = ref.value;
    bool is_const = ref.is_constant;

    llvm::Type* elem = type;
    if (ref.is_null()) {
        value = expr->value->accept(*this).unwrap(expr->value->span);
        if (!value->getType()->isPointerTy()) {
            ERROR(expr->value->span, "Value of type '{0}' does not support indexing", this->get_type_name(value->getType()));
        }

        type = value->getType();
        elem = type->getPointerElementType();
    } else {
        if (this->get_pointer_depth(type) > 1) {
            value = this->load(value);
            type = type->getPointerElementType();
        }

        elem = type;
    }

    if (type->isStructTy()) {
        llvm::StringRef name = type->getStructName();
        if (!name.startswith("__tuple")) {
            ERROR(expr->value->span, "Expected a pointer, array or tuple");
        }

        llvm::Value* val = expr->index->accept(*this).unwrap(expr->index->span);
        if (!llvm::isa<llvm::ConstantInt>(val)) {
            ERROR(expr->index->span, "Tuple indices must be integer constants");
        }

        int64_t index = llvm::cast<llvm::ConstantInt>(val)->getSExtValue();
        if (index < 0) {
            ERROR(expr->index->span, "Tuple indices must be a positive integer");
        }

        uint32_t elements = type->getStructNumElements();
        if (index > elements) {
            ERROR(expr->index->span, "Element index out of bounds");
        }

        if (is_const) {
            llvm::ConstantStruct* tuple = llvm::cast<llvm::ConstantStruct>(ref.get_constant_value());
            return Value(tuple->getAggregateElement(index), true);
        }

        return this->load(this->builder->CreateStructGEP(type, value, index));
    }

    llvm::Value* index = expr->index->accept(*this).unwrap(expr->index->span);
    if (!index->getType()->isIntegerTy()) {
        ERROR(expr->index->span, "Indicies must be integers");
    }

    if (is_const && llvm::isa<llvm::ConstantInt>(index)) {
        int64_t idx = llvm::cast<llvm::ConstantInt>(index)->getSExtValue();
        llvm::ConstantArray* array = llvm::cast<llvm::ConstantArray>(ref.get_constant_value());

        int64_t size = array->getType()->getArrayNumElements();
        if (idx > size - 1) {
            ERROR(expr->index->span, "Element index out of bounds. Index is {0} but the array has {1} elements", idx, size);
        }

        return Value(array->getAggregateElement(idx), true);
    }

    llvm::Value* ptr = nullptr;
    if (!type->isArrayTy()) {
        ptr = this->builder->CreateGEP(elem, value, index);
    } else {
        if (llvm::isa<llvm::ConstantInt>(index)) {
            int64_t idx = llvm::cast<llvm::ConstantInt>(index)->getSExtValue();
            int64_t size = type->getArrayNumElements();

            if (idx == size) {
                ERROR(expr->index->span, "Element index out of bounds. Index is {0} but the array has {1} elements (Indices start at 0)", idx, size);
            } else if (idx > size - 1) {
                ERROR(expr->index->span, "Element index out of bounds. Index is {0} but the array has {1} elements", idx, size);
            }
        } else {
            this->create_bounds_check(index, type->getArrayNumElements(), expr->index->span);
        }

        ptr = this->builder->CreateGEP(
            type, value, {this->builder->getInt32(0), index}
        );
    }
    
    return this->load(ptr);
}

void Visitor::create_bounds_check(llvm::Value* index, uint32_t count, Span span) {
    if (this->options.standalone || this->options.optimization == OptimizationLevel::Release) {
        return;
    }

    llvm::BasicBlock* merge = this->create_if_statement(
        this->builder->CreateICmpSGT(index, this->builder->getInt32(count - 1))
    );

    this->panic("Index out of bounds.", span);
    this->set_insert_point(merge);
}

void Visitor::store_array_element(ast::ElementExpr* expr, utils::Scope<ast::Expr> value) {
    auto ref = this->as_reference(expr->value);

    llvm::Value* parent = ref.value;
    llvm::Type* type = ref.type;

    if (!type->isPointerTy() && !type->isArrayTy()) {
        ERROR(expr->value->span, "Value of type '{0}' does not support item assignment", this->get_type_name(type));
    }

    if (ref.is_immutable) {
        ERROR(expr->value->span, "Cannot mutate immutable value '{0}'", ref.name);
    }

    if (this->get_pointer_depth(type) >= 1) {
        parent = this->load(parent);
        type = type->getPointerElementType();
    }

    llvm::Value* index = expr->index->accept(*this).unwrap(expr->span);
    if (!index->getType()->isIntegerTy()) {
        ERROR(expr->index->span, "Indices must be integers");
    }

    llvm::Type* ty = type->isArrayTy() ? type->getArrayElementType() : type;
    this->ctx = ty;

    Value val = value->accept(*this);
    llvm::Value* element = val.unwrap(value->span);

    if (val.is_aggregate) {
        element = this->load(element);
    }

    // llvm::Value* element = value->accept(*this).unwrap(value->span);
    if (!this->is_compatible(ty, element->getType())) {
        ERROR(
            value->span, 
            "Cannot assign value of type '{0}' to {1} of type '{2}'", 
            this->get_type_name(element->getType()),
            type->isArrayTy() ? "an array" : "a pointer",
            this->get_type_name(type)
        );
    } else {
        element = this->cast(element, ty);
    }

    this->ctx = nullptr;
    llvm::Value* ptr = nullptr;
    
    if (ref.type->isArrayTy()) {
        if (llvm::isa<llvm::ConstantInt>(index)) {
            int64_t idx = llvm::cast<llvm::ConstantInt>(index)->getSExtValue();
            int64_t size = type->getArrayNumElements();

            if (idx == size) {
                ERROR(expr->index->span, "Element index out of bounds. Index is {0} but the array has {1} elements (Indices start at 0)", idx, size);
            } else if (idx > size - 1) {
                ERROR(expr->index->span, "Element index out of bounds. Index is {0} but the array has {1} elements", idx, size);
            }
        } else {
            this->create_bounds_check(index, type->getArrayNumElements(), expr->index->span);
        }

        std::vector<llvm::Value*> indicies = {this->builder->getInt32(0), index};
        ptr = this->builder->CreateGEP(type, parent, indicies);
    } else {
        // if (this->get_pointer_depth(type) <= 1) {
        //     type = type->getPointerElementType();
        // }

        ptr = this->builder->CreateGEP(type, parent, index);
    }

    this->mark_as_mutated(ref);
    this->builder->CreateStore(element, ptr);
}
