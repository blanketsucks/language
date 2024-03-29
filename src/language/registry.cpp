#include <quart/language/registry.h>

// NOLINTBEGIN(cppcoreguidelines-owning-memory)

using namespace quart;

TypeRegistry::TypeRegistry(
    llvm::LLVMContext& context
) : context(context), void_type(this, TypeKind::Void), f32(this, TypeKind::Float), f64(this, TypeKind::Double),
    i1(this, 1, true), i8(this, 8, true), i16(this, 16, true), i32(this, 32, true), i64(this, 64, true),
    u8(this, 8, false), u16(this, 16, false), u32(this, 32, false), u64(this, 64, false) {}

OwnPtr<TypeRegistry> TypeRegistry::create(llvm::LLVMContext& context) {
    return OwnPtr<TypeRegistry>(new TypeRegistry(context));
}

quart::Type* TypeRegistry::wrap(llvm::Type* type) {
    switch (type->getTypeID()) {
        case llvm::Type::VoidTyID:
            return &this->void_type;
        case llvm::Type::FloatTyID:
            return &this->f32;
        case llvm::Type::DoubleTyID:
            return &this->f64;
        case llvm::Type::IntegerTyID:
            return this->create_int_type(type->getIntegerBitWidth(), true);
        case llvm::Type::StructTyID:
            return this->wrap(llvm::cast<llvm::StructType>(type));
        case llvm::Type::ArrayTyID: {
            quart::Type* element = this->wrap(type->getArrayElementType());
            size_t size = type->getArrayNumElements();

            return this->create_array_type(element, size);
        }
        case llvm::Type::PointerTyID: {
            quart::Type* pointee = this->wrap(type->getPointerElementType());
            return this->create_pointer_type(pointee, false);
        }
        default: return nullptr;
    }
}

quart::StructType* TypeRegistry::wrap(llvm::StructType* type) {
    std::string name = type->getName().str();
    auto iterator = this->structs.find(name);

    if (iterator != this->structs.end()) {
        return &*iterator->second;
    }

    std::vector<quart::Type*> fields;
    for (llvm::Type* field : type->elements()) {
        fields.push_back(this->wrap(field));
    }

    auto ty = new StructType(this, name, fields, type);
    this->structs[name] = OwnPtr<StructType>(ty);

    return ty;
}

IntType* TypeRegistry::create_int_type(::u32 bits, bool is_signed) {
    if (bits == 1) {
        return &this->i1;
    } else if (bits == 8) {
        return is_signed ? &this->i8 : &this->u8;
    } else if (bits == 16) {
        return is_signed ? &this->i16 : &this->u16;
    } else if (bits == 32) {
        return is_signed ? &this->i32 : &this->u32;
    } else if (bits == 64) {
        return is_signed ? &this->i64 : &this->u64;
    } else {
        auto iterator = this->integers.find({bits, is_signed});
        if (iterator != this->integers.end()) {
            return &*iterator->second;
        } else {
            auto type = new IntType(this, bits, is_signed);
            this->integers[{bits, is_signed}] = OwnPtr<IntType>(type);

            return type;
        }
    }
}

StructType* TypeRegistry::create_struct_type(
    const std::string& name, const std::vector<Type*>& fields, llvm::StructType* llvm_type
) {
    auto iterator = this->structs.find(name);
    if (iterator != this->structs.end()) {
        return &*iterator->second;
    } else {
        auto type = new StructType(this, name, fields, llvm_type);
        this->structs[name] = OwnPtr<StructType>(type);

        return type;
    }
}

ArrayType* TypeRegistry::create_array_type(Type* element, size_t size) {
    auto iterator = this->arrays.find({element, size});
    if (iterator != this->arrays.end()) {
        return &*iterator->second;
    } else {
        auto type = new ArrayType(this, element, size);
        this->arrays[{element, size}] = OwnPtr<ArrayType>(type);

        return type;
    }
}

EnumType* TypeRegistry::create_enum_type(const std::string& name, Type* inner) {
    auto iterator = this->enums.find(name);
    if (iterator != this->enums.end()) {
        return &*iterator->second;
    } else {
        auto type = new EnumType(this, name, inner);
        this->enums[name] = OwnPtr<EnumType>(type);

        return type;
    }
}

TupleType* TypeRegistry::create_tuple_type(const std::vector<Type*>& elements) {
    auto iterator = this->tuples.find(elements);
    if (iterator != this->tuples.end()) {
        return &*iterator->second;
    } else {
        auto type = new TupleType(this, elements);
        this->tuples[elements] = OwnPtr<TupleType>(type);

        return type;
    }
}

PointerType* TypeRegistry::create_pointer_type(Type* pointee, bool is_mutable) {
    auto iterator = this->pointers.find({pointee, is_mutable});
    if (iterator != this->pointers.end()) {
        return &*iterator->second;
    } else {
        auto type = new PointerType(this, pointee, is_mutable);
        this->pointers[{pointee, is_mutable}] = OwnPtr<PointerType>(type);

        return type;
    }
}

ReferenceType* TypeRegistry::create_reference_type(Type* type, bool is_mutable) {
    auto iterator = this->references.find({type, is_mutable});
    if (iterator != this->references.end()) {
        return &*iterator->second;
    } else {
        auto reference = new ReferenceType(this, type, is_mutable);
        this->references[{type, is_mutable}] = OwnPtr<ReferenceType>(reference);

        return reference;
    }
}

FunctionType* TypeRegistry::create_function_type(
    Type* return_type, const std::vector<Type*>& parameters
) {
    auto iterator = this->functions.find({return_type, parameters});
    if (iterator != this->functions.end()) {
        return &*iterator->second;
    } else {
        auto type = new FunctionType(this, return_type, parameters);
        this->functions[{return_type, parameters}] = OwnPtr<FunctionType>(type);

        return type;
    }
}

quart::Type* TypeRegistry::get_void_type() { return &this->void_type; }
quart::Type* TypeRegistry::get_f32_type() { return &this->f32; }
quart::Type* TypeRegistry::get_f64_type() { return &this->f64; }

// NOLINTEND(cppcoreguidelines-owning-memory)