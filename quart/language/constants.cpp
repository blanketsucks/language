#include <quart/language/constants.h>
#include <quart/language/context.h>

namespace quart {

ConstantInt* ConstantInt::get(Context& context, Type* type, u64 value) {
    return context.create_int_constant(value, type);
}

ConstantFloat* ConstantFloat::get(Context& context, Type* type, f64 value) {
    return context.create_float_constant(value, type);
}

ConstantString* ConstantString::get(Context& context, Type* type, const String& value) {
    return context.create_string_constant(value, type);
}

ConstantArray* ConstantArray::get(Context& context, Type* type, const Vector<Constant*>& elements) {
    return context.create_array_constant(elements, type);
}

ConstantStruct* ConstantStruct::get(Context &context, Type *type, const Vector<Constant*>& fields) {
    return context.create_struct_constant(fields, type);
}

ConstantNull* ConstantNull::get(Context& context, Type* type) {
    return context.create_null_constant(type);
}

}