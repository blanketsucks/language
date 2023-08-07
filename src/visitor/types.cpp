#include <quart/utils/utils.h>
#include <quart/objects/values.h>
#include <quart/parser/ast.h>
#include <quart/visitor.h>
#include <quart/utils/string.h>

#include <llvm/IR/Instructions.h>

#include <functional>

#define LLVM_MAX_INT_BITS (2 << (23 - 1)) // 2^23

static std::map<std::string, uint32_t> TYPE_SIZES = {
    {"i8", 1},
    {"i16", 2},
    {"i32", 4},
    {"i64", 8},
    {"i128", 16},
    {"f32", 4},
    {"f64", 8},
    {"bool", 1},
    {"char", 1},
    {"void", 0}
};

uint32_t Visitor::get_pointer_depth(llvm::Type* type) {
    uint32_t depth = 0;
    while (type->isPointerTy()) {
        type = type->getPointerElementType();
        depth++;
    }

    return depth;
} 

uint32_t Visitor::getsizeof(llvm::Value* value) {
    return this->getsizeof(value->getType());
}

uint32_t Visitor::getallocsize(llvm::Type* type) {
    llvm::TypeSize tsize = this->module->getDataLayout().getTypeAllocSize(type);
    return tsize.getFixedSize();
}

uint32_t Visitor::getsizeof(llvm::Type* type) {
    if (type->isPointerTy() || type->isStructTy()) {
        return this->getallocsize(type);
    } else if (type->isArrayTy()) {
        return type->getArrayNumElements() * this->getsizeof(type->getArrayElementType());
    }

    return type->getPrimitiveSizeInBits() / 8;
}

std::string Visitor::get_type_name(Type type) {
    return Visitor::get_type_name(type.value);
}

std::string Visitor::get_type_name(llvm::Type* type) {
    switch (type->getTypeID()) {
        case llvm::Type::VoidTyID: return "void";
        case llvm::Type::FloatTyID: return "f32";
        case llvm::Type::DoubleTyID: return "f64";
        case llvm::Type::IntegerTyID: {
            uint32_t bits = type->getIntegerBitWidth();
            if (bits == 1) {
                return "bool";
            } else if (bits > 128) {
                return FORMAT("int({0})", bits);
            } else {
                return FORMAT("i{0}", bits);
            }
        }
        case llvm::Type::PointerTyID: return "*" + Visitor::get_type_name(type->getPointerElementType());
        case llvm::Type::ArrayTyID: {
            std::string name = Visitor::get_type_name(type->getArrayElementType());
            return FORMAT("[{0}; {1}]", name, type->getArrayNumElements());
        }
        case llvm::Type::StructTyID: {
            llvm::StructType* ty = llvm::cast<llvm::StructType>(type);
            llvm::StringRef name = ty->getName();

            if (name.startswith("__tuple")) {
                std::vector<std::string> names;
                for (auto& field : ty->elements()) {
                    names.push_back(Visitor::get_type_name(field));
                }

                return FORMAT("({0})", llvm::make_range(names.begin(), names.end()));
            } else if (name.startswith("__variadic")) {
                llvm::Type* element = ty->getElementType(0);
                return FORMAT("({0}, ...)", Visitor::get_type_name(element));
            }

            return utils::replace(name.str(), ".", "::");
        }
        case llvm::Type::FunctionTyID: {
            llvm::FunctionType* ty = llvm::cast<llvm::FunctionType>(type);
            std::string ret = Visitor::get_type_name(ty->getReturnType());

            std::vector<std::string> args;
            for (auto& arg : ty->params()) {
                args.push_back(Visitor::get_type_name(arg));
            }

            if (ty->isVarArg()) {
                args.push_back("...");
            }

            return FORMAT("func({0}) -> {1}", llvm::make_range(args.begin(), args.end()), ret);
        }
        default: return "";
    }
}

bool Visitor::is_compatible(Type t1, llvm::Type* t2) {
    return this->is_compatible(t1.value, t2);
}

bool Visitor::is_compatible(llvm::Type* t1, llvm::Type* t2) {
    if (t1 == t2) {
        return true;
    }

    if (t1->isPointerTy()) {
        if (!t2->isPointerTy()) {
            if (t2->isArrayTy()) {
                return this->is_compatible(t1->getPointerElementType(), t2->getArrayElementType());
            }
            
            return false;
        }

        t2 = t2->getPointerElementType(); t1 = t1->getPointerElementType(); 
        if (t2->isVoidTy() || t1->isVoidTy()) {
            return true;
        }

        return this->is_compatible(t1, t2);
    } else if (t1->isArrayTy()) {
        if (!t2->isArrayTy()) { 
            if (!t2->isPointerTy()) { return false; }

            t2 = t2->getPointerElementType();
            return this->is_compatible(t1->getArrayElementType(), t2);
        }

        if (t1->getArrayNumElements() != t2->getArrayNumElements()) {
            return false;
        }

        return this->is_compatible(t1->getArrayElementType(), t2->getArrayElementType());
    } else if (t1->isStructTy()) {
        if (!t2->isStructTy()) { return false; }

        // each name is unique so we can just compare their names.
        return t1->getStructName() == t2->getStructName();
    }

    if (t1->isIntegerTy()) {
        if (t2->isFloatingPointTy() || t2->isIntegerTy()) {
            return true;
        } else {
            return false;
        }
    } else if (t1->isFloatingPointTy()) {
        return t2->isFloatingPointTy();
    }

    return false;
}

llvm::Value* Visitor::cast(llvm::Value* value, Type type) {
    return this->cast(value, type.value);
}

llvm::Value* Visitor::cast(llvm::Value* value, llvm::Type* type) {
    if (value->getType() == type) {
        return value;
    }

    if (llvm::isa<llvm::Constant>(value)) {
        llvm::Constant* constant = llvm::cast<llvm::Constant>(value);
        
        if (llvm::isa<llvm::ConstantInt>(constant)) {
            llvm::ConstantInt* cint = llvm::cast<llvm::ConstantInt>(constant);
            return llvm::ConstantInt::get(type, cint->getZExtValue());
        } else if (llvm::isa<llvm::ConstantFP>(constant)) {
            llvm::ConstantFP* cfp = llvm::cast<llvm::ConstantFP>(constant);
            return llvm::ConstantFP::get(type, cfp->getValueAPF().convertToDouble());
        }

        return constant;
    }

    llvm::Type* from = value->getType();
    if (from->isIntegerTy() && type->isIntegerTy()) {
        return this->builder->CreateIntCast(value, type, true);
    } else if (from->isArrayTy() && type->isPointerTy()) {
        assert(from->getArrayElementType() == type->getPointerElementType());
        value = this->as_reference(value);
    } else if (from->isFloatingPointTy() && type->isFloatingPointTy()) {
        return this->builder->CreateFPCast(value, type);
    }

    return this->builder->CreateBitCast(value, type);
}

bool Visitor::is_valid_sized_type(llvm::Type* type) {
    // There is no way to get a non-pointer function type currently but it's good to check for it regardless.
    return !type->isVoidTy() && !type->isFunctionTy();
}

llvm::Type* Visitor::get_builtin_type(ast::BuiltinType value) {
    switch (value) {
        case ast::BuiltinType::Void:
            return this->builder->getVoidTy();
        case ast::BuiltinType::Bool:
            return this->builder->getInt1Ty();
        case ast::BuiltinType::i8:
            return this->builder->getInt8Ty();
        case ast::BuiltinType::i16:
            return this->builder->getInt16Ty();
        case ast::BuiltinType::i32:
            return this->builder->getInt32Ty();
        case ast::BuiltinType::i64:
            return this->builder->getInt64Ty();
        case ast::BuiltinType::i128:
            return this->builder->getIntNTy(128);
        case ast::BuiltinType::f32:
            return this->builder->getFloatTy();
        case ast::BuiltinType::f64:
            return this->builder->getDoubleTy();
        default: __UNREACHABLE
    }
}

Value Visitor::visit(ast::CastExpr* expr) {
    llvm::Value* value = expr->value->accept(*this).unwrap(expr->span);

    llvm::Type* from = value->getType();
    llvm::Type* to = expr->to->accept(*this).value;

    if (from == to) {
        return value;
    }

    std::string err = FORMAT(
        "Invalid cast. Cannot cast value of type '{0}' to '{1}'", 
        this->get_type_name(from), this->get_type_name(to)
    );
    
    if (from->isArrayTy() && to->isPointerTy()) {
        if (from->getArrayElementType() != to->getPointerElementType()) {
            utils::error(expr->span, err);
        }

        value = this->as_reference(value);
        return this->builder->CreateBitCast(value, to);
    }

    if (from->isPointerTy() && !(to->isPointerTy() || to->isIntegerTy())) {
        utils::error(expr->value->span, err);
    } else if (from->isAggregateType() && !to->isAggregateType()) {
        utils::error(expr->value->span, err);
    }

    if (llvm::isa<llvm::Constant>(value)) {
        llvm::Constant* constant = llvm::cast<llvm::Constant>(value);
        if (llvm::isa<llvm::ConstantInt>(constant)) {
            llvm::ConstantInt* cint = llvm::cast<llvm::ConstantInt>(constant);
            return llvm::ConstantInt::get(to, cint->getZExtValue());
        } else if (llvm::isa<llvm::ConstantFP>(constant)) {
            llvm::ConstantFP* cfp = llvm::cast<llvm::ConstantFP>(constant);
            return llvm::ConstantFP::get(to, cfp->getValueAPF().convertToDouble());
        }

        return Value(constant, true);
    }

    if (from->isIntegerTy()) {
        if (to->isFloatingPointTy()) {
            return this->builder->CreateSIToFP(value, to);
        } else if (to->isIntegerTy()) {
            uint32_t bits = from->getIntegerBitWidth();
            if (bits < to->getIntegerBitWidth()) {
                return this->builder->CreateZExt(value, to);
            } else if (bits > to->getIntegerBitWidth()) {
                ERROR(expr->value->span, "Cannot cast value of type '{0}' to '{1}'", this->get_type_name(from), this->get_type_name(to));
            }
        } else if (to->isPointerTy()) {
            return this->builder->CreateIntToPtr(value, to);
        }
    } else if (from->isFloatingPointTy()) {
        if (to->isFloatingPointTy()) {
            return this->builder->CreateFPCast(value, to);
        } else if (to->isIntegerTy()) {
            return this->builder->CreateFPToSI(value, to);
        }
    } else if (from->isPointerTy()) {
        if (to->isIntegerTy()) {
            return this->builder->CreatePtrToInt(value, to);
        }
    }

    // TODO: Retain mutability of the value and check for `mut` types.
    return this->builder->CreateBitCast(value, to);
}

Value Visitor::visit(ast::SizeofExpr* expr) {
    uint32_t size = 0;

    if (expr->value->kind() == ast::ExprKind::Variable) {
        ast::VariableExpr* id = expr->value->as<ast::VariableExpr>();
        if (TYPE_SIZES.find(id->name) != TYPE_SIZES.end()) {
            return Value(this->builder->getInt32(TYPE_SIZES[id->name]), true);
        }
    } 

    Value val = expr->value->accept(*this);
    if (val.structure) {
        size = this->getsizeof(val.structure->type);
    } else {
        size = this->getsizeof(val.unwrap(expr->value->span));
    }

    return Value(this->builder->getInt32(size), true);
}

Type Visitor::visit(ast::BuiltinTypeExpr* expr) {
    return this->get_builtin_type(expr->value);
}

Type Visitor::visit(ast::IntegerTypeExpr* expr) {
    llvm::Value* value = expr->size->accept(*this).unwrap(expr->size->span);
    if (!llvm::isa<llvm::ConstantInt>(value)) {
        ERROR(expr->size->span, "Integer type size must be a constant");
    }

    int64_t size = llvm::cast<llvm::ConstantInt>(value)->getSExtValue();
    if (size < 1 || size > LLVM_MAX_INT_BITS) {
        ERROR(expr->size->span, "Integer type size must be between 1 and {0} bits", LLVM_MAX_INT_BITS);
    }

    return this->builder->getIntNTy(size);
}

Type Visitor::visit(ast::NamedTypeExpr* expr) {
    Scope* scope = this->scope;
    while (!expr->parents.empty()) {
        std::string name = expr->parents.front();
        expr->parents.pop_front();

        if (scope->has_namespace(name)) {
            scope = scope->get_namespace(name)->scope;
        } else if (scope->has_module(name)) {
            scope = scope->get_module(name)->scope;
        } else {
            ERROR(expr->span, "Unrecognised namespace '{0}'", name);
        }
    }

    if (scope->has_struct(expr->name)) {
        auto structure = scope->get_struct(expr->name);
        return structure->type;
    } else if (scope->has_type(expr->name)) {
        return scope->get_type(expr->name).type;
    } else if (scope->has_enum(expr->name)) {
        return scope->get_enum(expr->name)->type;
    }

    ERROR(expr->span, "Unrecognised type '{0}'", expr->name);
}

Type Visitor::visit(ast::PointerTypeExpr* expr) {
    Type type = expr->type->accept(*this);
    if (type->isPointerTy()) {
        // If the type that it's pointing to is a function, we don't want to double up on the pointer
        llvm::Type* elt = type->getPointerElementType();
        if (elt->isFunctionTy()) {
            return Type(type.value, false, true, expr->is_immutable);
        }
    }

    return Type(type->getPointerTo(), false, true, expr->is_immutable);
}

Type Visitor::visit(ast::ArrayTypeExpr* expr) {
    llvm::Type* type = expr->type->accept(*this).value;
    if (type->isVoidTy()) {
        ERROR(expr->span, "Cannot create an array of type 'void'");
    }

    llvm::Value* size = expr->size->accept(*this).unwrap(expr->span);
    if (!llvm::isa<llvm::ConstantInt>(size)) {
        ERROR(expr->size->span, "Array size must be a constant integer");
    }

    llvm::ConstantInt* csize = llvm::cast<llvm::ConstantInt>(size);
    return llvm::ArrayType::get(type, csize->getZExtValue());
}

Type Visitor::visit(ast::TupleTypeExpr* expr) {
    std::vector<llvm::Type*> types;
    for (auto& ty : expr->types) {
        llvm::Type* type = ty->accept(*this).value;
        if (type->isVoidTy()) {
            ERROR(ty->span, "Cannot create a tuple with a 'void' element");
        }

        types.push_back(type);
    }

    return this->create_tuple_type(types);
}

Type Visitor::visit(ast::FunctionTypeExpr* expr) {
    llvm::Type* ret = this->builder->getVoidTy();
    if (expr->ret) {
        ret = expr->ret->accept(*this).value;
    }

    std::vector<llvm::Type*> types;
    for (auto& param : expr->args) {
        llvm::Type* ty = param->accept(*this).value;
        if (ty->isVoidTy()) {
            ERROR(param->span, "Function parameter type cannot be of type 'void'");
        }

        types.push_back(ty);
    }

    auto f = llvm::FunctionType::get(ret, types, false);
    return f->getPointerTo();
}

Type Visitor::visit(ast::ReferenceTypeExpr* expr) {
    llvm::Type* type = expr->type->accept(*this).value;
    if (type->isVoidTy()) {
        ERROR(expr->span, "Cannot create a reference to type 'void'");
    }

    return Type(type->getPointerTo(), true, false, expr->is_immutable);
}

Value Visitor::visit(ast::TypeAliasExpr* expr) {
    llvm::Type* type = expr->type->accept(*this).value;
    this->scope->types[expr->name] = TypeAlias { expr->name, type, nullptr, expr->span };

    return nullptr;
}