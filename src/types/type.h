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

#include <string>

#include "../llvm.h"

class Type {
public:
    enum Value {
        Unknown = -1,
        Void,
        Short,
        Char,
        Integer,
        Long,
        LongLong,
        Double,
        Float,
        String,
        Boolean,
        Array,
        Struct,
        Function,
        Pointer
    };

    Type(Value value, int size);
    virtual ~Type() = default;

    bool operator==(Value other);

    static Type* create(Value value, int size);
    static Type* fromLLVMType(llvm::Type* type);
    virtual llvm::Type* toLLVMType(llvm::LLVMContext& context);

    virtual std::string name() { return this->str(); }
    virtual Type* copy();

    template<class T> T* cast() {
        T* value = dynamic_cast<T*>(this);
        assert(value != nullptr && "Invalid cast.");

        return value;
    }

    size_t getSize() { return this->size; }

    Type* getPointerTo();
    virtual std::string str();

    Type* getPointerElementType();
    Type* getArrayElementType();

    bool isShort() { return this->value == Short; }
    bool isChar() { return this->value == Char; }
    bool isInt() { return this->value == Integer; }
    bool isLong() { return this->value == Long; }
    bool isLongLong() { return this->value == LongLong; }
    bool isDouble() { return this->value == Double; }
    bool isFloat() { return this->value == Float; }
    bool isString() { return this->value == String; }
    bool isBoolean() { return this->value == Boolean; }
    bool isArray() { return this->value == Array; }
    bool isStruct() { return this->value == Struct; }
    bool isFunction() { return this->value == Function; }
    bool isVoid() { return this->value == Void; }
    bool isPointer() { return this->value == Pointer; }
    bool isFloationPoint() { return this->isFloat() || this->isDouble(); }
    bool isInteger() { return this->isBoolean() || this->isShort() || this->isInt() || this->isLong() || this->isLongLong() || this->isChar();  }
    bool isNumeric() { return this->isInteger() || this->isFloationPoint(); }

    bool hasContainedType() { return this->isArray() || this->isPointer(); }
    Type* getContainedType();

    virtual bool is_compatible(Type* other);
    virtual bool is_compatible(llvm::Type* type);
private:
    Value value;
    size_t size;
};

static Type* VoidType = Type::create(Type::Void, 0);
static Type* ShortType = Type::create(Type::Short, 16);
static Type* CharType = Type::create(Type::Char, 8);
static Type* IntegerType = Type::create(Type::Integer, 32);
static Type* LongType = Type::create(Type::Long, LONG_SIZE);
static Type* LongLongType = Type::create(Type::LongLong, 64);
static Type* DoubleType = Type::create(Type::Double, 64);
static Type* FloatType = Type::create(Type::Float, 32);
static Type* StringType = Type::create(Type::String, 32);
static Type* BooleanType = Type::create(Type::Boolean, 8);

#endif