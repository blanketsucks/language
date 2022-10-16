#ifndef _TYPES_STRUCT_H
#define _TYPES_STRUCT_H

#include "type.h"

#include <vector>

class StructType : public Type {
public:
    static StructType* create(std::string name, std::vector<Type*> fields);
    
    static StructType* from_llvm_type(llvm::StructType* type);
    llvm::StructType* to_llvm_type(llvm::LLVMContext& context) override;

    StructType* copy() override;

    std::string str() override;

    void setFields(std::vector<Type*> fields) { this->fields = fields; }

    bool is_compatible(Type* other) override;
    bool is_compatible(llvm::Type* type) override;

private:
    StructType(std::string name, std::vector<Type*> fields);

    std::string name;
    std::vector<Type*> fields;
};

#endif