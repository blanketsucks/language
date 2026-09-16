#include <quart/bytecode/constant.h>
#include <quart/language/context.h>
#include <quart/format.h>

namespace quart::bytecode {

ConstantInt* ConstantInt::get(Context& context, Type* type, u64 value) {
    return context.create_int_constant(value, type);
}

void ConstantInt::print(std::ostream& stream) const {
    stream << format("{} {}", type()->str(), m_value);
}

ConstantFloat* ConstantFloat::get(Context& context, Type* type, f64 value) {
    return context.create_float_constant(value, type);
}

void ConstantFloat::print(std::ostream& stream) const {
    stream << format("{} {}", type()->str(), m_value);
}

ConstantString* ConstantString::get(Context& context, Type* type, const String& value) {
    return context.create_string_constant(value, type);
}

void ConstantString::print(std::ostream& stream) const {
    stream << "\"" << escape(m_value) << "\"";
}

ConstantArray* ConstantArray::get(Context& context, Type* type, const Vector<Constant*>& elements) {
    return context.create_array_constant(elements, type);
}

void ConstantArray::print(std::ostream& stream) const {
    stream << "{}";
}

ConstantStruct* ConstantStruct::get(Context& context, Type* type, const Vector<Constant*>& elements) {
    return context.create_struct_constant(elements, type);
}

void ConstantStruct::print(std::ostream& stream) const {
    stream << "{}";
}

ConstantNull* ConstantNull::get(Context& context, Type* type) {
    return context.create_null_constant(type);
}

void ConstantNull::print(std::ostream& stream) const {
    stream << format("{} null", type()->str());
}

}