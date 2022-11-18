#include "parser/ast.h"
#include "visitor.h"

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

std::string Visitor::get_type_name(llvm::Type* type) {
    if (type->isVoidTy()) {
        return "void";
    } else if (type->isFloatTy()) {
        return "float";
    } else if (type->isDoubleTy()) {
        return "double";
    } else if (type->isIntegerTy(1)) {
        return "bool";
    } else if (type->isIntegerTy(8)) {
        return "char";
    } else if (type->isIntegerTy(16)) {
        return "short";
    } else if (type->isIntegerTy(32)) {
        return "int";
    } else if (type->isIntegerTy(LONG_SIZE)) {
        return "long";
    } else if (type->isPointerTy()) {
        return Visitor::get_type_name(type->getNonOpaquePointerElementType()) + "*";
    } else if (type->isArrayTy()) {
        std::string name = Visitor::get_type_name(type->getArrayElementType());
        return FORMAT("[{0}; {1}]", name, type->getArrayNumElements());
    } else if (type->isStructTy()) {
        llvm::StructType* ty = llvm::cast<llvm::StructType>(type);
        llvm::StringRef name = ty->getName();

        if (name.startswith("__tuple")) {
            std::vector<std::string> names;
            for (auto& field : ty->elements()) {
                names.push_back(Visitor::get_type_name(field));
            }

            return FORMAT("({0})", llvm::make_range(names.begin(), names.end()));
        }

        return name.str();
    } else if (type->isFunctionTy()) {
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
  
    return "";
}

bool Visitor::is_compatible(llvm::Type* t1, llvm::Type* t2) {
    if (t1 == t2) {
        return true;
    }

    if (t1->isPointerTy()) {
        if (!t2->isPointerTy()) { return false; }

        t2 = t2->getNonOpaquePointerElementType(); t1 = t1->getNonOpaquePointerElementType(); 
        if (t2->isVoidTy() || t1->isVoidTy()) {
            return true;
        }

        return this->is_compatible(t1, t2);
    } else if (t1->isArrayTy()) {
        if (!t2->isArrayTy()) { 
            if (!t2->isPointerTy()) { return false; }

            t2 = t2->getNonOpaquePointerElementType();
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

llvm::Type* Visitor::get_llvm_type(Type* type) {
    return type->to_llvm_type(*this);
}

llvm::Value* Visitor::cast(llvm::Value* value, Type* type) {
    return this->cast(value, this->get_llvm_type(type));
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

        constant->dump();
        return constant;
    }

    if (value->getType()->isIntegerTy() && type->isIntegerTy()) {
        return this->builder->CreateIntCast(value, type, true);
    } else if (value->getType()->isPointerTy() && type->isIntegerTy()) {
        return this->builder->CreatePtrToInt(value, type);
    }

    return this->builder->CreateBitCast(value, type);
}

Value Visitor::visit(ast::CastExpr* expr) {
    llvm::Value* value = expr->value->accept(*this).unwrap(expr->start);

    llvm::Type* from = value->getType();
    llvm::Type* to = expr->to->accept(*this).type;

    if (from == to) {
        return value;
    }

    std::string err = FORMAT(
        "Invalid cast. Cannot cast value of type '{0}' to '{1}'", 
        this->get_type_name(from), this->get_type_name(to)
    );
    
    if (from->isPointerTy() && (to->isPointerTy() || to->isIntegerTy())) {
        if (!to->isPointerTy() && to->getIntegerBitWidth() < 64) {
            NOTE(expr->end, "Pointer memory addresses are of type 'i64' not '{s}'", this->get_type_name(to));
        }
    } else if (from->isPointerTy() && !to->isPointerTy()) {
        utils::error(expr->end, err);
    } else if (from->isAggregateType() && !to->isAggregateType()) {
        utils::error(expr->end, err);
    }

    if (from->isIntegerTy()) {
        if (to->isFloatingPointTy()) {
            return this->builder->CreateSIToFP(value, to);
        } else if (to->isIntegerTy()) {
            unsigned bits = from->getIntegerBitWidth();
            if (bits < to->getIntegerBitWidth()) {
                return this->builder->CreateZExt(value, to);
            } else if (bits > to->getIntegerBitWidth()) {
                return this->builder->CreateTrunc(value, to);
            }
        } else if (to->isPointerTy()) {
            return this->builder->CreateIntToPtr(value, to);
        }
    } else if (from->isFloatTy()) {
        if (to->isDoubleTy()) {
            return this->builder->CreateFPExt(value, to);
        } else if (to->isIntegerTy()) {
            return this->builder->CreateFPToSI(value, to);
        }
    } else if (from->isPointerTy()) {
        if (to->isIntegerTy()) {
            return this->builder->CreatePtrToInt(value, to);
        }
    }

    return this->builder->CreateBitCast(value, to);
}

Value Visitor::visit(ast::SizeofExpr* expr) {
    uint32_t size = 0;
    if (expr->value) {
        llvm::Value* value = expr->value->accept(*this).unwrap(expr->start);
        size = this->getsizeof(value);
    } else {
        llvm::Type* type = expr->accept(*this).type;
        size = this->getsizeof(type);
    }

    return Value(this->builder->getInt32(size), true);
}

Value Visitor::visit(ast::BuiltinTypeExpr* expr) {
    switch (expr->value) {
        case ast::BuiltinType::Void:
            return Value::with_type(this->builder->getVoidTy());
        case ast::BuiltinType::Bool:
            return Value::with_type(this->builder->getInt1Ty());
        case ast::BuiltinType::i8:
            return Value::with_type(this->builder->getInt8Ty());
        case ast::BuiltinType::i16:
            return Value::with_type(this->builder->getInt16Ty());
        case ast::BuiltinType::i32:
            return Value::with_type(this->builder->getInt32Ty());
        case ast::BuiltinType::i64:
            return Value::with_type(this->builder->getInt64Ty());
        case ast::BuiltinType::i128:
            return Value::with_type(this->builder->getIntNTy(128));
        case ast::BuiltinType::f32:
            return Value::with_type(this->builder->getFloatTy());
        case ast::BuiltinType::f64:
            return Value::with_type(this->builder->getDoubleTy());
        default:
            _UNREACHABLE
    }
}

Value Visitor::visit(ast::NamedTypeExpr* expr) {
    Scope* scope = this->scope;
    while (!expr->parents.empty()) {
        std::string name = expr->parents.front();
        expr->parents.pop_front();

        if (this->scope->has_namespace(name)) {
            scope = this->scope->get_namespace(name)->scope;
        } else if (this->scope->has_module(name)) {
            scope = this->scope->get_module(name)->scope;
        } else {
            ERROR(expr->start, "Unrecognised namespace '{0}'", name);
        }
    }

    if (!scope->has_struct(expr->name)) {
        ERROR(expr->start, "Unrecognised type '{0}'", expr->name);
    }

    auto structure = scope->get_struct(expr->name);
    return Value::with_type(structure->type);
}

Value Visitor::visit(ast::PointerTypeExpr* expr) {
    Value ret = expr->element->accept(*this);
    return Value::with_type(ret.type->getPointerTo());
}

Value Visitor::visit(ast::ArrayTypeExpr* expr) {
    llvm::Type* element = expr->element->accept(*this).type;
    llvm::Value* size = expr->size->accept(*this).unwrap(expr->start);
    if (!llvm::isa<llvm::ConstantInt>(size)) {
        ERROR(expr->size->start, "Array size must be a constant integer");
    }

    llvm::ConstantInt* csize = llvm::cast<llvm::ConstantInt>(size);
    return Value::with_type(llvm::ArrayType::get(element, csize->getZExtValue()));
}

Value Visitor::visit(ast::TupleTypeExpr* expr) {
    std::vector<llvm::Type*> types;
    for (auto& element : expr->elements) {
        types.push_back(element->accept(*this).type);
    }

    TupleKey key(types);
    llvm::StructType* type = nullptr;

    if (this->tuples.find(key) == this->tuples.end()) {
        type = llvm::StructType::create(*this->context, types, "__tuple");
    } else {
        type = this->tuples[key];
    }

    return Value::with_type(type);
}

Value Visitor::visit(ast::FunctionTypeExpr* expr) {
    llvm::Type* ret = expr->ret->accept(*this).type;
    std::vector<llvm::Type*> types;
    for (auto& param : expr->args) {
        types.push_back(param->accept(*this).type);
    }

    return Value::with_type(llvm::FunctionType::get(ret, types, false));
}