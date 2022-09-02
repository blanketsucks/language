#ifndef _TYPES_FUNCTION_H
#define _TYPES_FUNCTION_H

#include "type.h"

#include <vector>

class FunctionType : public Type {
public:
    static FunctionType* create(std::vector<Type*> args, Type* return_type, bool has_varargs);
    ~FunctionType() override;

    static FunctionType* from_llvm_type(llvm::FunctionType* type);
    llvm::FunctionType* to_llvm_type(llvm::LLVMContext& context) override;

    FunctionType* copy() override;

    std::string str() override;

    uint32_t hash() override;

    std::vector<Type*> arguments() { return this->args; }
    Type* getReturnType() { return this->return_type; }
    bool isVarargs() { return this->has_varargs; }

    bool is_compatible(Type* other) override;
    bool is_compatible(llvm::Type* type) override;

private:
    FunctionType(std::vector<Type*> args, Type* return_type, bool has_varargs);

    std::vector<Type*> args;
    Type* return_type;
    bool has_varargs;
};

#endif