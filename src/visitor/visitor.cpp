#include "visitor.h"

Visitor::Visitor(std::string name) {
    this->module = utils::make_ref<llvm::Module>(name, this->context);
    this->builder = utils::make_ref<llvm::IRBuilder<>>(this->context);
    this->fpm = std::make_unique<llvm::legacy::FunctionPassManager>(this->module.get());

    this->fpm->add(llvm::createPromoteMemoryToRegisterPass());
    this->fpm->add(llvm::createInstructionCombiningPass());
    this->fpm->add(llvm::createReassociatePass());
    this->fpm->add(llvm::createGVNPass());
    this->fpm->add(llvm::createCFGSimplificationPass());
    this->fpm->add(llvm::createDeadStoreEliminationPass());

    this->fpm->doInitialization();

    this->constants = {
        {"true", llvm::ConstantInt::getTrue(this->context)},
        {"false", llvm::ConstantInt::getFalse(this->context)},
        {"null", llvm::ConstantInt::getNullValue(llvm::Type::getInt1PtrTy(this->context))},
        {"nullptr", llvm::ConstantPointerNull::get(llvm::Type::getInt1PtrTy(this->context))}
    };

    this->functions = {};
    this->structs = {};
    this->namespaces = {};
}

void Visitor::cleanup() {
    auto remove = [this](Function* func) {
        if (!func) {
            return;
        }

        if (!func->used) {
            for (auto call : func->calls) {
                if (call) call->used = false;
            }

            if (func->attrs.has("allow_dead_code")) {
                return;
            }

            llvm::Function* function = this->module->getFunction(func->name);
            if (function) {
                function->eraseFromParent();
            }
        }
    };

    for (auto pair : this->functions) {
        if (pair.first == "main") continue;
        remove(pair.second);
    }

    for (auto pair : this->structs) {
        for (auto method : pair.second->methods) {
            remove(method.second);
        }
    }

    for (auto pair : this->namespaces) {
        for (auto method : pair.second->functions) {
            remove(method.second);
        }
    }
}

void Visitor::free() {
    auto free = [](std::map<std::string, Function*> map) {
        for (auto& pair : map) {
            for (auto branch : pair.second->branches) {
                delete branch;
            }

            delete pair.second;
        }

        map.clear();
    };

    free(this->functions);

    for (auto pair : this->structs) { 
        free(pair.second->methods); 
        delete pair.second;
    }

    for (auto pair : this->namespaces) {
        // free(pair.second->functions);
        delete pair.second;
    }

    this->structs.clear();
    this->namespaces.clear();

    for (auto type : this->allocated_types) {
        delete type;
    }
}

void Visitor::dump(llvm::raw_ostream& stream) {
    this->module->print(stream, nullptr);
}

std::pair<std::string, bool> Visitor::is_intrinsic(std::string name) {
    bool is_intrinsic = false;
    if (name.substr(0, 12) == "__intrinsic_") {
        is_intrinsic = true;
        name = name.substr(12);
        for (size_t i = 0; i < name.size(); i++) {
            if (name[i] == '_') {
                name[i] = '.';
            }
        }
    }

    return std::make_pair(name, is_intrinsic);
}

std::string Visitor::format_name(std::string name) {
    if (this->current_namespace) { name = this->current_namespace->name + "." + name; }
    if (this->current_struct) { name = this->current_struct->name + "." + name; }

    return name;
}

llvm::AllocaInst* Visitor::create_alloca(llvm::Function* function, llvm::Type* type) {
    llvm::IRBuilder<> tmp(&function->getEntryBlock(), function->getEntryBlock().begin());
    return tmp.CreateAlloca(type, nullptr);
}

llvm::Type* Visitor::get_llvm_type(Type* type) {
    std::string name = type->name();
    if (this->structs.find(name) != this->structs.end()) {
        Struct* structure = this->structs[name];
        llvm::Type* ty = structure->type;

        // Only create a pointer type if the structure is not opaque since opaque types are not constructable.
        if (!structure->opaque) {
            ty = ty->getPointerTo();
        }

        while (type->isPointer()) {
            ty = ty->getPointerTo();
            type = type->getPointerElementType();
        }
        
        return ty;
    }

    return type->to_llvm_type(this->context);
}

Type* Visitor::from_llvm_type(llvm::Type* ty) {
    Type* type = Type::from_llvm_type(ty);
    this->allocated_types.push_back(type);

    Type* holder = type;
    while (holder->hasContainedType()) {
        holder = holder->getContainedType();
        this->allocated_types.push_back(holder);
    }

    return type;
}

llvm::Value* Visitor::cast(llvm::Value* value, Type* type) {
    return this->cast(value, type->to_llvm_type(this->context));
}

llvm::Value* Visitor::cast(llvm::Value* value, llvm::Type* type) {
    if (value->getType() == type) {
        return value;
    }

    if (value->getType()->isIntegerTy() && type->isIntegerTy()) {
        return this->builder->CreateIntCast(value, type, false);
    } else if (value->getType()->isPointerTy() && type->isIntegerTy()) {
        return this->builder->CreatePtrToInt(value, type);
    }

    return this->builder->CreateBitCast(value, type);
}

void Visitor::visit(std::vector<std::unique_ptr<ast::Expr>> statements) {
    for (auto& stmt : statements) {
        if (!stmt) {
            continue;
        }

        stmt->accept(*this);
    }
}

Value Visitor::visit(ast::IntegerExpr* expr) {
    return Value(llvm::ConstantInt::get(this->context, llvm::APInt(expr->bits, expr->value, true)), true);
}

Value Visitor::visit(ast::FloatExpr* expr) {
    return Value(llvm::ConstantFP::get(this->context, llvm::APFloat(expr->value)), true);
}

Value Visitor::visit(ast::StringExpr* expr) {
    return Value(this->builder->CreateGlobalStringPtr(expr->value, ".str"), true);
}

Value Visitor::visit(ast::BlockExpr* expr) {
    Value last = nullptr;
    for (auto& stmt : expr->block) {
        if (!stmt) {
            continue;
        }

        last = stmt->accept(*this);
    }

    return last;
}

Value Visitor::visit(ast::CastExpr* expr) {
    llvm::Value* value = expr->value->accept(*this).unwrap(this, expr->start);

    llvm::Type* from = value->getType();
    llvm::Type* to = this->get_llvm_type(expr->to);

    if (from == to) {
        return value;
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
    uint32_t size;

    if (expr->value) {
        llvm::Value* value = expr->value->accept(*this).unwrap(this, expr->start);
        llvm::Type* type = value->getType();

        if (type->isPointerTy()) {
            type = value->getType()->getPointerElementType();

            if (type->isArrayTy()) {
                uint32_t tsize = type->getArrayElementType()->getPrimitiveSizeInBits() / 8;
                size = type->getArrayNumElements() * tsize;
            } else if (type->isStructTy()) {
                llvm::TypeSize tsize = this->module->getDataLayout().getTypeAllocSize(type);
                size = tsize.getFixedSize();
            } else {
                size = type->getPrimitiveSizeInBits() / 8;
            }
        } else {
            if (type->isArrayTy()) {
                uint32_t tsize = type->getArrayElementType()->getPrimitiveSizeInBits() / 8;
                size = type->getArrayNumElements() * tsize;
            } else {
                size = value->getType()->getPrimitiveSizeInBits() / 8;
            }
        }
    } else {
        llvm::Type* type = this->get_llvm_type(expr->type);
        size = type->getPrimitiveSizeInBits() / 8;
    }

    return llvm::ConstantInt::get(llvm::Type::getInt32Ty(this->context), size);
}