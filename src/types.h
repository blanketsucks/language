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

#include <map>
#include "llvm.h"

enum class TypeVarType {
    Typename,
    Integer
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
        Struct,
        Pointer
    };

    Type(TypeValue value, int size);
    Type(TypeValue value, int size, std::vector<TypeVar> vars);

    static Type* create(TypeValue value, int size);
    static Type* create(TypeValue value, int size, std::vector<TypeVar> vars);
    static Type* from_llvm_type(llvm::Type* type);

    virtual llvm::Type* to_llvm_type(llvm::LLVMContext& context);
    virtual std::string get_name() { return this->to_str(); }
    virtual Type* copy();

    Type* to_pointer_type();
    std::string to_str();

    std::vector<TypeVar> get_type_vars() { return this->vars; }
    void set_type_var_value(int index, llvm::Any value) { vars[index].value = value; }
    void set_type_var_values(std::vector<llvm::Any> values) {
        for (int i = 0; i < values.size(); i++) {
            vars[i].value = values[i];
        }
    }

    Type* get_element_pointer_type();

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
    bool is_struct() { return this->value == Struct; }
    bool is_void() { return this->value == Void; }
    bool is_pointer() { return this->value == Pointer; }
    bool is_floating_point() { return this->is_float() || this->is_double(); }
    bool is_numeric() { return this->is_boolean() || this->is_short() || this->is_int() || this->is_long() || this->is_long_long() || this->is_byte() || this->is_floating_point(); }

    virtual bool is_compatible(Type* other);
    virtual bool is_compatible(llvm::Type* type);

private:
    std::vector<TypeVar> vars;
    TypeValue value;
    int size;
};

class PointerType : public Type {
public:
    static PointerType* create(Type* type);

    static PointerType* from_llvm_type(llvm::Type* type);
    llvm::PointerType* to_llvm_type(llvm::LLVMContext& context) override;
    
    PointerType* copy() override;

    std::string get_name() override { return this->element->get_name() + "*"; }
    Type* get_element_type() { return this->element; }

    bool is_compatible(Type* other) override;
    bool is_compatible(llvm::Type* type) override;

private:
    PointerType(Type* type);

    Type* element;
};

class StructType : public Type {
public:
    static StructType* create(std::string name, std::vector<Type*> fields);

    static StructType* from_llvm_type(llvm::StructType* type);
    llvm::StructType* to_llvm_type(llvm::LLVMContext& context) override;

    StructType* copy() override;

    std::string get_name() override { return this->name; }
    void set_fields(std::vector<Type*> fields) { this->fields = fields; }

    bool is_compatible(Type* other) override;
    bool is_compatible(llvm::Type* type) override;

private:
    StructType(std::string name, std::vector<Type*> fields);

    std::string name;
    std::vector<Type*> fields;
};

class FunctionType : public Type {
public:
    static FunctionType* create(std::vector<Type*> args, Type* return_type, bool has_varargs);

    static FunctionType* from_llvm_type(llvm::FunctionType* type);
    llvm::FunctionType* to_llvm_type(llvm::LLVMContext& context) override;

    FunctionType* copy() override;

    std::vector<Type*> arguments() { return this->args; }
    Type* get_return_type() { return this->return_type; }
    bool is_varargs() { return this->has_varargs; }

    bool is_compatible(Type* other) override;
    bool is_compatible(llvm::Type* type) override;

private:
    FunctionType(std::vector<Type*> args, Type* return_type, bool has_varargs);

    std::vector<Type*> args;
    Type* return_type;
    bool has_varargs;
};

static Type* VoidType = Type::create(Type::Void, 0);
static Type* ShortType = Type::create(Type::Short, 16);
static Type* ByteType = Type::create(Type::Byte, 8);
static Type* IntegerType = Type::create(Type::Integer, 32);
static Type* LongType = Type::create(Type::Long, LONG_SIZE);
static Type* LongLongType = Type::create(Type::LongLong, 64);
static Type* DoubleType = Type::create(Type::Double, 64);
static Type* FloatType = Type::create(Type::Float, 32);
static Type* StringType = Type::create(Type::String, 32);
static Type* BooleanType = Type::create(Type::Boolean, 8);
static Type* ArrayType = Type::create(Type::Array, 0, {TypeVar("T", TypeVarType::Typename), TypeVar("size", TypeVarType::Integer)});

#endif