#include "parser/ast.h"
#include "utils.h"
#include "visitor.h"
#include "llvm/ADT/StringRef.h"
#include <cstdio>

Value Visitor::visit(ast::ArrayExpr* expr) {
    std::vector<Value> elements;
    bool is_const = std::all_of(
        expr->elements.begin(), 
        expr->elements.end(), 
        [](auto& element) { return element->is_constant(); }
    );

    for (auto& element : expr->elements) {
        Value value = element->accept(*this);
        elements.push_back(value);
    }

    if (elements.size() == 0) {
        return llvm::Constant::getNullValue(llvm::ArrayType::get(llvm::Type::getInt32Ty(this->context), 0));
    }

    // The type of an array is determined from the first element.
    llvm::Type* etype = elements[0].type();
    for (size_t i = 1; i < elements.size(); i++) {
        if (elements[i].type() != etype) {
            ERROR(expr->start, "All elements of an array must be of the same type");
        }
    }

    llvm::Function* parent = this->builder->GetInsertBlock()->getParent();
    llvm::ArrayType* type = llvm::ArrayType::get(etype, elements.size());

    if (is_const) {
        std::vector<llvm::Constant*> constants;
        for (auto elem : elements) {
            constants.push_back((llvm::Constant*)elem.value);
        }
        
        return Value(llvm::ConstantArray::get(type, constants), true);
    }

    llvm::AllocaInst* array = this->create_alloca(parent, type);
    for (size_t i = 0; i < elements.size(); i++) {
        std::vector<llvm::Value*> indices = {
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(this->context), llvm::APInt(32, 0)),
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(this->context), llvm::APInt(32, i))
        };

        llvm::Value* ptr = this->builder->CreateGEP(type, array, indices);
        this->builder->CreateStore(elements[i].unwrap(expr->start), ptr);
    }

    return this->builder->CreateLoad(type, array);
}


Value Visitor::visit(ast::ElementExpr* expr) {
    llvm::Value* value = expr->value->accept(*this).unwrap(expr->value->start);
    llvm::Type* type = value->getType();

    if (!type->isPointerTy()) {
        ERROR(expr->value->start, "Expected an array or a tuple");
    }

    type = type->getNonOpaquePointerElementType();
    if (type->isStructTy()) {
        llvm::StringRef name = type->getStructName();
        if (!name.startswith("__tuple")) {
            ERROR(expr->value->start, "Expected an array or a tuple");
        }

        if (expr->index->kind() != ast::ExprKind::Integer) {
            ERROR(expr->index->start, "Tuple indices must be integer constants");
        }

        ast::IntegerExpr* index = expr->index->cast<ast::IntegerExpr>();
        if (index->value < 0) {
            ERROR(expr->index->start, "Tuple indices must be a positive integer");
        }

        uint32_t elements = type->getStructNumElements();
        if ((uint64_t)index->value > elements) {
            ERROR(expr->index->start, "Element index out of bounds");
        }

        llvm::Value* ptr = this->builder->CreateStructGEP(type, value, index->value);
        return this->builder->CreateLoad(type->getStructElementType(index->value), ptr);
    }

    llvm::Value* index = expr->index->accept(*this).unwrap(expr->index->start);
    llvm::Type* element = type;
    if (type->isArrayTy()) {
        element = type->getArrayElementType();
    }

    llvm::Value* ptr = this->builder->CreateGEP(type, value, {this->builder->getInt32(0), index});
    return this->builder->CreateLoad(element, ptr);
}

std::pair<llvm::Value*, int> Visitor::get_array_element(ast::ElementExpr* expr) {
    llvm::Value* parent = expr->value->accept(*this).unwrap(expr->start);
    llvm::Type* type = parent->getType();

    if (!type->isPointerTy()) {
        ERROR(expr->start, "Array access on non-array type");
    }

    type = type->getNonOpaquePointerElementType();
    if (!type->isAggregateType()) {
        ERROR(expr->start, "Array access on non-array type");
    }

    llvm::Value* index = expr->index->accept(*this).unwrap(expr->start);
    if (!index->getType()->isIntegerTy()) {
        ERROR(expr->index->start, "Indicies must be integers");
    }

    if (type->isArrayTy()) {
        return {this->builder->CreateGEP(
            type, parent, {this->builder->getInt32(0), index}
        ), -1
        };
    } else {
        return {this->builder->CreateGEP(type, parent, index), -1};
    }
}