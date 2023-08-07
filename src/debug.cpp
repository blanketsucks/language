#include <quart/debug.h>
#include <quart/visitor.h>

llvm::DIType* DebugInfo::wrap(llvm::Type* type) {
    if (this->types.find(type) != this->types.end()) {
        return this->types[type];
    }

    llvm::dwarf::TypeKind kind;
    switch (type->getTypeID()) {
        case llvm::Type::PointerTyID: {
            llvm::Type* inner = type->getPointerElementType();

            llvm::DIType* ty = this->dbuilder->createPointerType(
                this->wrap(inner), 
                inner->getPrimitiveSizeInBits(),
                0, 
                type->getPointerAddressSpace(), 
                Visitor::get_type_name(type)
            );

            this->types[type] = ty;
            return ty;
        } case llvm::Type::ArrayTyID: {
            llvm::Type* inner = type->getArrayElementType();
            uint64_t size = type->getArrayNumElements();

            llvm::DIType* ty = this->dbuilder->createArrayType(
                size, type->getPrimitiveSizeInBits(), this->wrap(inner), {}
            );

            this->types[type] = ty;
            return ty;
        } case llvm::Type::StructTyID: {
            llvm::StructType* structure = llvm::cast<llvm::StructType>(type);
            std::vector<llvm::Metadata*> elements;

            for (llvm::Type* element : structure->elements()) {
                elements.push_back(this->wrap(element));
            }

            llvm::DIType* ty = this->dbuilder->createStructType(
                this->unit, 
                Visitor::get_type_name(type), 
                this->file, 
                0, 
                type->getPrimitiveSizeInBits(), 
                0, 
                llvm::DINode::DIFlags::FlagZero, 
                nullptr, 
                this->dbuilder->getOrCreateArray(elements)
            );

            this->types[type] = ty;
            return ty;
        } case llvm::Type::FunctionTyID: {
            llvm::FunctionType* function = llvm::cast<llvm::FunctionType>(type);
            std::vector<llvm::Metadata*> elements;

            for (llvm::Type* element : function->params()) {
                elements.push_back(this->wrap(element));
            }

            llvm::DIType* ty = this->dbuilder->createSubroutineType(
                this->dbuilder->getOrCreateTypeArray(elements)
            );

            this->types[type] = ty;
            return ty;
        } case llvm::Type::IntegerTyID: {
            kind = llvm::dwarf::DW_ATE_signed; // Right now we do now have unsigned integers so everything is signed
            break;
        } case llvm::Type::FloatTyID:
          case llvm::Type::DoubleTyID: {
            kind = llvm::dwarf::DW_ATE_float;
            break;
        }
        default:
            __builtin_unreachable();
    }

    llvm::DIType* ty = this->dbuilder->createBasicType(
        Visitor::get_type_name(type), type->getPrimitiveSizeInBits(), kind
    );

    this->types[type] = ty;
    return ty;
}

void DebugInfo::emit(ast::Expr* expr) {
    if (!expr) {
        return this->builder->SetCurrentDebugLocation(llvm::DebugLoc());
    }

    llvm::DIScope* scope = this->unit;
    llvm::DILocation* location = llvm::DILocation::get(
        this->unit->getContext(), 
        expr->span.start.line, 
        expr->span.start.column, 
        scope
    );

    this->builder->SetCurrentDebugLocation(location);
}