#pragma once

#include <quart/common.h>
#include <quart/casting.h>
#include <quart/language/types.h>

#include <quart/bytecode/value.h> 

namespace quart {
    class Context;
}

namespace quart::bytecode {

enum class ConstantKind {
    Int,
    Float,
    String,
    Array,
    Struct,
    Null
};

class Constant : public Value {
public:
    static bool classof(const Value* value) { return value->id() == Value::ConstantID; }

    Context* context() const { return m_context; }
    ConstantKind kind() const { return m_kind; }

protected:
    Constant(Context* context, ConstantKind kind, Type* type) : Value(type, Value::ConstantID), m_context(context), m_kind(kind) {}

private:
    Context* m_context;
    ConstantKind m_kind;
};

template<ConstantKind K, typename T>
class ConstantBase : public Constant {
public:
    using value_type = T;

    static bool classof(const Value* value) { return isa<Constant>(value) && classof(cast<Constant>(value)); }
    static bool classof(const Constant* constant) { return constant->kind() == K; }

    friend Context;
protected:
    ConstantBase(Context* context, Type* type) : Constant(context, K, type) {} 
};

class ConstantInt : public ConstantBase<ConstantKind::Int, u64> {
public:
    static ConstantInt* get(Context&, Type*, u64 value);
    
    u64 value() const { return m_value; }

    void print(std::ostream&) const override;

private:
    friend Context;
    ConstantInt(Context* context, Type* type, u64 value) : ConstantBase(context, type), m_value(value) {}
    
    u64 m_value;
};

class ConstantFloat : public ConstantBase<ConstantKind::Float, f64> {
public:
    static ConstantFloat* get(Context&, Type*, f64 value);

    f64 value() const { return m_value; }

    void print(std::ostream&) const override;
    
private:
    friend Context;
    ConstantFloat(Context* context, Type* type, f64 value) : ConstantBase(context, type), m_value(value) {}
    
    f64 m_value;
};

class ConstantString : public ConstantBase<ConstantKind::String, String> {
public:
    static ConstantString* get(Context&, Type*, const String& value);

    String value() const { return m_value; }

    void print(std::ostream&) const override;

private:
    friend Context;
    ConstantString(Context* context, Type* type, String value) : ConstantBase(context, type), m_value(move(value)) {}

    String m_value;
};

class ConstantStruct : public ConstantBase<ConstantKind::Struct, Vector<Constant*>> {
public:
    static ConstantStruct* get(Context&, Type*, const Vector<Constant*>& value);

    Constant* at(size_t index) const {
        return index >= m_elements.size() ? nullptr : m_elements[index];
    }

    size_t size() const { return m_elements.size(); }
    Vector<Constant*> const& elements() const { return m_elements; }

    void print(std::ostream&) const override;

private:
    friend Context;
    ConstantStruct(
        Context* context, Type* type, Vector<Constant*> elements
    ) : ConstantBase(context, type), m_elements(move(elements)) {}

    Vector<Constant*> m_elements;
};

class ConstantArray : public ConstantBase<ConstantKind::Array, Vector<Constant*>> {
public:
    static ConstantArray* get(Context&, Type*, const Vector<Constant*>& value);

    Constant* at(size_t index) const {
        return index >= m_elements.size() ? nullptr : m_elements[index];
    }

    size_t size() const { return m_elements.size(); }
    Vector<Constant*> const& elements() const { return m_elements; }

    void print(std::ostream&) const override;

private:
    friend Context;
    ConstantArray(
        Context* context, Type* type, Vector<Constant*> elements
    ) : ConstantBase(context, type), m_elements(move(elements)) {}

    Vector<Constant*> m_elements;
};

class ConstantNull : public ConstantBase<ConstantKind::Null, void> {
public:
    static ConstantNull* get(Context&, Type* type);

    void print(std::ostream&) const override;

private:
    friend Context;
    ConstantNull(Context* context, Type* type) : ConstantBase(context, type) {}
};


}