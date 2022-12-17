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

        elements.push_back(value.unwrap(element->start));
    }

    if (elements.size() == 0) {
        if (!this->ctx) {
            utils::note(expr->start, "The type of empty arrays defaults to [int; 0]");
             return llvm::ConstantAggregateZero::get(
                llvm::ArrayType::get(llvm::Type::getInt32Ty(*this->context), 0)
            );
        }

        if (!this->ctx->isArrayTy()) {
            ERROR(expr->start, "Expected an array type");
        }

        return llvm::ConstantAggregateZero::get(this->ctx);
    }

    // The type of the array is determined from the first element.
    llvm::Type* etype = elements[0]->getType();
    for (size_t i = 1; i < elements.size(); i++) {
        llvm::Value* element = elements[i];
        if (!this->is_compatible(etype, element->getType())) {
            ERROR(expr->start, "All elements of an array must be of the same type");
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
    llvm::Value* element = expr->element->accept(*this).unwrap(expr->element->start);
    llvm::Value* count = expr->count->accept(*this).unwrap(expr->count->start);

    if (!llvm::isa<llvm::ConstantInt>(count)) {
        ERROR(expr->count->start, "Expected a constant integer");
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
    bool is_const = false;
    Value val = expr->value->accept(*this); is_const = val.is_constant;

    llvm::Value* value = val.unwrap(expr->value->start);
    llvm::Type* type = value->getType();
    if (type->isStructTy()) {
        llvm::StringRef name = type->getStructName();
        if (!name.startswith("__tuple")) {
            ERROR(expr->value->start, "Expected a pointer, array or tuple");
        }

        llvm::Value* val = expr->index->accept(*this).unwrap(expr->index->start);
        if (!llvm::isa<llvm::ConstantInt>(val)) {
            ERROR(expr->index->start, "Tuple indices must be integer constants");
        }

        llvm::ConstantInt* index = llvm::cast<llvm::ConstantInt>(val);
        int64_t idx = index->getSExtValue();

        if (idx < 0) {
            ERROR(expr->index->start, "Tuple indices must be a positive integer");
        }

        uint32_t elements = type->getStructNumElements();
        if (idx > elements) {
            ERROR(expr->index->start, "Element index out of bounds");
        }

        if (is_const) {
            llvm::ConstantStruct* tuple = llvm::cast<llvm::ConstantStruct>(value);
            return Value(tuple->getAggregateElement(idx), true);
        }

        llvm::Value* ptr = this->builder->CreateStructGEP(type, value, idx);
        return this->builder->CreateLoad(type->getStructElementType(idx), ptr);
    }

    llvm::Value* index = expr->index->accept(*this).unwrap(expr->index->start);
    if (!index->getType()->isIntegerTy()) {
        ERROR(expr->index->start, "Indicies must be integers");
    }

    if (is_const && llvm::isa<llvm::ConstantInt>(index)) {
        int64_t idx = llvm::cast<llvm::ConstantInt>(index)->getSExtValue();
        llvm::ConstantArray* array = llvm::cast<llvm::ConstantArray>(value);

        int64_t size = array->getType()->getArrayNumElements();
        if (idx > size - 1) {
            ERROR(expr->index->start, "Element index out of bounds. Index is {0} but the array has {1} elements", idx, size);
        }

        return Value(array->getAggregateElement(idx), true);
    }

    llvm::Type* element = nullptr;
    if (type->isArrayTy()) {
        element = type->getArrayElementType();
    } else {
        element = type->getNonOpaquePointerElementType();
    }

    llvm::Value* ptr = nullptr;
    if (!type->isArrayTy()) {
        ptr = this->builder->CreateInBoundsGEP(element, value, index);
    } else {
        if (llvm::isa<llvm::ConstantInt>(index)) {
            int64_t idx = llvm::cast<llvm::ConstantInt>(index)->getSExtValue();
            int64_t size = type->getArrayNumElements();

            if (idx == size) {
                ERROR(expr->index->start, "Element index out of bounds. Index is {0} but the array has {1} elements (Indices start at 0)", idx, size);
            } else if (idx > size - 1) {
                ERROR(expr->index->start, "Element index out of bounds. Index is {0} but the array has {1} elements", idx, size);
            }
        }

        // ExtractValue doesn't take a Value so i'm kinda forced to do this.
        value = this->as_reference(expr->value.get()).value;
        ptr = this->builder->CreateInBoundsGEP(type, value, {this->builder->getInt32(0), index});
    }
    
    return this->load(ptr, element);
}

void Visitor::store_array_element(ast::ElementExpr* expr, utils::Ref<ast::Expr> value) {
    auto ref = this->as_reference(expr->value.get());
    llvm::Value* parent = ref.value;

    if (!ref.type->isPointerTy() && !ref.type->isArrayTy()) {
        ERROR(expr->value->start, "Value of type '{0}' does not support item assignment", this->get_type_name(ref.type));
    }

    if (ref.is_immutable) {
        ERROR(expr->value->start, "Cannot modify immutable value '{0}'", ref.name);
    }

    llvm::Type* type = ref.type;
    if (this->get_pointer_depth(ref.type) >= 1) {
        parent = this->load(parent, ref.type);
        type = type->getPointerElementType();
    }

    llvm::Value* index = expr->index->accept(*this).unwrap(expr->start);
    if (!index->getType()->isIntegerTy()) {
        ERROR(expr->index->start, "Indices must be integers");
    }

    llvm::Value* element = value->accept(*this).unwrap(value->start);
    llvm::Value* ptr = nullptr;

    if (ref.type->isArrayTy()) {
        if (llvm::isa<llvm::ConstantInt>(index)) {
            int64_t idx = llvm::cast<llvm::ConstantInt>(index)->getSExtValue();
            int64_t size = type->getArrayNumElements();

            if (idx == size) {
                ERROR(expr->index->start, "Element index out of bounds. Index is {0} but the array has {1} elements (Indices start at 0)", idx, size);
            } else if (idx > size - 1) {
                ERROR(expr->index->start, "Element index out of bounds. Index is {0} but the array has {1} elements", idx, size);
            }
        }

        std::vector<llvm::Value*> indicies = {this->builder->getInt32(0), index};
        ptr = this->builder->CreateGEP(type, parent, indicies);
    } else {
        ptr = this->builder->CreateGEP(type, parent, index);
    }

    this->builder->CreateStore(element, ptr);
}
