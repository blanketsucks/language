#pragma once

#include <quart/common.h>
#include <quart/language/types.h>

namespace quart {

class Constant {
public:
    enum class Kind {
        Int,
        Float,
        String,
        Array,
        Struct
    };

    NO_COPY(Constant)
    NO_MOVE(Constant)

    virtual ~Constant() = default;

    Context* context() const { return m_context; }

    Kind kind() const { return m_kind; }
    Type* type() const { return m_type; }

    template<typename T> requires(std::is_base_of_v<Constant, T>)
    bool is() const { return T::classof(this); }

    template<typename T> requires(std::is_base_of_v<Constant, T>)
    T* as() {
        if (!T::classof(this)) {
            return nullptr;
        }

        return static_cast<T*>(this); 
    }

protected:
    Constant(Context* context, Kind kind, Type* type) : m_context(context), m_kind(kind), m_type(type) {}

private:
    Context* m_context;

    Kind m_kind;
    Type* m_type;
};

class ConstantInt : public Constant {
public:
    friend Context;
    using value_type = u64;

    static bool classof(const Constant* constant) { return constant->kind() == Kind::Int; }

    static ConstantInt* get(Context&, Type*, u64 value);
    
    u64 value() const { return m_value; }
    
private:
    ConstantInt(Context* context, Type* type, u64 value) : Constant(context, Kind::Int, type), m_value(value) {}
    
    u64 m_value;
};

class ConstantFloat : public Constant {
public:
    friend Context;
    using value_type = f64;

    static bool classof(const Constant* constant) { return constant->kind() == Kind::Float; }

    static ConstantFloat* get(Context&, Type*, f64 value);

    f64 value() const { return m_value; }
    
private:
    ConstantFloat(Context* context, Type* type, f64 value) : Constant(context, Kind::Float, type), m_value(value) {}
    
    f64 m_value;
};

class ConstantString : public Constant {
public:
    friend Context;
    using value_type = String;

    static bool classof(const Constant* constant) { return constant->kind() == Kind::String; }

    static ConstantString* get(Context&, Type*, const String& value);

    String value() const { return m_value; }

private:
    ConstantString(Context* context, Type* type, String value) : Constant(context, Kind::String, type), m_value(move(value)) {}

    String m_value;
};

class ConstantArray : public Constant {
public:
    friend Context;
    using value_type = Vector<Constant*>;

    static bool classof(const Constant* constant) { return constant->kind() == Kind::Array; }

    static ConstantArray* get(Context&, Type*, const Vector<Constant*>& elements);

    Constant* at(size_t index) const {
        if (index >= m_elements.size()) {
            return nullptr;
        }

        return m_elements[index];
    }

    size_t size() const { return m_elements.size(); }
    Vector<Constant*> const& elements() const { return m_elements; }

private:
    ConstantArray(
        Context* context, Type* type, Vector<Constant*> elements
    ) : Constant(context, Kind::Array, type), m_elements(move(elements)) {}

    Vector<Constant*> m_elements;
};

class ConstantStruct : public Constant {
public:
    friend Context;
    using value_type = Vector<Constant*>;

    static bool classof(const Constant* constant) { return constant->kind() == Kind::Struct; }

    static ConstantStruct* get(Context&, Type*, const Vector<Constant*>& fields);

    Constant* at(size_t index) const {
        if (index >= m_fields.size()) {
            return nullptr;
        }

        return m_fields[index];
    }

    Vector<Constant*> const& fields() const { return m_fields; }

private:
    ConstantStruct(
        Context* context, Type* type, Vector<Constant*> fields
    ) : Constant(context, Kind::Struct, type), m_fields(move(fields)) {}

    Vector<Constant*> m_fields;
};


}