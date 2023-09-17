#include <quart/language/types.h>
#include <quart/language/registry.h>
#include <quart/logging.h>

#include <iostream>

using namespace quart;

TypeKind Type::kind() const { return this->_kind; }

bool Type::can_safely_cast_to(Type* from, Type* to) {
    if (from == to) {
        return true;
    }
    
    if (to->is_enum()) {
        if (!from->is_enum()) return false; 
        return to->get_enum_name() == from->get_enum_name();
    }
    
    if (to->is_pointer() && from->is_pointer()) {
        bool is_match_mutable = false;
        if ((from->is_mutable() && !to->is_mutable()) || (from->is_mutable() && to->is_mutable())) {
            is_match_mutable = true;
        }

        from = from->get_pointee_type(); 
        to = to->get_pointee_type();

        if (from->is_void() || to->is_void()) {
            return is_match_mutable;
        }

        return is_match_mutable && Type::can_safely_cast_to(from, to);
    } else if (from->is_reference() && to->is_pointer()) {
        bool is_match_mutable = false;
        if ((from->is_mutable() && !to->is_mutable()) || (from->is_mutable() && to->is_mutable())) {
            is_match_mutable = true;
        }

        from = from->get_reference_type();
        to = to->get_pointee_type();

        if (to->is_void()) return is_match_mutable;
        return is_match_mutable && Type::can_safely_cast_to(from, to);
    } else if (from->is_reference() && to->is_reference()) {
        bool is_match_mutable = false;
        if ((from->is_mutable() && !to->is_mutable()) || (from->is_mutable() && to->is_mutable())) {
            is_match_mutable = true;
        }

        from = from->get_reference_type();
        to = to->get_reference_type();

        return is_match_mutable && (from == to);
    } else if (from->is_array() && to->is_array()) {
        if (to->get_array_size() != from->get_array_size()) {
            return false;
        }

        return Type::can_safely_cast_to(from->get_array_element_type(), to->get_array_element_type());
    } else if (from->is_struct() && to->is_struct()) {
        return to->get_struct_name() == from->get_struct_name();
    } else if (from->is_int() && to->is_int()) {
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

TypeRegistry* Type::get_type_registry() const { return this->registry; }

PointerType* Type::get_pointer_to(bool is_mutable) {
    return this->registry->create_pointer_type(this, is_mutable);
}

ReferenceType* Type::get_reference_to(bool is_mutable) {
    return this->registry->create_reference_type(this, is_mutable);
}

uint32_t Type::get_int_bit_width() const {
    return this->as<IntType>()->get_bit_width();
}

bool Type::is_int_unsigned() const {
    return this->as<IntType>()->is_unsigned();
}

Type* Type::get_pointee_type() const {
    return this->as<PointerType>()->get_pointee_type();
}

Type* Type::get_reference_type() const {
    return this->as<ReferenceType>()->get_reference_type();
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

std::vector<Type*> Type::get_struct_fields() const {
    return this->as<StructType>()->get_fields();
}

Type* Type::get_struct_field_at(size_t index) const {
    return this->as<StructType>()->get_field_at(index);
}

std::string Type::get_struct_name() const { return this->as<StructType>()->get_name(); }

Type* Type::get_array_element_type() const {
    return this->as<ArrayType>()->get_element_type();
}

size_t Type::get_array_size() const {
    return this->as<ArrayType>()->get_size();
}

std::vector<Type*> Type::get_tuple_types() const {
    return this->as<TupleType>()->get_types();
}

size_t Type::get_tuple_size() const {
    return this->as<TupleType>()->get_size();
}

Type* Type::get_tuple_element(size_t index) const {
    return this->as<TupleType>()->get_type_at(index);
}

Type* Type::get_inner_enum_type() const {
    return this->as<EnumType>()->get_inner_type();
}

std::string Type::get_enum_name() const {
    return this->as<EnumType>()->get_name();
}

Type* Type::get_function_return_type() const {
    return this->as<FunctionType>()->get_return_type();
}

std::vector<Type*> Type::get_function_params() const {
    return this->as<FunctionType>()->get_parameter_types();
}

Type* Type::get_function_param(size_t index) const {
    return this->as<FunctionType>()->get_parameter_at(index);
}

std::string Type::get_as_string() const {
    switch (this->kind()) {
        case TypeKind::Void: return "void";
        case TypeKind::Float: return "f32";
        case TypeKind::Double: return "f64";
        case TypeKind::Int: {
            bool is_unsigned = this->is_int_unsigned();
            uint32_t bits = this->get_int_bit_width();

            if (bits == 1) return "bool";

            return FORMAT("{0}{1}", is_unsigned ? "u" : "i", bits);
        }
        case TypeKind::Enum: return this->get_enum_name();
        case TypeKind::Struct: return this->get_struct_name();
        case TypeKind::Array: {
            std::string element = this->get_array_element_type()->get_as_string();
            size_t size = this->get_array_size();

            return FORMAT("[{0}; {1}]", element, size);
        }
        case TypeKind::Tuple: {
            std::vector<std::string> types;
            for (auto& type : this->get_tuple_types()) {
                types.push_back(type->get_as_string());
            }

            return FORMAT("({0})", llvm::make_range(types.begin(), types.end()));
        }
        case TypeKind::Pointer: {
            std::string pointee = this->get_pointee_type()->get_as_string();
            bool is_mutable = this->is_mutable();

            return FORMAT("*{0}{1}", is_mutable ? "mut " : "", pointee);
        }
        case TypeKind::Reference: {
            std::string type = this->get_reference_type()->get_as_string();
            bool is_mutable = this->is_mutable();

            return FORMAT("&{0}{1}", is_mutable ? "mut " : "", type);
        }
        case TypeKind::Function: {
            std::string return_type = this->get_function_return_type()->get_as_string();

            std::vector<std::string> params;
            for (auto& param : this->get_function_params()) {
                params.push_back(param->get_as_string());
            }

            return FORMAT("func({0}) -> {1}", llvm::make_range(params.begin(), params.end()), return_type);
        }
    }

    return "";
}

void Type::print() const { std::cout << this->get_as_string() << std::endl; }

llvm::Type* Type::to_llvm_type() const {
    llvm::LLVMContext& context = this->registry->get_context();
    switch (this->kind()) {
        case TypeKind::Void: return llvm::Type::getVoidTy(context);
        case TypeKind::Float: return llvm::Type::getFloatTy(context);
        case TypeKind::Double: return llvm::Type::getDoubleTy(context);
        case TypeKind::Int: {
            uint32_t bits = this->get_int_bit_width();
            return llvm::Type::getIntNTy(context, bits);
        }
        case TypeKind::Enum: {
            return this->get_inner_enum_type()->to_llvm_type();
        }
        case TypeKind::Struct: {
            const StructType* type = this->as<StructType>();
            llvm::StructType* structure = type->get_llvm_struct_type();

            if (structure) return structure;
            std::vector<llvm::Type*> fields;

            for (auto& field : type->get_fields()) {
                fields.push_back(field->to_llvm_type());
            }

            return llvm::StructType::get(context, fields);
        }
        case TypeKind::Array: {
            const ArrayType* type = this->as<ArrayType>();

            llvm::Type* element = type->get_element_type()->to_llvm_type();
            size_t size = type->get_size();

            return llvm::ArrayType::get(element, size);
        }
        case TypeKind::Tuple: {
            const TupleType* type = this->as<TupleType>();

            std::vector<llvm::Type*> types;
            for (auto& type : type->get_types()) {
                types.push_back(type->to_llvm_type());
            }

            return llvm::StructType::get(context, types);
        }
        case TypeKind::Pointer: {
            const PointerType* type = this->as<PointerType>();

            llvm::Type* pointee = type->get_pointee_type()->to_llvm_type();
            return llvm::PointerType::get(pointee, 0);
        }
        case TypeKind::Reference: {
            const ReferenceType* type = this->as<ReferenceType>();

            llvm::Type* reference = type->get_reference_type()->to_llvm_type();
            return llvm::PointerType::get(reference, 0);
        }
        case TypeKind::Function: {
            const FunctionType* type = this->as<FunctionType>();

            llvm::Type* return_type = type->get_return_type()->to_llvm_type();

            std::vector<llvm::Type*> params;
            for (auto& param : type->get_parameter_types()) {
                params.push_back(param->to_llvm_type());
            }

            return llvm::FunctionType::get(return_type, params, false);
        }
        default:
            return nullptr;
    }
}

bool IntType::is_boolean_type() const { return this->bits == 1; }
uint32_t IntType::get_bit_width() const { return this->bits; }
bool IntType::is_unsigned() const { return !this->is_signed; }

std::vector<Type*> StructType::get_fields() const { return this->fields; }
std::string StructType::get_name() const { return this->name; }
llvm::StructType* StructType::get_llvm_struct_type() const { return this->type; }

Type* StructType::get_field_at(size_t index) const {
    if (index >= this->fields.size()) {
        return nullptr;
    }

    return this->fields[index];
}

void StructType::set_fields(const std::vector<Type*>& fields) {
    this->fields = fields;
}

Type* ArrayType::get_element_type() const { return this->element; }
size_t ArrayType::get_size() const { return this->size; }

std::vector<Type*> TupleType::get_types() const { return this->types; }
size_t TupleType::get_size() const { return this->types.size(); }

Type* TupleType::get_type_at(size_t index) const {
    if (index >= this->types.size()) {
        return nullptr;
    }

    return this->types[index];
}

Type* PointerType::get_pointee_type() const { return this->pointee; }
bool PointerType::is_mutable() const { return !this->is_immutable; }

PointerType* PointerType::get_as_const() {
    if (!this->is_mutable()) {
        return this;
    }

    return this->registry->create_pointer_type(this->pointee, false);
}

PointerType* PointerType::get_as_mutable() {
    if (this->is_mutable()) {
        return this;
    }

    return this->registry->create_pointer_type(this->pointee, true);
}

Type* ReferenceType::get_reference_type() const { return this->type; }
bool ReferenceType::is_mutable() const { return !this->is_immutable; }

ReferenceType* ReferenceType::get_as_const() {
    if (!this->is_mutable()) {
        return this;
    }

    return this->registry->create_reference_type(this->type, false);
}

ReferenceType* ReferenceType::get_as_mutable() {
    if (this->is_mutable()) {
        return this;
    }

    return this->registry->create_reference_type(this->type, true);
}

Type* EnumType::get_inner_type() const { return this->inner; }
std::string EnumType::get_name() const { return this->name; }

Type* FunctionType::get_return_type() const { return this->return_type; }
std::vector<Type*> FunctionType::get_parameter_types() const { return this->params; }

Type* FunctionType::get_parameter_at(size_t index) const {
    if (index >= this->params.size()) {
        return nullptr;
    }

    return this->params[index];
}