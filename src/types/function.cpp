#include "types/function.h"

#include "utils.h"

#include <sstream>
#include <stdint.h>

FunctionType::FunctionType(
    std::vector<Type*> args, Type* return_type, bool is_variadic
) : Type(Type::Function, 0), args(args), return_type(return_type), is_variadic(is_variadic) {}

FunctionType* FunctionType::create(std::vector<Type*> args, Type* return_type, bool is_variadic) {
    auto type = new FunctionType(args, return_type, is_variadic);
    Type::push(type);

    return type;
}

FunctionType* FunctionType::from_llvm_type(llvm::FunctionType* type) {
    std::vector<Type*> args;
    for (auto arg : type->params()) {
        args.push_back(Type::from_llvm_type(arg));
    }

    return FunctionType::create(args, Type::from_llvm_type(type->getReturnType()), type->isVarArg());
}

llvm::FunctionType* FunctionType::to_llvm_type(Visitor& visitor) {
    std::vector<llvm::Type*> args;
    for (auto arg : this->args) {
        args.push_back(arg->to_llvm_type(visitor));
    }

    return llvm::FunctionType::get(
        this->return_type->to_llvm_type(visitor), args, this->is_variadic
    );
}

FunctionType* FunctionType::copy() {
    std::vector<Type*> args;
    for (auto arg : this->args) {
        args.push_back(arg->copy());
    }

    return FunctionType::create(args, this->return_type->copy(), this->is_variadic);
}

std::string FunctionType::str() {
    std::stringstream stream;
    stream << "func(";

    for (auto argument : this->args) {
        stream << argument->str() << ", ";
    }

    if (this->args.size() > 0) {
        stream.seekp(-2, stream.cur);
    }

    stream << ") -> " << this->return_type->str();
    return stream.str();
}

bool FunctionType::is_compatible(Type* other) {
    if (!other->isFunction()) {
        return false;
    }

    FunctionType* func = (FunctionType*)other;
    if (this->args.size() != func->args.size()) {
        return false;
    }

    for (size_t i = 0; i < this->args.size(); i++) {
        if (!this->args[i]->is_compatible(func->args[i])) {
            return false;
        }
    }

    return this->return_type->is_compatible(func->return_type);
}

bool FunctionType::is_compatible(llvm::Type* type) {
    return this->is_compatible(Type::from_llvm_type(type));
}