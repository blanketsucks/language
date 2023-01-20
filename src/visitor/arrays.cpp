#include "visitor.h"

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

    if (elements.size() == 0) {
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

    // The type of the array is determined from the first element.
    llvm::Type* etype = elements[0]->getType();
    for (size_t i = 1; i < elements.size(); i++) {
        llvm::Value* element = elements[i];
        if (!this->is_compatible(etype, element->getType())) {
            ERROR(expr->span, "All elements of an array must be of the same type");
        } else {
            elements[i] = this->cast(element, etype);
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

    llvm::Value* array = llvm::ConstantAggregateZero::get(type);
    for (uint32_t i = 0; i < size; i++) {
        array = this->builder->CreateInsertValue(array, element, {0, i});
    }

    return array;
}

Value Visitor::visit(ast::ElementExpr* expr) {
    auto ref = this->as_reference(expr->value);
    if (ref.is_null()) {
        llvm::Value* value = expr->value->accept(*this).unwrap(expr->value->span);
        if (!value->getType()->isPointerTy()) {
            ERROR(expr->value->span, "Value of type '{0}' does not support indexing", this->get_type_name(value->getType()));
        }

        ref.value = value;
        ref.type = value->getType();
    } else {
        if (this->get_pointer_depth(ref.type) >= 1) {
            ref.value = this->load(ref.value);
        }
    }

    if (ref.type->isStructTy() && !this->is_tuple(ref.type)) {
        auto structure = this->get_struct(ref.type);
        if (!structure->has_method("getitem")) {
            ERROR(expr->value->span, "Value of type '{0}' does not support indexing", this->get_type_name(ref.type));
        }

        auto method = structure->get_method("getitem");
        if (!method->is_operator) {
            ERROR(expr->value->span, "Value of type '{0}' does not support indexing", this->get_type_name(ref.type));
        }

        if (method->argc() != 2) { // 2 with self
            ERROR(expr->value->span, "Method 'getitem' of type '{0}' must take exactly one argument", this->get_type_name(ref.type));
        }

        return this->call(method->value, { expr->index->accept(*this).unwrap(expr->index->span) }, ref.value);
    }

    if (ref.type->isStructTy()) {
        llvm::StringRef name = ref.type->getStructName();
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

        uint32_t elements = ref.type->getStructNumElements();
        if (index > elements) {
            ERROR(expr->index->span, "Element index out of bounds");
        }

        if (ref.is_constant) {
            llvm::ConstantStruct* tuple = llvm::cast<llvm::ConstantStruct>(ref.get_constant_value());
            return Value(tuple->getAggregateElement(index), true);
        }

        return this->load(this->builder->CreateStructGEP(ref.type, ref.value, index));
    }

    llvm::Value* index = expr->index->accept(*this).unwrap(expr->index->span);
    if (!index->getType()->isIntegerTy()) {
        ERROR(expr->index->span, "Indicies must be integers");
    }

    if (ref.is_constant && llvm::isa<llvm::ConstantInt>(index)) {
        int64_t idx = llvm::cast<llvm::ConstantInt>(index)->getSExtValue();
        llvm::ConstantArray* array = llvm::cast<llvm::ConstantArray>(ref.get_constant_value());

        int64_t size = array->getType()->getArrayNumElements();
        if (idx > size - 1) {
            ERROR(expr->index->span, "Element index out of bounds. Index is {0} but the array has {1} elements", idx, size);
        }

        return Value(array->getAggregateElement(idx), true);
    }

    llvm::Value* ptr = nullptr;
    if (!ref.type->isArrayTy()) {
        ptr = this->builder->CreateGEP(
            ref.type->getPointerElementType(), ref.value, index
        );
    } else {
        if (llvm::isa<llvm::ConstantInt>(index)) {
            int64_t idx = llvm::cast<llvm::ConstantInt>(index)->getSExtValue();
            int64_t size = ref.type->getArrayNumElements();

            if (idx == size) {
                ERROR(expr->index->span, "Element index out of bounds. Index is {0} but the array has {1} elements (Indices start at 0)", idx, size);
            } else if (idx > size - 1) {
                ERROR(expr->index->span, "Element index out of bounds. Index is {0} but the array has {1} elements", idx, size);
            }
        } else {
            this->create_bounds_check(index, ref.type->getArrayNumElements(), expr->index->span);
        }

        ptr = this->builder->CreateGEP(
            ref.type, ref.value, {this->builder->getInt32(0), index}
        );
    }
    
    return this->load(ptr);
}

void Visitor::create_bounds_check(llvm::Value* index, uint32_t count, Span span) {
    // TODO: Disable in release mode
    llvm::BasicBlock* merge = this->create_if_statement(
        this->builder->CreateICmpSGT(index, this->builder->getInt32(count - 1))
    );

    this->panic("Index out of bounds.", span);
    this->set_insert_point(merge);
}

void Visitor::store_array_element(ast::ElementExpr* expr, utils::Scope<ast::Expr> value) {
    auto ref = this->as_reference(expr->value);
    llvm::Value* parent = ref.value;

    if (!ref.type->isPointerTy() && !ref.type->isArrayTy()) {
        ERROR(expr->value->span, "Value of type '{0}' does not support item assignment", this->get_type_name(ref.type));
    }

    if (ref.is_immutable) {
        ERROR(expr->value->span, "Cannot modify immutable value '{0}'", ref.name);
    }

    llvm::Type* type = ref.type;
    if (this->get_pointer_depth(ref.type) >= 1) {
        parent = this->load(parent, ref.type);
    }

    llvm::Value* index = expr->index->accept(*this).unwrap(expr->span);
    if (!index->getType()->isIntegerTy()) {
        ERROR(expr->index->span, "Indices must be integers");
    }

    llvm::Value* element = value->accept(*this).unwrap(value->span);
    llvm::Type* ty = type->isArrayTy() ? type->getArrayElementType() : type->getPointerElementType();

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
        if (this->get_pointer_depth(type) <= 1) {
            type = type->getPointerElementType();
        }

        ptr = this->builder->CreateGEP(type, parent, index);
    }

    this->builder->CreateStore(element, ptr);
}
