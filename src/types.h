#ifndef _TYPES_H
#define _TYPES_H

// Determine the size of `long` depending on whether the machine running this is 32 or 64 bit.
#if _WIN32 || _WIN64
    #if _WIN64
        #define LONG_SIZE 64
    #else
        #define LONG_SIZE 32
    #endif
#endif

#if __GNUC__
    #if __x86_64__ || __ppc64__
        #define LONG_SIZE 64
    #else
        #define LONG_SIZE 32
    #endif
#endif

#include "llvm.h"

enum class TypeVarType {
    Typename,
    Value
};

struct TypeVar {
    std::string name;
    TypeVarType type;
    llvm::Any value;

    TypeVar(std::string name, TypeVarType type) : name(name), type(type) {}
};

class Type {
public:
    enum TypeValue {
        Unknown,
        Void,
        Short,
        Byte,
        Integer,
        Long,
        LongLong,
        Double,
        Float,
        String,
        Boolean,
        Array,
    };

    Type(TypeValue value = Unknown, int size = 0);
    Type(TypeValue value, int size, std::vector<TypeVar> vars);

    static Type from_llvm_type(llvm::Type* type);
    llvm::Type* to_llvm_type(llvm::LLVMContext& context);
    Type copy();

    std::string to_str();

    std::vector<TypeVar> get_type_vars() { return this->vars; }
    void set_type_var_value(int index, llvm::Any value) { vars[index].value = value; }
    void set_type_var_values(std::vector<llvm::Any> values) {
        for (int i = 0; i < values.size(); i++) {
            vars[i].value = values[i];
        }
    }

    bool is_generic() { return !vars.empty(); }
    bool is_unknown() { return this->value == Unknown; }
    bool is_short() { return this->value == Short; }
    bool is_byte() { return this->value == Byte; }
    bool is_int() { return this->value == Integer; }
    bool is_long() { return this->value == Long; }
    bool is_long_long() { return this->value == LongLong; }
    bool is_double() { return this->value == Double; }
    bool is_float() { return this->value == Float; }
    bool is_string() { return this->value == String; }
    bool is_boolean() { return this->value == Boolean; }
    bool is_array() { return this->value == Array; }
    bool is_void() { return this->value == Void; }
    bool is_floating_point() { return this->is_float() || this->is_double(); }
    bool is_numeric() { 
        return this->is_boolean() || this->is_short() || this->is_int() || this->is_long() 
            || this->is_long_long() || this->is_byte() || this->is_floating_point();
    }

    bool is_compatible(Type other);
    bool is_compatible(llvm::Type* type);

private:
    std::vector<TypeVar> vars;
    TypeValue value;
    int size;
};

class Struct : public Type {
public:
    Struct(std::string name, std::vector<Type> fields);

    static Struct from_llvm_type(llvm::StructType* type);
    llvm::StructType* to_llvm_type(llvm::LLVMContext& context);

    bool is_compatible(Type other) { return false; }
    bool is_compatible(llvm::Type* type) { return false; }

    bool is_compatible(Struct other);
    bool is_compatible(llvm::StructType* type);
private:
    std::string name;
    std::vector<Type> fields;
};

static Type VoidType = Type(Type::Void, 0);
static Type ShortType = Type(Type::Short, 16);
static Type ByteType = Type(Type::Byte, 8);
static Type IntegerType = Type(Type::Integer, 32);
static Type LongType = Type(Type::Long, LONG_SIZE);
static Type LongLongType = Type(Type::LongLong, 64);
static Type DoubleType = Type(Type::Double, 64);
static Type FloatType = Type(Type::Float, 32);
static Type StringType = Type(Type::String, 32);
static Type BooleanType = Type(Type::Boolean, 8);
static Type ArrayType = Type(Type::Array, 0, {TypeVar("T", TypeVarType::Typename), TypeVar("size", TypeVarType::Value)});

#endif