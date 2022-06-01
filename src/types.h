#ifndef _TYPES_H
#define _TYPES_H

#include "llvm.h"

class Type {
public:
    enum TypeValue {
        Void,
        Short,
        Byte,
        Integer,
        Long,
        Double,
        Float,
        String,
        Boolean,
    };

    Type(TypeValue value = Void, int size = 0);

    static Type from_llvm_type(llvm::Type* type);
    llvm::Type* to_llvm_type(llvm::LLVMContext& context);

    std::string to_str();

    bool is_short() { return this->value == Short; }
    bool is_byte() { return this->value == Byte; }
    bool is_int() { return this->value == Integer; }
    bool is_long() { return this->value == Long; }
    bool is_double() { return this->value == Double; }
    bool is_float() { return this->value == Float; }
    bool is_string() { return this->value == String; }
    bool is_boolean() { return this->value == Boolean; }
    bool is_void() { return this->value == Void; }
    bool is_floating_point() { return this->is_float() || this->is_double(); }
    bool is_numeric() { 
        return this->is_boolean() || this->is_short() || this->is_int() || this->is_long() || this->is_byte() || this->is_floating_point();
    }

    bool is_compatible(Type other);
    bool is_compatible(llvm::Type* type);

private:
    TypeValue value;
    int size;
};

static Type VoidType = Type(Type::Void, 0);
static Type ShortType = Type(Type::Short, 16);
static Type ByteType = Type(Type::Byte, 8);
static Type IntegerType = Type(Type::Integer, 32);
static Type LongType = Type(Type::Long, 64);
static Type DoubleType = Type(Type::Double, 64);
static Type FloatType = Type(Type::Float, 32);
static Type StringType = Type(Type::String, 32);
static Type BooleanType = Type(Type::Boolean, 8);

#endif