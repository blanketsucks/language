#pragma once

#include <quart/language/types.h>
#include <quart/parser/ast.h>
#include <quart/llvm.h>
#include <quart/common.h>

#include <llvm/ADT/DenseMap.h>
#include <memory>
#include <map>

namespace quart {

using IntTypeStorageKey = std::pair<u32, bool>;
using TupleTypeStorageKey = std::vector<Type*>;
using PointerTypeStorageKey = std::pair<Type*, bool>;
using ArrayTypeStorageKey = std::pair<Type*, size_t>;
using FunctionTypeStorageKey = std::pair<Type*, std::vector<Type*>>;

class TypeRegistry {
public:
    template<typename K, typename V> using TypeMap = std::map<K, V, std::equal_to<K>>;

    static std::unique_ptr<TypeRegistry> create(llvm::LLVMContext& context);

    void clear();

    Type* wrap(llvm::Type* type);
    StructType* wrap(llvm::StructType* type);

    IntType* create_int_type(u32 bits, bool is_signed);
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