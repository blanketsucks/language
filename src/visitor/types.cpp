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
        t2 = t2->getNonOpaquePointerElementType();
        if (t2->isVoidTy()) {
            return true;
        }

        t1 = t1->getNonOpaquePointerElementType(); 
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
        if (t2->isFloatingPointTy()) {
            return true;
        } else if (t2->isIntegerTy()) {
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
    std::string name = type->name();
    if (this->structs.find(name) != this->structs.end()) {
        Struct* structure = this->structs[name];
        llvm::Type* ty = structure->type;

        while (type->isPointer()) {
            ty = ty->getPointerTo();
            type = type->getPointerElementType();
        }
        
        return ty;
    } else if (type->isTuple()) { // TODO: pointers to tuples
        TupleType* tuple = type->cast<TupleType>();
        uint32_t hash = tuple->hash();
        
        llvm::StructType* structure;
        if (this->tuples.find(hash) != this->tuples.end()) {
            structure = this->tuples[hash];
        } else {
            structure = tuple->to_llvm_type(*this->context);
            this->tuples[hash] = structure;
        }

        return structure;
    }

    return type->to_llvm_type(*this->context);
}

llvm::Value* Visitor::cast(llvm::Value* value, Type* type) {
    return this->cast(value, type->to_llvm_type(*this->context));
}

llvm::Value* Visitor::cast(llvm::Value* value, llvm::Type* type) {
    if (value->getType() == type) {
        return value;
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
    llvm::Type* to = this->get_llvm_type(expr->to);

    if (from == to) {
        return value;
    }

    std::string err = FORMAT(
        "Invalid cast. Cannot cast value of type '{0}' to '{1}'", 
        this->get_type_name(from), this->get_type_name(to)
    );
    
    if (from->isPointerTy() && !to->isIntegerTy(LONG_SIZE)) {
        if (!to->isPointerTy()) {
            NOTE(expr->end, "Pointer memory addresses are of type 'long' not '{s}'", this->get_type_name(to));
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
        llvm::Type* type = this->get_llvm_type(expr->type);
        size = this->getsizeof(type);
    }

    return Value(this->builder->getInt32(size), true);
}
