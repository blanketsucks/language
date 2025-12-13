#pragma once

#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>

#include <quart/common.h>
#include <quart/assert.h>

namespace quart {

class Struct;
class Function;

class Context;

class PointerType;
class ReferenceType;

enum class TypeKind : u8 {
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
    Function,
    Trait
};

class Type {
public:
    virtual ~Type() = default;

    NO_COPY(Type)
    NO_MOVE(Type)

    TypeKind kind() const { return m_kind; }
    Context* context() const { return m_context; }

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
    bool is_trait() const { return m_kind == TypeKind::Trait; }

    bool is_aggregate() const { return this->is_struct() || this->is_array() || this->is_tuple(); }
    bool is_floating_point() const { return this->is_float() || this->is_double(); }
    bool is_numeric() const { return this->is_int() || this->is_floating_point(); }

    bool is_sized_type() const { return !this->is_void() && !this->is_function() && !this->is_trait(); }

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

    String const& get_trait_name() const;

    size_t size() const;

    String str() const;

    void print() const;

    llvm::Type* to_llvm_type(llvm::LLVMContext&) const;

    friend Context;
protected:
    Type(Context* context, TypeKind kind) : m_context(context), m_kind(kind) {}
    
    Context* m_context; // NOLINT
private:
    TypeKind m_kind;
};

class IntType : public Type {
public:
    static bool classof(const Type* type) { return type->kind() == TypeKind::Int; }

    static IntType* get(Context&, u32 bits, bool is_signed);

    enum {
        MIN_BITS = llvm::IntegerType::MIN_INT_BITS,
        MAX_BITS = llvm::IntegerType::MAX_INT_BITS
    };

    bool is_boolean_type() const {
        return m_bits == 1;
    }

    u32 bit_width() const { return m_bits; }
    bool is_unsigned() const { return !m_is_signed; }

    friend Context;
private:
    IntType(
        Context* context, u32 bits, bool is_signed
    ) : Type(context, TypeKind::Int), m_bits(bits), m_is_signed(is_signed) {}

    u32 m_bits;
    bool m_is_signed;
}; 

class StructType : public Type {
public:
    static bool classof(const Type* type) { return type->kind() == TypeKind::Struct; }

    static StructType* get(Context&, const String& name, const Vector<Type*>& fields);

    Vector<Type*> const& fields() const { return m_fields; }
    String const& name() const { return m_name; }

    Struct const* decl() const { return m_decl; }
    Struct* decl() { return m_decl; }

    Type* get_field_at(size_t index) const {
        if (index >= m_fields.size()) {
            return nullptr;
        }

        return m_fields[index];
    }

    void set_fields(const Vector<Type*>& fields);
    void set_decl(Struct* decl) { m_decl = decl; }

    friend Context;
private:
    StructType(
        Context* context, 
        String name,
        Vector<Type*> fields,
        Struct* decl
    ) : Type(context, TypeKind::Struct), m_name(move(name)), m_fields(move(fields)), m_decl(decl) {}

    String m_name;
    Vector<Type*> m_fields;

    Struct* m_decl;
};

class ArrayType : public Type {
public:
    static bool classof(const Type* type) { return type->kind() == TypeKind::Array; }

    static ArrayType* get(Context&, Type* element, size_t size);

    size_t size() const { return m_size; }
    Type* element_type() const { return m_element; }

    friend Context;
private:
    ArrayType(
        Context* context, Type* element, size_t size
    ) : Type(context, TypeKind::Array), m_element(element), m_size(size) {}

    Type* m_element;
    size_t m_size;
};

class TupleType : public Type {
public:
    static bool classof(const Type* type) { return type->kind() == TypeKind::Tuple; }

    static TupleType* get(Context&, const Vector<Type*>& types);

    Vector<Type*> const& types() const { return m_types; }
    size_t size() const { return m_types.size(); }

    Type* get_type_at(size_t index) const {
        if (index >= m_types.size()) {
            return nullptr;
        }

        return m_types[index];
    }

    friend Context;
private:
    TupleType(
        Context* context, Vector<Type*> types
    ) : Type(context, TypeKind::Tuple), m_types(move(types)) {}

    Vector<Type*> m_types;
};

class PointerType : public Type {
public:
    static bool classof(const Type* type) { return type->kind() == TypeKind::Pointer; }

    static PointerType* get(Context&, Type* pointee, bool is_mutable);

    Type* pointee() const { return m_pointee; }
    bool is_mutable() const { return m_is_mutable; }

    PointerType* as_const();
    PointerType* as_mutable();
    
    friend Context;
private:
    PointerType(
        Context* context, Type* pointee, bool is_mutable
    ) : Type(context, TypeKind::Pointer), m_pointee(pointee), m_is_mutable(is_mutable) {}

    Type* m_pointee;
    bool m_is_mutable;
};

class ReferenceType : public Type {
public:
    static bool classof(const Type* type) { return type->kind() == TypeKind::Reference; }

    static ReferenceType* get(Context&, Type* type, bool is_mutable);

    Type* reference_type() const { return m_type; }
    bool is_mutable() const { return m_is_mutable; }

    ReferenceType* as_const();
    ReferenceType* as_mutable();
    
    friend Context;
private:
    ReferenceType(
        Context* context, Type* type, bool is_mutable
    ) : Type(context, TypeKind::Reference), m_type(type), m_is_mutable(is_mutable) {}

    Type* m_type;
    bool m_is_mutable;
};

class EnumType : public Type {
public:
    static bool classof(const Type* type) { return type->kind() == TypeKind::Enum; }

    static EnumType* get(Context&, const String& name, Type* inner);

    Type* inner() const { return m_inner; }
    String const& name() const { return m_name; }

    friend Context;
private:
    EnumType(
        Context* context, String name, Type* inner
    ) : Type(context, TypeKind::Enum), m_name(move(name)), m_inner(inner) {}

    String m_name;
    Type* m_inner;
};

class FunctionType : public Type {
public:
    static bool classof(const Type* type) { return type->kind() == TypeKind::Function; }

    static FunctionType* get(Context&, Type* return_type, const Vector<Type*>& params, bool is_var_arg);

    Type* return_type() const { return m_return_type; }
    Vector<Type*> const& parameters() const { return m_params; }
    size_t parameter_count() const { return m_params.size(); }

    Type* get_parameter_at(size_t index) const {
        if (index >= m_params.size()) {
            return nullptr;
        }

        return m_params[index];
    }

    bool is_var_arg() const { return m_is_var_arg; }

    friend Context;
private:
    FunctionType(
        Context* context, Type* return_type, const Vector<Type*>& params,  bool is_var_arg
    ) : Type(context, TypeKind::Function), m_return_type(return_type), m_params(params), m_is_var_arg(is_var_arg) {}

    Type* m_return_type;
    Vector<Type*> m_params;

    bool m_is_var_arg;
};

class TraitType : public Type {
public:
    static bool classof(const Type* type) { return type->kind() == TypeKind::Trait; }

    static TraitType* get(Context&, const String& name);

    String const& name() const { return m_name; }

    friend Context;
private:
    TraitType(Context* context, String name) : Type(context, TypeKind::Trait), m_name(move(name)) {}

    String m_name;
};

// Returns true if `type` is a struct or a pointer to a struct
inline bool is_structure_type(Type* type) {
    if (type->is_pointer()) type = type->get_pointee_type();
    return type->is_struct();
}

}
