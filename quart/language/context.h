#pragma once

#include <quart/common.h>
#include <quart/language/types.h>
#include <quart/bytecode/constant.h>

namespace quart {

using IntTypeStorageKey = Pair<u32, bool>;
using TupleTypeStorageKey = Vector<Type*>;
using PointerTypeStorageKey = Pair<Type*, bool>;
using ArrayTypeStorageKey = Pair<Type*, size_t>;
using FunctionTypeStorageKey = Pair<Type*, Pair<Vector<Type*>, bool>>;

template<typename T> requires(std::is_base_of_v<bytecode::Constant, T>)
using ConstantMap = HashMap<Pair<Type*, typename T::value_type>, OwnPtr<T>>;

class Context {
public:
    template<typename K, typename V> using TypeMap = std::map<K, V>;

    static OwnPtr<Context> create();

    IntType* create_int_type(u32 bits, bool is_signed);
    StructType* create_struct_type(const String& name, const Vector<Type*>& fields);
    EnumType* create_enum_type(const String& name, Type* type);
    ArrayType* create_array_type(Type* element, size_t size);
    TupleType* create_tuple_type(const Vector<Type*>& types);
    PointerType* create_pointer_type(Type* pointee, bool is_mutable);
    ReferenceType* create_reference_type(Type* type, bool is_mutable);
    FunctionType* create_function_type(Type* return_type, const Vector<Type*>& parameters, bool is_var_arg = false);
    TraitType* create_trait_type(const String& name);
    EmptyType* create_empty_type(const String& name);

    bytecode::ConstantInt* create_int_constant(u64 value, Type* type);
    bytecode::ConstantFloat* create_float_constant(f64 value, Type* type);
    bytecode::ConstantString* create_string_constant(const String& value, Type* type);
    bytecode::ConstantArray* create_array_constant(const Vector<bytecode::Constant*>& elements, Type* type);
    bytecode::ConstantStruct* create_struct_constant(const Vector<bytecode::Constant*>& fields, Type* type);
    bytecode::ConstantNull* create_null_constant(Type* type);

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

private:
    Context();

    Type m_void_type, m_f32, m_f64;

    IntType m_i1, m_i8, m_i16, m_i32, m_i64;
    IntType m_u8, m_u16, m_u32, m_u64;

    HashMap<String, OwnPtr<StructType>> m_struct_types;
    HashMap<String, OwnPtr<EnumType>> m_enum_types;

    TypeMap<IntTypeStorageKey, OwnPtr<IntType>> m_integer_types;

    TypeMap<PointerTypeStorageKey, OwnPtr<PointerType>> m_pointer_types;
    TypeMap<PointerTypeStorageKey, OwnPtr<ReferenceType>> m_reference_types;

    TypeMap<ArrayTypeStorageKey, OwnPtr<ArrayType>> m_array_types;
    TypeMap<TupleTypeStorageKey, OwnPtr<TupleType>> m_tuple_types;

    TypeMap<FunctionTypeStorageKey, OwnPtr<FunctionType>> m_function_types;

    TypeMap<String, OwnPtr<TraitType>> m_trait_types;
    TypeMap<String, OwnPtr<EmptyType>> m_empty_types;

    ConstantMap<bytecode::ConstantInt> m_int_constants;
    ConstantMap<bytecode::ConstantFloat> m_float_constants;
    
    ConstantMap<bytecode::ConstantString> m_string_constants;

    HashMap<Type*, OwnPtr<bytecode::ConstantNull>> m_null_constants;

    HashMap<Pair<Type*, Vector<bytecode::Constant*>>, OwnPtr<bytecode::Constant>> m_aggregate_constants;
};

}