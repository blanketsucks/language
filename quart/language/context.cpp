#include <quart/language/context.h>

#define CREATE_TYPE(storage, Type, key, ...) ({                         \
    auto iterator = storage.find(key);                                  \
    if (iterator != storage.end()) {                                    \
        return static_cast<Type*>(&*iterator->second);                  \
    }                                                                   \
    auto* t = new Type(this, __VA_ARGS__);                              \
    storage[key] = OwnPtr<Type>(t);                                     \
    t;                                                                  \
})                                                                      \

#define CREATE_CONSTANT CREATE_TYPE

// NOLINTBEGIN(cppcoreguidelines-owning-memory, cppcoreguidelines-avoid-magic-numbers)

namespace quart {

Context::Context() : 
    m_void_type(this, TypeKind::Void), m_f32(this, TypeKind::Float), m_f64(this, TypeKind::Double),
    m_i1(this, 1, true), m_i8(this, 8, true), m_i16(this, 16, true), m_i32(this, 32, true), m_i64(this, 64, true),
    m_u8(this, 8, false), m_u16(this, 16, false), m_u32(this, 32, false), m_u64(this, 64, false) {}

OwnPtr<Context> Context::create() {
    return OwnPtr<Context>(new Context);
}

IntType* Context::create_int_type(::u32 bits, bool is_signed) {
    switch (bits) {
        case 1:
            return &m_i1;
        case 8:
            return is_signed ? &m_i8 : &m_u8;
        case 16:
            return is_signed ? &m_i16 : &m_u16;
        case 32:
            return is_signed ? &m_i32 : &m_u32;
        case 64:
            return is_signed ? &m_i64 : &m_u64;
        default: break;
    }

    auto key = std::make_pair(bits, is_signed);
    return CREATE_TYPE(m_integer_types, IntType, key, bits, is_signed);
}

StructType* Context::create_struct_type(const String& name, const Vector<Type*>& fields) {
    return CREATE_TYPE(m_struct_types, StructType, name, name, fields, nullptr);
}

ArrayType* Context::create_array_type(Type* element, size_t size) {
    auto key = std::make_pair(element, size);
    return CREATE_TYPE(m_array_types, ArrayType, key, element, size);
}

EnumType* Context::create_enum_type(const String& name, Type* inner) {
    return CREATE_TYPE(m_enum_types, EnumType, name, name, inner);
}

TupleType* Context::create_tuple_type(const Vector<Type*>& elements) {
    return CREATE_TYPE(m_tuple_types, TupleType, elements, elements);
}

PointerType* Context::create_pointer_type(Type* pointee, bool is_mutable) {
    auto key = std::make_pair(pointee, is_mutable);
    return CREATE_TYPE(m_pointer_types, PointerType, key, pointee, is_mutable);
}

ReferenceType* Context::create_reference_type(Type* inner, bool is_mutable) {
    auto key = std::make_pair(inner, is_mutable);
    return CREATE_TYPE(m_reference_types, ReferenceType, key, inner, is_mutable);
}

FunctionType* Context::create_function_type(Type* return_type, const Vector<Type*>& parameters, bool is_var_arg) {
    auto key = std::make_pair(return_type, std::make_pair(parameters, is_var_arg));
    return CREATE_TYPE(m_function_types, FunctionType, key, return_type, parameters, is_var_arg);
}

TraitType* Context::create_trait_type(const String& name) {
    return CREATE_TYPE(m_trait_types, TraitType, name, name);
}

EmptyType* Context::create_empty_type(const String& name) {
    return CREATE_TYPE(m_empty_types, EmptyType, name, name);
}

ConstantInt* Context::create_int_constant(::u64 value, Type* type) {
    auto key = std::make_pair(type, value);
    return CREATE_CONSTANT(m_int_constants, ConstantInt, key, type, value);
}

ConstantFloat* Context::create_float_constant(::f64 value, Type* type) {
    auto key = std::make_pair(type, value);
    return CREATE_CONSTANT(m_float_constants, ConstantFloat, key, type, value);
}

ConstantString* Context::create_string_constant(const String& value, Type* type) {
    auto key = std::make_pair(type, value);
    return CREATE_CONSTANT(m_string_constants, ConstantString, key, type, value);
}

ConstantArray* Context::create_array_constant(const Vector<Constant*>& elements, Type* type) {
    auto key = std::make_pair(type, elements);
    return CREATE_CONSTANT(m_aggregate_constants, ConstantArray, key, type, elements); // NOLINT
}

ConstantStruct* Context::create_struct_constant(const Vector<Constant*>& fields, Type* type) {
    auto key = std::make_pair(type, fields);
    return CREATE_CONSTANT(m_aggregate_constants, ConstantStruct, key, type, fields); // NOLINT
}

PointerType* Context::cstr() {
    return this->create_pointer_type(&m_i8, false);
}

// NOLINTEND(cppcoreguidelines-owning-memory, cppcoreguidelines-avoid-magic-numbers)

}