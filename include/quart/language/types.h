#pragma once

#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>

namespace quart {

class TypeRegistry;

class PointerType;
class ReferenceType;

enum class TypeKind {
    Void,
    Int,
    Float,
    Double,
    Struct,
    Array,
    Tuple,
    Enum,
    Pointer,
    Reference,
    Function
};

class Type {
public:
    virtual ~Type() = default;
    
    Type& operator=(const Type&) = delete;
    Type(const Type&) = delete;

    TypeKind kind() const;

    template<typename T> const T* as() const {
        assert(T::classof(this) && "Cannot cast to type");
        return static_cast<const T*>(this);
    }

    bool is_void() const { return this->kind() == TypeKind::Void; }
    bool is_int() const { return this->kind() == TypeKind::Int; }
    bool is_float() const { return this->kind() == TypeKind::Float; }
    bool is_double() const { return this->kind() == TypeKind::Double; }
    bool is_struct() const { return this->kind() == TypeKind::Struct; }
    bool is_array() const { return this->kind() == TypeKind::Array; }
    bool is_tuple() const { return this->kind() == TypeKind::Tuple; }
    bool is_enum() const { return this->kind() == TypeKind::Enum; }
    bool is_pointer() const { return this->kind() == TypeKind::Pointer; }
    bool is_reference() const { return this->kind() == TypeKind::Reference; }
    bool is_function() const { return this->kind() == TypeKind::Function; }

    bool is_aggregate() const { return this->is_struct() || this->is_array() || this->is_tuple(); }
    bool is_floating_point() const { return this->is_float() || this->is_double(); }
    bool is_numeric() const { return this->is_int() || this->is_floating_point(); }

    bool is_sized_type() const { return !this->is_void() && !this->is_function(); }

    bool is_mutable() const;

    // Can cast from `from` to `to` without losing information
    static bool can_safely_cast_to(Type* from, Type* to);

    TypeRegistry* get_type_registry() const;

    PointerType* get_pointer_to(bool is_mutable);
    ReferenceType* get_reference_to(bool is_mutable);

    uint32_t get_int_bit_width() const;
    bool is_int_unsigned() const;

    Type* get_pointee_type() const;
    size_t get_pointer_depth() const;

    Type* get_reference_type() const;

    std::vector<Type*> get_struct_fields() const;
    Type* get_struct_field_at(size_t index) const;
    std::string get_struct_name() const;

    Type* get_array_element_type() const;
    size_t get_array_size() const;

    std::vector<Type*> get_tuple_types() const;
    size_t get_tuple_size() const;
    Type* get_tuple_element(size_t index) const;

    Type* get_inner_enum_type() const;
    std::string get_enum_name() const;

    Type* get_function_return_type() const;
    std::vector<Type*> get_function_params() const;
    Type* get_function_param(size_t index) const;

    std::string get_as_string() const;

    void print() const;

    llvm::Type* to_llvm_type() const;

    friend TypeRegistry;
protected:
    Type(TypeRegistry* registry, TypeKind kind) : registry(registry), _kind(kind) {}
    
    TypeRegistry* registry;
private:
    TypeKind _kind;
};

class IntType : public Type {
public:
    static bool classof(const Type* type) { return type->kind() == TypeKind::Int; }

    enum {
        MIN_BITS = llvm::IntegerType::MIN_INT_BITS,
        MAX_BITS = llvm::IntegerType::MAX_INT_BITS
    };

    bool is_boolean_type() const;

    uint32_t get_bit_width() const;
    bool is_unsigned() const;

    friend TypeRegistry;
private:
    IntType(
        TypeRegistry* registry, uint32_t bits, bool is_signed
    ) : Type(registry, TypeKind::Int), bits(bits), is_signed(is_signed) {}

    uint32_t bits;
    bool is_signed;
}; 

class StructType : public Type {
public:
    static bool classof(const Type* type) { return type->kind() == TypeKind::Struct; }

    std::vector<Type*> get_fields() const;
    std::string get_name() const;

    Type* get_field_at(size_t index) const;

    void set_fields(const std::vector<Type*>& fields);

    llvm::StructType* get_llvm_struct_type() const;

    friend TypeRegistry;
private:
    StructType(
        TypeRegistry* registry, 
        const std::string& name, 
        const std::vector<Type*>& fields,
        llvm::StructType* type
    ) : Type(registry, TypeKind::Struct), name(name), fields(fields), type(type) {}

    std::string name;
    std::vector<Type*> fields;
    llvm::StructType* type;
};

class ArrayType : public Type {
public:
    static bool classof(const Type* type) { return type->kind() == TypeKind::Array; }

    size_t get_size() const;
    Type* get_element_type() const;

    friend TypeRegistry;
private:
    ArrayType(
        TypeRegistry* registry, Type* element, size_t size
    ) : Type(registry, TypeKind::Array), element(element), size(size) {}

    Type* element;
    size_t size;
};

class TupleType : public Type {
public:
    static bool classof(const Type* type) { return type->kind() == TypeKind::Tuple; }

    std::vector<Type*> get_types() const;
    size_t get_size() const;
    Type* get_type_at(size_t index) const;

    friend TypeRegistry;
private:
    TupleType(
        TypeRegistry* registry, const std::vector<Type*>& types
    ) : Type(registry, TypeKind::Tuple), types(types) {}

    std::vector<Type*> types;
};

class PointerType : public Type {
public:
    static bool classof(const Type* type) { return type->kind() == TypeKind::Pointer; }

    Type* get_pointee_type() const;
    bool is_mutable() const;

    PointerType* get_as_const();
    PointerType* get_as_mutable();
    
    friend TypeRegistry;
private:
    PointerType(
        TypeRegistry* registry, Type* pointee, bool is_mutable
    ) : Type(registry, TypeKind::Pointer), pointee(pointee), is_immutable(!is_mutable) {}

    Type* pointee;
    bool is_immutable;
};

class ReferenceType : public Type {
public:
    static bool classof(const Type* type) { return type->kind() == TypeKind::Reference; }

    Type* get_reference_type() const;
    bool is_mutable() const;

    ReferenceType* get_as_const();
    ReferenceType* get_as_mutable();
    
    friend TypeRegistry;
private:
    ReferenceType(
        TypeRegistry* registry, Type* type, bool is_mutable
    ) : Type(registry, TypeKind::Reference), type(type), is_immutable(!is_mutable) {}

    Type* type;
    bool is_immutable;
};

class EnumType : public Type {
public:
    static bool classof(const Type* type) { return type->kind() == TypeKind::Enum; }

    Type* get_inner_type() const;
    std::string get_name() const;

    friend TypeRegistry;
private:
    EnumType(
        TypeRegistry* registry, const std::string& name, Type* inner
    ) : Type(registry, TypeKind::Enum), name(name), inner(inner) {}

    std::string name;
    Type* inner;
};

class FunctionType : public Type {
public:
    static bool classof(const Type* type) { return type->kind() == TypeKind::Function; }

    Type* get_return_type() const;
    std::vector<Type*> get_parameter_types() const;
    Type* get_parameter_at(size_t index) const;

    friend TypeRegistry;
private:
    FunctionType(
        TypeRegistry* registry, Type* return_type, const std::vector<Type*>& params
    ) : Type(registry, TypeKind::Function), return_type(return_type), params(params) {}

    Type* return_type;
    std::vector<Type*> params;
};

// Returns true if `type` is a struct or a pointer to a struct
inline bool is_structure_type(Type* type) {
    if (type->is_pointer()) type = type->get_pointee_type();
    return type->is_struct();
}

}