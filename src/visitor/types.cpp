#include "objects/values.h"
#include "parser/ast.h"
#include "visitor.h"
#include "utils/string.h"

#include "llvm/IR/Instructions.h"
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
    if (type->isVoidTy()) {
        return "void";
    } else if (type->isFloatTy()) {
        return "f32";
    } else if (type->isDoubleTy()) {
        return "f64";
    } else if (type->isIntegerTy(1)) {
        return "bool";
    } else if (type->isIntegerTy(8)) {
        return "i8";
    } else if (type->isIntegerTy(16)) {
        return "i16";
    } else if (type->isIntegerTy(32)) {
        return "i32";
    } else if (type->isIntegerTy(64)) {
        return "i64";
    } else if (type->isIntegerTy(128)) {
        return "i128";
    } else if (type->isPointerTy()) {
        return Visitor::get_type_name(type->getPointerElementType()) + "*";
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

        return utils::replace(name.str(), ".", "::");
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
    llvm::Type* to = expr->to->accept(*this).type.value;

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

    if (expr->value->kind() == ast::ExprKind::Variable) {
        ast::VariableExpr* id = expr->value->as<ast::VariableExpr>();
        if (TYPE_SIZES.find(id->name) != TYPE_SIZES.end()) {
            return Value(this->builder->getInt32(TYPE_SIZES[id->name]), true);
        }
    } 

    Value val = expr->value->accept(*this);
    if (val.structure) {
        size = this->getsizeof(val.structure->type);
    } else if (val.enumeration) {
        size = this->getsizeof(val.enumeration->type);
    } else {
        size = this->getsizeof(val.unwrap(expr->value->span));
    }

    return Value(this->builder->getInt32(size), true);
}

Value Visitor::visit(ast::BuiltinTypeExpr* expr) {
    return Value::from_type(this->get_builtin_type(expr->value));
}

Value Visitor::visit(ast::IntegerTypeExpr* expr) {
    llvm::Value* value = expr->size->accept(*this).unwrap(expr->size->span);
    if (!llvm::isa<llvm::ConstantInt>(value)) {
        ERROR(expr->size->span, "Integer type size must be a constant");
    }

    int64_t size = llvm::cast<llvm::ConstantInt>(value)->getSExtValue();
    if (size < 1 || size > LLVM_MAX_INT_BITS) {
        ERROR(expr->size->span, "Integer type size must be between 1 and {0} bits", LLVM_MAX_INT_BITS);
    }

    return Value::from_type(this->builder->getIntNTy(size));
}

Value Visitor::visit(ast::NamedTypeExpr* expr) {
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
        return Value::from_type(scope->get_struct(expr->name)->type);
    } else if (scope->has_type(expr->name)) {
        return Value::from_type(scope->get_type(expr->name).type);
    } else if (scope->has_enum(expr->name)) {
        return Value::from_type(scope->get_enum(expr->name)->type);
    }

    ERROR(expr->span, "Unrecognised type '{0}'", expr->name);
}

Value Visitor::visit(ast::PointerTypeExpr* expr) {
    Value ret = expr->element->accept(*this);
    if (ret.type->isPointerTy()) {
        // If the type that it's pointing to is a function, we don't want to double up on the pointer
        llvm::Type* element = ret.type->getPointerElementType();
        if (element->isFunctionTy()) {
            return ret;
        }
    }

    return Value::from_type(ret.type->getPointerTo());
}

Value Visitor::visit(ast::ArrayTypeExpr* expr) {
    llvm::Type* element = expr->element->accept(*this).type.value;
    if (element->isVoidTy()) {
        ERROR(expr->span, "Cannot create an array of type 'void'");
    }

    llvm::Value* size = expr->size->accept(*this).unwrap(expr->span);
    if (!llvm::isa<llvm::ConstantInt>(size)) {
        ERROR(expr->size->span, "Array size must be a constant integer");
    }

    llvm::ConstantInt* csize = llvm::cast<llvm::ConstantInt>(size);
    return Value::from_type(llvm::ArrayType::get(element, csize->getZExtValue()));
}

Value Visitor::visit(ast::TupleTypeExpr* expr) {
    std::vector<llvm::Type*> types;
    for (auto& element : expr->elements) {
        llvm::Type* type = element->accept(*this).type.value;
        if (type->isVoidTy()) {
            ERROR(element->span, "Cannot create a tuple with a 'void' element");
        }

        types.push_back(type);
    }

    return Value::from_type(this->create_tuple_type(types));
}

Value Visitor::visit(ast::FunctionTypeExpr* expr) {
    llvm::Type* ret = this->builder->getVoidTy();
    if (expr->ret) {
        ret = expr->ret->accept(*this).type.value;
    }

    std::vector<llvm::Type*> types;
    for (auto& param : expr->args) {
        llvm::Type* ty = param->accept(*this).type.value;
        if (ty->isVoidTy()) {
            ERROR(param->span, "Function parameter type cannot be of type 'void'");
        }

        types.push_back(ty);
    }

    auto f = llvm::FunctionType::get(ret, types, false);
    return Value::from_type(f->getPointerTo());
}

Value Visitor::visit(ast::ReferenceTypeExpr* expr) {
    llvm::Type* type = expr->type->accept(*this).type.value;
    if (type->isVoidTy()) {
        ERROR(expr->span, "Cannot create a reference to type 'void'");
    }

    return Value::from_type(Type(type->getPointerTo(), true));
}

Value Visitor::visit(ast::TypeAliasExpr* expr) {
    llvm::Type* type = expr->type->accept(*this).type.value;
    this->scope->types[expr->name] = TypeAlias { expr->name, type, nullptr, expr->span };

    return nullptr;
}