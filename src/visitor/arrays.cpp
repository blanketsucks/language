#include <quart/visitor.h>

using namespace quart;

Value Visitor::visit(ast::ArrayExpr* expr) {
    std::vector<Value> elements;
    bool is_const = true;

    for (auto& element : expr->elements) {
        if (!element) {
            continue;
        }

        Value value = element->accept(*this);
        if (value.is_empty_value()) ERROR(element->span, "Expected a value");

        elements.push_back(value);
    }

    if (elements.empty()) {
        if (!this->inferred) {
            logging::note(expr->span, "The type of empty arrays defaults to [int; 0]");
             return llvm::ConstantAggregateZero::get(
                llvm::ArrayType::get(llvm::Type::getInt32Ty(*this->context), 0)
            );
        }

        if (!this->inferred->is_array()) {
            ERROR(expr->span, "Expected an array type");
        }

        return {
            llvm::ConstantAggregateZero::get(this->inferred->to_llvm_type()),
            this->inferred
        };
    }

    quart::Type* etype = elements[0].type;
    for (size_t i = 1; i < elements.size(); i++) {
        auto& element = elements[i];
        if (!Type::can_safely_cast_to(element.type, etype)) {
            ERROR(
                expr->span, 
                "All elements of an array must be of the same type (element {0} is of type '{1}' but the first element is of type '{2}')", 
                i, element.type->get_as_string(), etype->get_as_string()
            );
        } else {
            element = this->cast(element, etype);
        }
    }

    quart::ArrayType* type = this->registry->create_array_type(etype, elements.size());
    llvm::Type* atype = type->to_llvm_type();

    if (is_const) {
        std::vector<llvm::Constant*> constants;
        for (auto element : elements) {
            constants.push_back(llvm::cast<llvm::Constant>(element.inner));
        }
        
        llvm::Constant* array = llvm::ConstantArray::get(
            llvm::cast<llvm::ArrayType>(atype), constants
        );

        return Value(array, type, Value::Constant);
    }

    llvm::Value* array = llvm::ConstantAggregateZero::get(atype);
    for (uint32_t i = 0; i < elements.size(); i++) {
        array = this->builder->CreateInsertValue(array, elements[i], {0, i});
    }

    return {array, type};
}

Value Visitor::visit(ast::ArrayFillExpr* expr) {
    Value element = expr->element->accept(*this);
    if (element.is_empty_value()) ERROR(expr->element->span, "Expected a value");

    Value count = expr->count->accept(*this);
    if (count.is_empty_value()) ERROR(expr->count->span, "Expected a value");


    llvm::ConstantInt* size = llvm::dyn_cast<llvm::ConstantInt>(count.inner);
    if (!size) ERROR(expr->count->span, "Expected a constant integer");

    quart::ArrayType* type = this->registry->create_array_type(element.type, size->getSExtValue());
    llvm::ArrayType* atype = llvm::cast<llvm::ArrayType>(type->to_llvm_type());

    if (llvm::isa<llvm::Constant>(element.inner)) {
        return {llvm::ConstantArray::get(
            atype,
            std::vector<llvm::Constant*>(size->getSExtValue(), llvm::cast<llvm::Constant>(element.inner))
        ), type, Value::Constant};
    }

    llvm::Value* alloca = this->alloca(atype);
    for (uint32_t i = 0; i < size->getZExtValue(); i++) {
        llvm::Value* ptr = this->builder->CreateGEP(
            atype, alloca, {this->builder->getInt32(0), this->builder->getInt32(i)}
        );
        
        this->builder->CreateStore(element, ptr);
    }

    return Value(alloca, type, Value::Aggregate);
}

Value Visitor::visit(ast::IndexExpr* expr) {
    auto ref = this->as_reference(expr->value);

    quart::Type* type = ref.type;
    llvm::Value* value = ref.value;
    bool is_constant = ref.is_constant();

    quart::Type* element_type = type;
    if (ref.is_null()) {
        Value array = expr->value->accept(*this);
        if (array.is_empty_value()) ERROR(expr->value->span, "Expected a value");

        if (!array.type->is_pointer()) {
            ERROR(expr->value->span, "Value of type '{0}' does not support indexing", array.type->get_as_string());
        }

        type = array.type;
        value = array.inner;

        element_type = type->get_pointee_type();
    } else {
        if (type->get_pointer_depth() > 1) {
            value = this->load(value);
            type = type->get_pointee_type();
        }

        element_type = type;
    }

    if (type->is_tuple()) {
        Value index = expr->index->accept(*this);
        if (index.is_empty_value()) ERROR(expr->index->span, "Expected a value");

        llvm::ConstantInt* idx = llvm::dyn_cast<llvm::ConstantInt>(index.inner);
        if (!idx) ERROR(expr->index->span, "Tuple indices must be integer constants");

        size_t n = type->get_tuple_size();
        if (idx->getZExtValue() > n - 1) {
            ERROR(expr->index->span, "Tuple index out of bounds. Index is {0} but the tuple has {1} elements", idx->getSExtValue(), n);
        }

        if (is_constant) {
            llvm::ConstantStruct* tuple = llvm::cast<llvm::ConstantStruct>(ref.get_constant_value());
            return Value(
                tuple->getAggregateElement(idx->getSExtValue()),
                Value::Constant,
                type->get_tuple_element(idx->getSExtValue())
            );
        }

        return {
            this->load(this->builder->CreateStructGEP(
                type->to_llvm_type(), value, idx->getSExtValue()
            )),
            type->get_tuple_element(idx->getSExtValue())
        };
    }

    Value index = expr->index->accept(*this);

    if (index.is_empty_value()) ERROR(expr->index->span, "Expected a value");
    if (!index.type->is_int()) ERROR(expr->index->span, "Indicies must be integers");

    if (is_constant && llvm::isa<llvm::ConstantInt>(index.inner)) {
        int64_t idx = llvm::cast<llvm::ConstantInt>(index.inner)->getSExtValue();
        llvm::ConstantArray* array = llvm::cast<llvm::ConstantArray>(ref.get_constant_value());

        int64_t size = array->getType()->getArrayNumElements();
        if (idx > size - 1) {
            ERROR(expr->index->span, "Element index out of bounds. Index is {0} but the array has {1} elements", idx, size);
        }

        return Value(
            array->getAggregateElement(idx), 
            type->get_array_element_type(), 
            Value::Constant
        );
    }

    llvm::Value* ptr = nullptr;
    quart::Type* result = element_type;

    if (!type->is_array()) {
        ptr = this->builder->CreateGEP(element_type->to_llvm_type(), value, index);
    } else {
        if (llvm::isa<llvm::ConstantInt>(index.inner)) {
            int64_t idx = llvm::cast<llvm::ConstantInt>(index.inner)->getSExtValue();
            int64_t size = type->get_array_size();

            if (idx == size) {
                ERROR(expr->index->span, "Element index out of bounds. Index is {0} but the array has {1} elements (Indices start at 0)", idx, size);
            } else if (idx > size - 1) {
                ERROR(expr->index->span, "Element index out of bounds. Index is {0} but the array has {1} elements", idx, size);
            }
        }

        ptr = this->builder->CreateGEP(
            type->to_llvm_type(), value, {this->builder->getInt32(0), index}
        );

        result = type->get_array_element_type();
    }
    
    return {this->load(ptr), result};
}

void Visitor::create_bounds_check(llvm::Value* index, uint32_t count, const Span& span) {
    if (this->options.standalone || this->options.optimization == OptimizationLevel::Release) {
        return;
    }

    llvm::BasicBlock* merge = this->create_if_statement(
        this->builder->CreateICmpSGT(index, this->builder->getInt32(count - 1))
    );

    this->panic("Index out of bounds.", span);
    this->set_insert_point(merge);
}

Value Visitor::store_array_element(ast::IndexExpr* expr, std::unique_ptr<ast::Expr> value) {
    auto ref = this->as_reference(expr->value);
    quart::Type* type = ref.type;

    if (!type->is_pointer() && !type->is_array()) {
        ERROR(expr->value->span, "Value of type '{0}' does not support item assignment", type->get_as_string());
    }

    if (!ref.is_mutable()) {
        ERROR(expr->value->span, "Cannot mutate immutable value '{0}'", ref.name);
    }

    if (type->get_pointer_depth() >= 1) {
        ref.value = this->load(ref.value);
        type = type->get_pointee_type();
    }

    Value index = expr->index->accept(*this);

    if (index.is_empty_value()) ERROR(expr->index->span, "Expected a value");
    if (!index.type->is_int()) ERROR(expr->index->span, "Indicies must be integers");

    quart::Type* expected = type->is_array() ? type->get_array_element_type() : type;
    this->inferred = expected;

    Value element = value->accept(*this);
    if (element.is_empty_value()) ERROR(value->span, "Expected a value");

    this->inferred = nullptr;
    if (element.flags & Value::Aggregate) {
        element = {this->load(element), element.type};
    }

    if (!Type::can_safely_cast_to(element.type, expected)) {
        ERROR(
            value->span, 
            "Cannot assign value of type '{0}' to {1} of type '{2}'", 
            element.type->get_as_string(),
            type->is_array() ? "an array" : "a pointer",
            expected->get_as_string()
        );
    } else {
        element = this->cast(element, expected);
    }

    llvm::Value* ptr = nullptr;
    llvm::Type* ltype = type->to_llvm_type();

    if (ref.type->is_array()) {
        if (llvm::isa<llvm::ConstantInt>(index.inner)) {
            int64_t idx = llvm::cast<llvm::ConstantInt>(index.inner)->getSExtValue();
            int64_t size = type->get_array_size();

            if (idx == size) {
                ERROR(expr->index->span, "Element index out of bounds. Index is {0} but the array has {1} elements (Indices start at 0)", idx, size);
            } else if (idx > size - 1) {
                ERROR(expr->index->span, "Element index out of bounds. Index is {0} but the array has {1} elements", idx, size);
            }
        }

        std::vector<llvm::Value*> indicies = {this->builder->getInt32(0), index};
        ptr = this->builder->CreateGEP(ltype, ref.value, indicies);
    } else {
        ptr = this->builder->CreateGEP(ltype, ref.value, index);
    }

    this->mark_as_mutated(ref);
    this->builder->CreateStore(element, ptr);

    return element;
}
