#include "visitor.h"

Value Visitor::visit(ast::TupleExpr* expr) {
    std::vector<Type*> types;
    std::vector<llvm::Value*> elements;

    bool is_const = true;
    for (auto& elem : expr->elements) {
        Value val = elem->accept(*this);
        is_const &= val.is_constant;

        llvm::Value* value = val.unwrap(elem->start);

        elements.push_back(value);
        types.push_back(Type::from_llvm_type(value->getType()));
    }

    uint32_t hash = TupleType::getHashFromTypes(types);
    llvm::StructType* type = nullptr;
    if (this->tuples.find(hash) != this->tuples.end()) {
        type = this->tuples[hash];
    } else {
        TupleType* tuple = TupleType::create(types);
        type = tuple->to_llvm_type(*this->context);

        this->tuples[hash] = type;
    }
    
    if (is_const) {
        std::vector<llvm::Constant*> constants;
        for (auto& element : elements) {
            constants.push_back(llvm::cast<llvm::Constant>(element));
        }

        return Value(llvm::ConstantStruct::get(type, constants), true);
    }

    if (!this->current_function) {
        utils::error(expr->start, "Tuple literals cannot contain non-constant elements");
    }

    llvm::AllocaInst* inst = this->create_alloca(type);
    uint32_t index = 0;
    for (auto& element : elements) {
        llvm::Value* ptr = this->builder->CreateStructGEP(type, inst, index);
        this->builder->CreateStore(element, ptr);

        index += 1;
    }

    return this->builder->CreateLoad(type, inst);
}  