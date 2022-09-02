#include "visitor.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"

Value Visitor::visit(ast::TupleExpr* expr) {
    std::vector<Type*> types;
    std::vector<llvm::Value*> elements;

    for (auto& elem : expr->elements) {
        llvm::Value* value = elem->accept(*this).unwrap(elem->start);

        elements.push_back(value);
        types.push_back(Type::from_llvm_type(value->getType()));
    }

    uint32_t hash = TupleType::getHashFromTypes(types);
    llvm::StructType* type = nullptr;
    if (this->tuples.find(hash) != this->tuples.end()) {
        type = this->tuples[hash];
    } else {
        TupleType* tuple = TupleType::create(types);
        type = tuple->to_llvm_type(this->context);

        this->tuples[hash] = type;
    }
    
    bool is_const = std::all_of(
        expr->elements.begin(),
        expr->elements.end(),
        [](auto& expr) { return expr->is_constant(); }
    );

    if (is_const) {
        std::vector<llvm::Constant*> constants;
        for (auto& element : elements) {
            constants.push_back(llvm::cast<llvm::Constant>(element));
        }

        return Value(llvm::ConstantStruct::get(type, constants), true);
    }

    if (!this->current_function) {
        ERROR(expr->start, "Tuple literals cannot contain non-constant elements");
    }

    llvm::Function* function = this->builder->GetInsertBlock()->getParent();
    llvm::AllocaInst* inst = this->create_alloca(function, type);

    uint32_t index = 0;
    for (auto& element : elements) {
        llvm::Value* ptr = this->builder->CreateStructGEP(type, inst, index);
        this->builder->CreateStore(element, ptr);

        index += 1;
    }

    return this->builder->CreateLoad(type, inst);
}  