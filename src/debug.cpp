#include <quart/debug.h>
#include <quart/visitor.h>

using namespace quart;

// llvm::DIType* DebugInfo::wrap(quart::Type* type) {
//     if (this->types.find(type) != this->types.end()) {
//         return this->types[type];
//     }

//     llvm::dwarf::TypeKind kind;
//     switch (type->getTypeID()) {
//         case llvm::Type::PointerTyID: {
//             quart::Type* inner = type->get_pointee_type();

//             llvm::DIType* ty = this->dbuilder->createPointerType(
//                 this->wrap(inner), 
//                 inner->getPrimitiveSizeInBits(),
//                 0, 
//                 0, 
//                 type->get_as_string()
//             );

//             this->types[type] = ty;
//             return ty;
//         } case llvm::Type::ArrayTyID: {
//             quart::Type* inner = type->get_array_element_type();
//             u64 size = type->get_array_size();

//             llvm::DIType* ty = this->dbuilder->createArrayType(
//                 size, type->getPrimitiveSizeInBits(), this->wrap(inner), {}
//             );

//             this->types[type] = ty;
//             return ty;
//         } case llvm::Type::StructTyID: {
//             quart::StructType* structure = type->as<quart::StructType>();
//             std::vector<llvm::Metadata*> elements;

//             for (quart::Type* element : structure->elements()) {
//                 elements.push_back(this->wrap(element));
//             }

//             llvm::DIType* ty = this->dbuilder->createStructType(
//                 this->unit, 
//                 type->get_as_string(), 
//                 this->file, 
//                 0, 
//                 type->getPrimitiveSizeInBits(), 
//                 0, 
//                 llvm::DINode::DIFlags::FlagZero, 
//                 nullptr, 
//                 this->dbuilder->getOrCreateArray(elements)
//             );

//             this->types[type] = ty;
//             return ty;
//         } case llvm::Type::FunctionTyID: {
//             llvm::FunctionType* function = llvm::cast<llvm::FunctionType>(type);
//             std::vector<llvm::Metadata*> elements;

//             for (llvm::Type* element : function->params()) {
//                 elements.push_back(this->wrap(element));
//             }

//             llvm::DIType* ty = this->dbuilder->createSubroutineType(
//                 this->dbuilder->getOrCreateTypeArray(elements)
//             );

//             this->types[type] = ty;
//             return ty;
//         } case llvm::Type::IntegerTyID: {
//             kind = llvm::dwarf::DW_ATE_signed; // Right now we do now have unsigned integers so everything is signed
//             break;
//         } case llvm::Type::FloatTyID:
//           case llvm::Type::DoubleTyID: {
//             kind = llvm::dwarf::DW_ATE_float;
//             break;
//         }
//         default:
//             __builtin_unreachable();
//     }

//     llvm::DIType* ty = this->dbuilder->createBasicType(
//         type->get_as_string(), type->getPrimitiveSizeInBits(), kind
//     );

//     this->types[type] = ty;
//     return ty;
// }

// void DebugInfo::emit(ast::Expr* expr) {
//     if (!expr) {
//         return this->builder->SetCurrentDebugLocation(llvm::DebugLoc());
//     }

//     llvm::DIScope* scope = nullptr;
//     if (this->scopes.empty()) {
//         scope = this->unit;
//     } else {
//         scope = this->scopes.back();
//     }

//     llvm::DILocation* location = llvm::DILocation::get(
//         scope->getContext(), 
//         expr->span.start.line, 
//         expr->span.start.column, 
//         scope
//     );

//     this->builder->SetCurrentDebugLocation(location);
// }

// llvm::DISubprogram* DebugInfo::create_function(
//     llvm::DIScope* scope, 
//     llvm::Function* function, 
//     const std::string& name,
//     u32 line
// ) {
//     auto type = llvm::cast<llvm::DISubroutineType>(this->wrap(function->getFunctionType()));
//     llvm::DISubprogram* subprogram = this->dbuilder->createFunction(
//         scope, 
//         name, 
//         llvm::StringRef(), 
//         this->file, 
//         line,
//         type,
//         line,
//         llvm::DINode::FlagZero,
//         llvm::DISubprogram::SPFlagDefinition | llvm::DISubprogram::SPFlagLocalToUnit
//     );

//     function->setSubprogram(subprogram);
//     return subprogram;
// }