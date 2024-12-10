#include <quart/language/type_registry.h>

// NOLINTBEGIN(cppcoreguidelines-owning-memory)

using namespace quart;

TypeRegistry::TypeRegistry() : 
    m_void_type(this, TypeKind::Void), m_f32(this, TypeKind::Float), m_f64(this, TypeKind::Double),
    m_i1(this, 1, true), m_i8(this, 8, true), m_i16(this, 16, true), m_i32(this, 32, true), m_i64(this, 64, true),
    m_u8(this, 8, false), m_u16(this, 16, false), m_u32(this, 32, false), m_u64(this, 64, false) {}

OwnPtr<TypeRegistry> TypeRegistry::create() {
    return OwnPtr<TypeRegistry>(new TypeRegistry);
}

IntType* TypeRegistry::create_int_type(::u32 bits, bool is_signed) {
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

    auto iterator = m_integers.find({ bits, is_signed });
    if (iterator != m_integers.end()) {
        return &*iterator->second;
    }

    auto* type = new IntType(this, bits, is_signed);
    m_integers[{ bits, is_signed }] = OwnPtr<IntType>(type);

    return type;
}

StructType* TypeRegistry::create_struct_type(const String& name, const Vector<Type*>& fields, llvm::StructType* llvm_type) {
    auto iterator = m_structs.find(name);
    if (iterator != m_structs.end()) {
        return &*iterator->second;
    }

    auto* type = new StructType(this, name, fields, llvm_type, nullptr);
    m_structs[name] = OwnPtr<StructType>(type);

    return type;
}

ArrayType* TypeRegistry::create_array_type(Type* element, size_t size) {
    auto iterator = m_arrays.find({ element, size });
    if (iterator != m_arrays.end()) {
        return &*iterator->second;
    }

    auto* type = new ArrayType(this, element, size);
    m_arrays[{ element, size }] = OwnPtr<ArrayType>(type);

    return type;
}

EnumType* TypeRegistry::create_enum_type(const String& name, Type* inner) {
    auto iterator = m_enums.find(name);
    if (iterator != m_enums.end()) {
        return &*iterator->second;
    }

    auto* type = new EnumType(this, name, inner);
    m_enums[name] = OwnPtr<EnumType>(type);

    return type;
}

TupleType* TypeRegistry::create_tuple_type(const Vector<Type*>& elements) {
    auto iterator = m_tuples.find(elements);
    if (iterator != m_tuples.end()) {
        return &*iterator->second;
    }

    auto* type = new TupleType(this, elements);
    m_tuples[elements] = OwnPtr<TupleType>(type);

    return type;
}

PointerType* TypeRegistry::create_pointer_type(Type* pointee, bool is_mutable) {
    auto iterator = m_pointers.find({ pointee, is_mutable });
    if (iterator != m_pointers.end()) {
        return &*iterator->second;
    }

    auto* type = new PointerType(this, pointee, is_mutable);
    m_pointers[{ pointee, is_mutable }] = OwnPtr<PointerType>(type);

    return type;
}

ReferenceType* TypeRegistry::create_reference_type(Type* inner, bool is_mutable) {
    auto iterator = m_references.find({ inner, is_mutable });
    if (iterator != m_references.end()) {
        return &*iterator->second;
    }

    auto* type = new ReferenceType(this, inner, is_mutable);
    m_references[{ type, is_mutable }] = OwnPtr<ReferenceType>(type);

    return type;
}

FunctionType* TypeRegistry::create_function_type(Type* return_type, const Vector<Type*>& parameters, bool is_var_arg) {
    auto iterator = m_functions.find({ return_type, { parameters, is_var_arg } });
    if (iterator != m_functions.end()) {
        return &*iterator->second;
    }

    auto* type = new FunctionType(this, return_type, parameters, is_var_arg);
    m_functions[{ return_type, { parameters, is_var_arg } }] = OwnPtr<FunctionType>(type);

    return type;
}

PointerType* TypeRegistry::cstr() {
    return this->create_pointer_type(&m_i8, false);
}

// NOLINTEND(cppcoreguidelines-owning-memory)