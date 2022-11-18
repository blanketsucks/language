#ifndef _TYPES_TYPE_H
#define _TYPES_TYPE_H

#if _WIN32 || _WIN64
    #if _WIN64
        #define LONG_SIZE 64
    #else
        #define LONG_SIZE 32
    #endif
#elif __GNUC__
    #if __x86_64__ || __ppc64__
        #define LONG_SIZE 64
    #else
        #define LONG_SIZE 32
    #endif
#endif

#include "llvm.h"

#include <string>

class Visitor;

class Type {
public:
    enum Value {
        Void,
        Short,
        Char,
        Integer,
        Long,
        Double,
        Float,
        Boolean,
        Array,
        Struct,
        Function,
        Pointer,
        Tuple
    };

    bool operator==(Value other);

    static Type* create(Value value, size_t size);
    static Type* from_llvm_type(llvm::Type* type);
    virtual llvm::Type* to_llvm_type(Visitor& visitor);

    virtual std::string name() { return this->str(); }
    virtual Type* copy();

    template<class T> T* cast() { return static_cast<T*>(this); }

    size_t getSize() { return this->size; }
    Value getValue() { return this->value; }

    Type* getPointerTo();
    virtual std::string str();

    Type* getPointerElementType();
    Type* getArrayElementType();
    std::vector<Type*> getTupleElementTypes();

    bool isShort() { return this->value == Short; }
    bool isChar() { return this->value == Char; }
    bool isInt() { return this->value == Integer; }
    bool isLong() { return this->value == Long; }
    bool isDouble() { return this->value == Double; }
    bool isFloat() { return this->value == Float; }
    bool isBoolean() { return this->value == Boolean; }
    bool isArray() { return this->value == Array; }
    bool isStruct() { return this->value == Struct; }
    bool isFunction() { return this->value == Function; }
    bool isVoid() { return this->value == Void; }
    bool isPointer() { return this->value == Pointer; }
    bool isTuple() { return this->value == Tuple; }
    bool isFloatingPoint() { return this->isFloat() || this->isDouble(); }
    bool isInteger() { return this->isBoolean() || this->isShort() || this->isInt() || this->isLong() || this->isChar();  }
    bool isNumeric() { return this->isInteger() || this->isFloatingPoint(); }

    bool hasContainedType() { return this->isArray() || this->isPointer(); }
    Type* getContainedType();

    virtual bool is_compatible(Type* other);
    virtual bool is_compatible(llvm::Type* type);
protected:
    Type(Value value, size_t size);

private:
    Value value;
    size_t size;
};

#endif