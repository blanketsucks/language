#pragma once

#include <quart/language/types.h>
#include <quart/parser/ast.h>
#include <quart/llvm.h>

#include <llvm/ADT/DenseMap.h>
#include <memory>
#include <map>

namespace quart {

struct ArrayTypeStorageKey {
    Type* element;
    size_t size;

    ArrayTypeStorageKey(Type* element, size_t size) : element(element), size(size) {}

    bool operator<(const ArrayTypeStorageKey& other) const {
        return this->element < other.element && this->size < other.size;
    }
};

struct FunctionTypeStorageKey {
    Type* return_type;
    std::vector<Type*> parameters;

    FunctionTypeStorageKey(Type* return_type, const std::vector<Type*>& parameters) : return_type(return_type), parameters(parameters) {}

    bool operator<(const FunctionTypeStorageKey& other) const {
        return this->return_type < other.return_type && this->parameters < other.parameters;
    }
};

using IntTypeStorageKey = std::pair<uint32_t, bool>;
using TupleTypeStorageKey = std::vector<Type*>;
using PointerTypeStorageKey = std::pair<Type*, bool>;

class TypeRegistry {
public:
    template<typename K, typename V> using TypeMap = std::map<K, V, std::equal_to<K>>;

    static std::unique_ptr<TypeRegistry> create(llvm::LLVMContext& context);

    void clear();

    Type* wrap(llvm::Type* type);
    StructType* wrap(llvm::StructType* type);

    IntType* create_int_type(uint32_t bits, bool is_signed);
    StructType* create_struct_type(const std::string& name, const std::vector<Type*>& fields, llvm::StructType* type = nullptr);
    EnumType* create_enum_type(const std::string& name, Type* type);
    ArrayType* create_array_type(Type* element, size_t size);
    TupleType* create_tuple_type(const std::vector<Type*>& types);
    PointerType* create_pointer_type(Type* pointee, bool is_mutable);
    ReferenceType* create_reference_type(Type* type, bool is_mutable);
    FunctionType* create_function_type(Type* return_type, const std::vector<Type*>& parameters);

    Type* get_void_type();
    Type* get_f32_type();
    Type* get_f64_type();

    llvm::LLVMContext& get_context() { return this->context; }

    friend class Visitor;
private:
    TypeRegistry(llvm::LLVMContext& context);

    llvm::LLVMContext& context;

    Type void_type, f32, f64;

    IntType i1, i8, i16, i32, i64;
    IntType u8, u16, u32, u64;

    std::map<std::string, StructType*> structs;
    std::map<std::string, EnumType*> enums;
    std::map<ArrayTypeStorageKey, ArrayType*> arrays;
    std::map<TupleTypeStorageKey, TupleType*> tuples;
    std::map<IntTypeStorageKey, IntType*> integers;
    std::map<PointerTypeStorageKey, PointerType*> pointers;
    std::map<PointerTypeStorageKey, ReferenceType*> references;
    std::map<FunctionTypeStorageKey, FunctionType*> functions;
};

}