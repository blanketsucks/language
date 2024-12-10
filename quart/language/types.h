#pragma once

#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>

#include <quart/common.h>
#include <quart/assert.h>

namespace quart {

class Struct;
class Function;

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

    Type(Type&&) = delete;
    Type& operator=(Type&&) = delete;

    TypeKind kind() const { return m_kind; }
    TypeRegistry* type_registry() const { return m_type_registry; }

    template<typename T> requires(std::is_base_of_v<Type, T>)
    bool is() const {
        return T::classof(this);
    }

    template<typename T> requires(std::is_base_of_v<Type, T>) 
    T const* as() const {
        ASSERT(T::classof(this), "Cannot cast to type");
        return static_cast<T const*>(this);
    }

    template<typename T> requires(std::is_base_of_v<Type, T>)
    T* as() {
        ASSERT(T::classof(this), "Cannot cast to type");
        return static_cast<T*>(this);
    }

    bool is_void() const { return m_kind == TypeKind::Void; }
    bool is_int() const { return m_kind == TypeKind::Int; }
    bool is_float() const { return m_kind == TypeKind::Float; }
    bool is_double() const { return m_kind == TypeKind::Double; }
    bool is_struct() const { return m_kind == TypeKind::Struct; }
    bool is_array() const { return m_kind == TypeKind::Array; }
    bool is_tuple() const { return m_kind == TypeKind::Tuple; }
    bool is_enum() const { return m_kind == TypeKind::Enum; }
    bool is_pointer() const { return m_kind == TypeKind::Pointer; }
    bool is_reference() const { return m_kind == TypeKind::Reference; }
    bool is_function() const { return m_kind == TypeKind::Function; }

    bool is_aggregate() const { return this->is_struct() || this->is_array() || this->is_tuple(); }
    bool is_floating_point() const { return this->is_float() || this->is_double(); }
    bool is_numeric() const { return this->is_int() || this->is_floating_point(); }

    bool is_sized_type() const { return !this->is_void() && !this->is_function(); }

    // Checks whether either the pointee type or the inner reference type is `kind`
    bool is_underlying_type_of(TypeKind kind) const;
    Type* underlying_type();

    bool is_mutable() const;

    bool can_safely_cast_to(Type* to);

    PointerType* get_pointer_to(bool is_mutable = false);
    ReferenceType* get_reference_to(bool is_mutable = false);

    u32 get_int_bit_width() const;
    bool is_int_unsigned() const;

    Type* get_pointee_type() const;
    size_t get_pointer_depth() const;

    Type* get_reference_type() const;

    Vector<Type*> const& get_struct_fields() const;
    Type* get_struct_field_at(size_t index) const;
    String const& get_struct_name() const;

    Type* get_array_element_type() const;
    size_t get_array_size() const;

    Vector<Type*> const& get_tuple_types() const;
    size_t get_tuple_size() const;
    Type* get_tuple_element(size_t index) const;

    Type* get_inner_enum_type() const;
    String const& get_enum_name() const;

    Type* get_function_return_type() const;
    Vector<Type*> const& get_function_params() const;
    Type* get_function_param(size_t index) const;
    bool is_function_var_arg() const;

    size_t size() const;

    String str() const;

    void print() const;

    llvm::Type* to_llvm_type(llvm::LLVMContext&) const;

    friend TypeRegistry;
protected:
    Type(TypeRegistry* type_registry, TypeKind kind) : m_type_registry(type_registry), m_kind(kind) {}
    
    TypeRegistry* m_type_registry; // NOLINT
private:
    TypeKind m_kind;
};

class IntType : public Type {
public:
    static bool classof(const Type* type) { return type->kind() == TypeKind::Int; }

    enum {
        MIN_BITS = llvm::IntegerType::MIN_INT_BITS,
        MAX_BITS = llvm::IntegerType::MAX_INT_BITS
    };

    bool is_boolean_type() const {
        return m_bits == 1;
    }

    u32 bit_width() const { return m_bits; }
    bool is_unsigned() const { return !m_is_signed; }

    friend TypeRegistry;
private:
    IntType(
        TypeRegistry* type_registry, u32 bits, bool is_signed
    ) : Type(type_registry, TypeKind::Int), m_bits(bits), m_is_signed(is_signed) {}

    u32 m_bits;
    bool m_is_signed;
}; 

class StructType : public Type {
public:
    static bool classof(const Type* type) { return type->kind() == TypeKind::Struct; }

    Vector<Type*> const& fields() const { return m_fields; }
    String const& name() const { return m_name; }

    Type* get_field_at(size_t index) const {
        if (index >= m_fields.size()) {
            return nullptr;
        }

        return m_fields[index];
    }

    void set_fields(const Vector<Type*>& fields);

    llvm::StructType* get_llvm_struct_type() const { return m_type; }
    void set_llvm_struct_type(llvm::StructType* type) { m_type = type; }

    void set_struct(Struct* structure) { m_struct = structure; }
    Struct* get_struct() const { return m_struct; }

    friend TypeRegistry;
private:
    StructType(
        TypeRegistry* type_registry, 
        String name,
        Vector<Type*> fields,
        llvm::StructType* type,
        Struct* structure
    ) : Type(type_registry, TypeKind::Struct), m_name(move(name)), m_fields(move(fields)), m_type(type), m_struct(structure) {}

    String m_name;
    Vector<Type*> m_fields;

    llvm::StructType* m_type;
    Struct* m_struct;
};

class ArrayType : public Type {
public:
    static bool classof(const Type* type) { return type->kind() == TypeKind::Array; }

    size_t size() const { return m_size; }
    Type* element_type() const { return m_element; }

    friend TypeRegistry;
private:
    ArrayType(
        TypeRegistry* type_registry, Type* element, size_t size
    ) : Type(type_registry, TypeKind::Array), m_element(element), m_size(size) {}

    Type* m_element;
    size_t m_size;
};

class TupleType : public Type {
public:
    static bool classof(const Type* type) { return type->kind() == TypeKind::Tuple; }

    Vector<Type*> const& types() const { return m_types; }
    size_t size() const { return m_types.size(); }

    Type* get_type_at(size_t index) const {
        if (index >= m_types.size()) {
            return nullptr;
        }

        return m_types[index];
    }

    friend TypeRegistry;
private:
    TupleType(
        TypeRegistry* type_registry, Vector<Type*> types
    ) : Type(type_registry, TypeKind::Tuple), m_types(move(types)) {}

    Vector<Type*> m_types;
};

class PointerType : public Type {
public:
    static bool classof(const Type* type) { return type->kind() == TypeKind::Pointer; }

    Type* pointee() const { return m_pointee; }
    bool is_mutable() const { return m_is_mutable; }

    PointerType* as_const();
    PointerType* as_mutable();
    
    friend TypeRegistry;
private:
    PointerType(
        TypeRegistry* type_registry, Type* pointee, bool is_mutable
    ) : Type(type_registry, TypeKind::Pointer), m_pointee(pointee), m_is_mutable(is_mutable) {}

    Type* m_pointee;
    bool m_is_mutable;
};

class ReferenceType : public Type {
public:
    static bool classof(const Type* type) { return type->kind() == TypeKind::Reference; }

    Type* reference_type() const { return m_type; }
    bool is_mutable() const { return m_is_mutable; }

    ReferenceType* as_const();
    ReferenceType* as_mutable();
    
    friend TypeRegistry;
private:
    ReferenceType(
        TypeRegistry* type_registry, Type* type, bool is_mutable
    ) : Type(type_registry, TypeKind::Reference), m_type(type), m_is_mutable(is_mutable) {}

    Type* m_type;
    bool m_is_mutable;
};

class EnumType : public Type {
public:
    static bool classof(const Type* type) { return type->kind() == TypeKind::Enum; }

    Type* inner() const { return m_inner; }
    String const& name() const { return m_name; }

    friend TypeRegistry;
private:
    EnumType(
        TypeRegistry* type_registry, String name, Type* inner
    ) : Type(type_registry, TypeKind::Enum), m_name(move(name)), m_inner(inner) {}

    String m_name;
    Type* m_inner;
};

class FunctionType : public Type {
public:
    static bool classof(const Type* type) { return type->kind() == TypeKind::Function; }

    Type* return_type() const { return m_return_type; }
    Vector<Type*> const& parameters() const { return m_params; }

    Type* get_parameter_at(size_t index) const {
        if (index >= m_params.size()) {
            return nullptr;
        }

        return m_params[index];
    }

    bool is_var_arg() const { return m_is_var_arg; }
    
    Function* function() const { return m_function; }
    void set_function(Function* function) { m_function = function; }

    friend TypeRegistry;
private:
    FunctionType(
        TypeRegistry* type_registry, Type* return_type, const Vector<Type*>& params,  bool is_var_arg
    ) : Type(type_registry, TypeKind::Function), m_return_type(return_type), m_params(params), m_is_var_arg(is_var_arg) {}

    Type* m_return_type;
    Vector<Type*> m_params;

    bool m_is_var_arg;

    Function* m_function;
};

// Returns true if `type` is a struct or a pointer to a struct
inline bool is_structure_type(Type* type) {
    if (type->is_pointer()) type = type->get_pointee_type();
    return type->is_struct();
}

}
