#pragma once

#include <quart/language/types.h>
#include <quart/parser/ast.h>
#include <quart/llvm.h>
#include <quart/common.h>

#include <memory>
#include <map>

namespace quart {

using IntTypeStorageKey = std::pair<u32, bool>;
using TupleTypeStorageKey = Vector<Type*>;
using PointerTypeStorageKey = std::pair<Type*, bool>;
using ArrayTypeStorageKey = std::pair<Type*, size_t>;
using FunctionTypeStorageKey = std::pair<Type*, Vector<Type*>>;

class TypeRegistry {
public:
    template<typename K, typename V> using TypeMap = std::map<K, V, std::equal_to<K>>;

    static OwnPtr<TypeRegistry> create();

    IntType* create_int_type(u32 bits, bool is_signed);
    StructType* create_struct_type(const String& name, const Vector<Type*>& fields, llvm::StructType* type = nullptr);
    EnumType* create_enum_type(const String& name, Type* type);
    ArrayType* create_array_type(Type* element, size_t size);
    TupleType* create_tuple_type(const Vector<Type*>& types);
    PointerType* create_pointer_type(Type* pointee, bool is_mutable);
    ReferenceType* create_reference_type(Type* type, bool is_mutable);
    FunctionType* create_function_type(Type* return_type, const Vector<Type*>& parameters);

    Type* void_type() { return &m_void_type; }

    Type* f32() { return &m_f32; }
    Type* f64() { return &m_f64; }

    IntType* i1() { return &m_i1; }

    IntType* i8() { return &m_i8; }
    IntType* i16() { return &m_i16; }
    IntType* i32() { return &m_i32; }
    IntType* i64() { return &m_i64; }

    IntType* u8() { return &m_u8; }
    IntType* u16() { return &m_u16; }
    IntType* u32() { return &m_u32; }
    IntType* u64() { return &m_u64; }

    PointerType* cstr();

    friend class Visitor;
private:
    TypeRegistry();

    Type m_void_type, m_f32, m_f64;

    IntType m_i1, m_i8, m_i16, m_i32, m_i64;
    IntType m_u8, m_u16, m_u32, m_u64;

    HashMap<String, OwnPtr<StructType>> m_structs;
    HashMap<String, OwnPtr<EnumType>> m_enums;

    TypeMap<IntTypeStorageKey, OwnPtr<IntType>> m_integers;

    TypeMap<PointerTypeStorageKey, OwnPtr<PointerType>> m_pointers;
    TypeMap<PointerTypeStorageKey, OwnPtr<ReferenceType>> m_references;

    TypeMap<ArrayTypeStorageKey, OwnPtr<ArrayType>> m_arrays;
    TypeMap<TupleTypeStorageKey, OwnPtr<TupleType>> m_tuples;

    TypeMap<FunctionTypeStorageKey, OwnPtr<FunctionType>> m_functions;
};

}