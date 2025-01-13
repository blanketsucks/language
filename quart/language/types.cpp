#include <quart/language/types.h>
#include <quart/language/context.h>
#include <quart/language/structs.h>

namespace quart {

bool Type::is_underlying_type_of(TypeKind kind) const {
    if (this->is_pointer()) {
        return this->get_pointee_type()->kind() == kind;
    } else if (this->is_reference()) {
        return this->get_reference_type()->kind() == kind;
    } else {
        return this->kind() == kind;
    }
}

Type* Type::underlying_type() {
    if (this->is_pointer()) {
        return this->get_pointee_type();
    } else if (this->is_reference()) {
        return this->get_reference_type();
    }

    return nullptr;
}

bool Type::can_safely_cast_to(Type* to) {
    Type* from = this;

    if (from == to) {
        return true;
    }
    
    if (to->is_enum()) {
        if (!from->is_enum()) return false; 
        return to->get_enum_name() == from->get_enum_name();
    }

    bool is_match_mutable = false;
    if ((from->is_mutable() && !to->is_mutable()) || (from->is_mutable() && to->is_mutable()) || (!from->is_mutable() && !to->is_mutable())) {
        is_match_mutable = true;
    }
    
    if (to->is_pointer() && from->is_pointer()) {
        from = from->get_pointee_type(); 
        to = to->get_pointee_type();

        if (from->is_void() || to->is_void()) {
            return is_match_mutable;
        }

        return is_match_mutable && from->can_safely_cast_to(to);
    } else if (from->is_reference() && to->is_pointer()) {
        from = from->get_reference_type();
        to = to->get_pointee_type();

        if (to->is_void()) {
            return is_match_mutable;
        }

        return is_match_mutable && from->can_safely_cast_to(to);
    } else if (from->is_reference() && to->is_reference()) {
        from = from->get_reference_type();
        to = to->get_reference_type();

        return is_match_mutable && (from == to);
    } else if (from->is_array() && to->is_array()) {
        if (to->get_array_size() != from->get_array_size()) {
            return false;
        }
    
        from = from->get_array_element_type();
        to = to->get_array_element_type();

        return from == to;
    } else if (from->is_struct() && to->is_struct()) {
        return to->get_struct_name() == from->get_struct_name();
    } else if (from->is_int() && to->is_int()) {
        if (to->get_int_bit_width() == 1) {
            return true;
        }

        return from->is_int_unsigned() == to->is_int_unsigned() && to->get_int_bit_width() >= from->get_int_bit_width();
    } else if (from->is_float() && to->is_float()) {
        return true;
    } else if (to->is_double() && (from->is_float() || from->is_double())) {
        return true;
    } else if (from->is_tuple() && to->is_tuple()) {
        return from->get_tuple_types() == to->get_tuple_types();
    }

    return false;
}

bool Type::is_mutable() const {
    if (this->kind() == TypeKind::Pointer) {
        return this->as<PointerType>()->is_mutable();
    } else if (this->kind() == TypeKind::Reference) {
        return this->as<ReferenceType>()->is_mutable();
    } else {
        return true;
    }
}

PointerType* Type::get_pointer_to(bool is_mutable) {
    return m_context->create_pointer_type(this, is_mutable);
}

ReferenceType* Type::get_reference_to(bool is_mutable) {
    return m_context->create_reference_type(this, is_mutable);
}

u32 Type::get_int_bit_width() const {
    return this->as<IntType>()->bit_width();
}

bool Type::is_int_unsigned() const {
    return this->as<IntType>()->is_unsigned();
}

Type* Type::get_pointee_type() const {
    return this->as<PointerType>()->pointee();
}

Type* Type::get_reference_type() const {
    return this->as<ReferenceType>()->reference_type();
}

size_t Type::get_pointer_depth() const {
    size_t depth = 0;
    const Type* type = this;

    while (type->is_pointer()) {
        type = type->get_pointee_type();
        depth++;
    }

    return depth;
}

Vector<Type*> const& Type::get_struct_fields() const {
    return this->as<StructType>()->fields();
}

Type* Type::get_struct_field_at(size_t index) const {
    return this->as<StructType>()->get_field_at(index);
}

String const& Type::get_struct_name() const {
    auto* type = this->as<StructType>();
    auto* structure = type->get_struct();

    if (structure) {
        return structure->qualified_name();
    }

    return type->name();
}

Type* Type::get_array_element_type() const {
    return this->as<ArrayType>()->element_type();
}

size_t Type::get_array_size() const {
    return this->as<ArrayType>()->size();
}

Vector<Type*> const& Type::get_tuple_types() const {
    return this->as<TupleType>()->types();
}

size_t Type::get_tuple_size() const {
    return this->as<TupleType>()->size();
}

Type* Type::get_tuple_element(size_t index) const {
    return this->as<TupleType>()->get_type_at(index);
}

Type* Type::get_inner_enum_type() const {
    return this->as<EnumType>()->inner();
}

String const& Type::get_enum_name() const {
    return this->as<EnumType>()->name();
}

Type* Type::get_function_return_type() const {
    return this->as<FunctionType>()->return_type();
}

Vector<Type*> const& Type::get_function_params() const {
    return this->as<FunctionType>()->parameters();
}

Type* Type::get_function_param(size_t index) const {
    return this->as<FunctionType>()->get_parameter_at(index);
}

bool Type::is_function_var_arg() const {
    return this->as<FunctionType>()->is_var_arg();
}

String Type::str() const {
    switch (this->kind()) {
        case TypeKind::Void: return "void";
        case TypeKind::Float: return "f32";
        case TypeKind::Double: return "f64";
        case TypeKind::Int: {
            bool is_unsigned = this->is_int_unsigned();
            u32 bits = this->get_int_bit_width();

            if (bits == 1) {
                return "bool";
            }

            return format("{0}{1}", is_unsigned ? "u" : "i", bits);
        }
        case TypeKind::Enum: return this->get_enum_name();
        case TypeKind::Struct: return this->get_struct_name();
        case TypeKind::Array: {
            String element = this->get_array_element_type()->str();
            size_t size = this->get_array_size();

            return format("[{0}; {1}]", element, size);
        }
        case TypeKind::Tuple: {
            Vector<String> types;
            for (auto& type : this->get_tuple_types()) {
                types.push_back(type->str());
            }

            return format("({0})", llvm::make_range(types.begin(), types.end()));
        }
        case TypeKind::Pointer: {
            String pointee = this->get_pointee_type()->str();
            bool is_mutable = this->is_mutable();

            return format("*{0}{1}", is_mutable ? "mut " : "", pointee);
        }
        case TypeKind::Reference: {
            String type = this->get_reference_type()->str();
            bool is_mutable = this->is_mutable();

            return format("&{0}{1}", is_mutable ? "mut " : "", type);
        }
        case TypeKind::Function: {
            String return_type = this->get_function_return_type()->str();

            Vector<String> params;
            for (auto& param : this->get_function_params()) {
                params.push_back(param->str());
            }

            if (this->is_function_var_arg()) {
                params.emplace_back("...");
            }

            return format("func({0}) -> {1}", llvm::make_range(params.begin(), params.end()), return_type);
        }
    }

    return "";
}

void Type::print() const {
    std::cout << this->str() << std::endl;
}

llvm::Type* Type::to_llvm_type(llvm::LLVMContext& context) const {
    switch (this->kind()) {
        case TypeKind::Void: return llvm::Type::getVoidTy(context);
        case TypeKind::Float: return llvm::Type::getFloatTy(context);
        case TypeKind::Double: return llvm::Type::getDoubleTy(context);
        case TypeKind::Int: {
            u32 bits = this->get_int_bit_width();
            return llvm::Type::getIntNTy(context, bits);
        }
        case TypeKind::Enum: {
            return this->get_inner_enum_type()->to_llvm_type(context);
        }
        case TypeKind::Struct: {
            const auto* type = this->as<StructType>();

            llvm::StructType* structure = type->get_llvm_struct_type();
            if (structure) {
                return structure;
            }

            Vector<llvm::Type*> fields;

            for (auto& field : type->fields()) {
                fields.push_back(field->to_llvm_type(context));
            }

            return llvm::StructType::get(context, fields);
        }
        case TypeKind::Array: {
            const auto* type = this->as<ArrayType>();

            llvm::Type* element = type->element_type()->to_llvm_type(context);
            size_t size = type->size();

            return llvm::ArrayType::get(element, size);
        }
        case TypeKind::Tuple: {
            const auto* type = this->as<TupleType>();

            Vector<llvm::Type*> types;
            for (auto& ty : type->types()) {
                types.push_back(ty->to_llvm_type(context));
            }

            return llvm::StructType::get(context, types);
        }
        case TypeKind::Pointer: {
            const auto* type = this->as<PointerType>();

            llvm::Type* pointee = type->get_pointee_type()->to_llvm_type(context);
            return llvm::PointerType::get(pointee, 0);
        }
        case TypeKind::Reference: {
            const auto* type = this->as<ReferenceType>();

            llvm::Type* reference = type->get_reference_type()->to_llvm_type(context);
            return llvm::PointerType::get(reference, 0);
        }
        case TypeKind::Function: {
            const auto* type = this->as<FunctionType>();
            llvm::Type* return_type = type->return_type()->to_llvm_type(context);

            Vector<llvm::Type*> params;
            for (auto& param : type->parameters()) {
                params.push_back(param->to_llvm_type(context));
            }

            return llvm::FunctionType::get(return_type, params, type->is_var_arg());
        }
        default:
            return nullptr;
    }
}

size_t Type::size() const {
    switch (this->kind()) {
        case TypeKind::Float:
            return 4;
        case TypeKind::Double:
            return 8;
        case TypeKind::Int:
            return this->get_int_bit_width() / 8;
        case TypeKind::Enum:
            return this->get_inner_enum_type()->size();
        case TypeKind::Struct: {
            size_t size = 0;
            for (auto& field : this->get_struct_fields()) {
                size += field->size();
            }

            return size;
        }
        case TypeKind::Array: {
            return this->get_array_element_type()->size() * this->get_array_size();
        }
        case TypeKind::Tuple: {
            size_t size = 0;
            for (auto& type : this->get_tuple_types()) {
                size += type->size();
            }

            return size;
        }
        case TypeKind::Pointer:
        case TypeKind::Reference: // FIXME: Use Target::word_size()
            return 8;
        default: break;
    }

    return 0;
}

IntType* IntType::get(Context& context, u32 bit_width, bool is_unsigned) {
    return context.create_int_type(bit_width, is_unsigned);
}

StructType* StructType::get(Context& context, const String& name, const Vector<Type*>& fields, llvm::StructType* type) {
    return context.create_struct_type(name, fields, type);
}

ArrayType* ArrayType::get(Context& context, Type* element, size_t size) {
    return context.create_array_type(element, size);
}

TupleType* TupleType::get(Context& context, const Vector<Type*>& types) {
    return context.create_tuple_type(types);
}

PointerType* PointerType::get(Context& context, Type* pointee, bool is_mutable) {
    return context.create_pointer_type(pointee, is_mutable);
}

ReferenceType* ReferenceType::get(Context& context, Type* type, bool is_mutable) {
    return context.create_reference_type(type, is_mutable);
}

EnumType* EnumType::get(Context& context, const String& name, Type* inner) {
    return context.create_enum_type(name, inner);
}

FunctionType* FunctionType::get(Context& context, Type* return_type, const Vector<Type*>& params, bool is_var_arg) {
    return context.create_function_type(return_type, params, is_var_arg);
}

void StructType::set_fields(const Vector<Type*>& fields) {
    m_fields = fields;
}

PointerType* PointerType::as_const() {
    if (!this->is_mutable()) {
        return this;
    }

    return m_context->create_pointer_type(m_pointee, false);
}

PointerType* PointerType::as_mutable() {
    if (this->is_mutable()) {
        return this;
    }

    return m_context->create_pointer_type(m_pointee, true);
}

ReferenceType* ReferenceType::as_const() {
    if (!this->is_mutable()) {
        return this;
    }

    return m_context->create_reference_type(m_type, false);
}

ReferenceType* ReferenceType::as_mutable() {
    if (this->is_mutable()) {
        return this;
    }

    return m_context->create_reference_type(m_type, true);
}

}