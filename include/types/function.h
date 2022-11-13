#ifndef _TYPES_FUNCTION_H
#define _TYPES_FUNCTION_H

#include "type.h"

#include <vector>

class FunctionType : public Type {
public:
    static FunctionType* create(std::vector<Type*> args, Type* return_type, bool is_variadic);

    static FunctionType* from_llvm_type(llvm::FunctionType* type);
    llvm::FunctionType* to_llvm_type(Visitor& visitor) override;

    FunctionType* copy() override;

    std::string str() override;

    std::vector<Type*> arguments() { return this->args; }
    Type* getReturnType() { return this->return_type; }
    bool isVariadic() { return this->is_variadic; }

    bool is_compatible(Type* other) override;
    bool is_compatible(llvm::Type* type) override;

private:
    FunctionType(std::vector<Type*> args, Type* return_type, bool is_variadic);

    std::vector<Type*> args;
    Type* return_type;
    bool is_variadic;
};

#endif