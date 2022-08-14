#include "types/function.h"

#include "utils.h"

#include <sstream>

FunctionType::FunctionType(
    std::vector<Type*> args, Type* return_type, bool has_varargs
) : Type(Type::Function, 0), args(args), return_type(return_type), has_varargs(has_varargs) {}

FunctionType::~FunctionType() {
    // for (auto argument : this->args) {
    //     delete argument;
    // }

    // delete this->return_type;
}

FunctionType* FunctionType::create(std::vector<Type*> args, Type* return_type, bool has_varargs) {
    return new FunctionType(args, return_type, has_varargs);
}

FunctionType* FunctionType::from_llvm_type(llvm::FunctionType* type) {
    std::vector<Type*> args;
    for (auto arg : type->params()) {
        args.push_back(Type::from_llvm_type(arg));
    }

    return FunctionType::create(args, Type::from_llvm_type(type->getReturnType()), type->isVarArg());
}

llvm::Type* FunctionType::to_llvm_type(llvm::LLVMContext& context) {
    std::vector<llvm::Type*> types;
    for (auto& arg : this->args) {
        types.push_back(arg->to_llvm_type(context));
    }

    return llvm::FunctionType::get(this->return_type->to_llvm_type(context), types, this->has_varargs)->getPointerTo();
}

FunctionType* FunctionType::copy() {
    std::vector<Type*> args;
    for (auto arg : this->args) {
        args.push_back(arg->copy());
    }

    return FunctionType::create(args, this->return_type->copy(), this->has_varargs);
}

std::string FunctionType::str() {
    std::stringstream stream;
    stream << "(";

    for (auto argument : this->args) {
        stream << argument->str() << ", ";
    }

    stream << this->return_type->str() << ")";
    return stream.str();
}

bool FunctionType::is_compatible(Type* other) {
    if (!other->isFunction()) {
        return false;
    }

    // TODO: this
    return true;
}

bool FunctionType::is_compatible(llvm::Type* type) {
    return this->is_compatible(Type::from_llvm_type(type));
}