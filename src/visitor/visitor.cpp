#include "visitor.h"
#include "utils.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Transforms/Scalar.h"

Visitor::Visitor(std::string name, std::string entry, bool with_optimizations) {
    this->name = name;
    this->entry = entry;
    this->with_optimizations = with_optimizations;

    this->module = utils::make_ref<llvm::Module>(name, this->context);
    this->builder = utils::make_ref<llvm::IRBuilder<>>(this->context);
    this->fpm = utils::make_ref<llvm::legacy::FunctionPassManager>(this->module.get());

    this->fpm->add(llvm::createPromoteMemoryToRegisterPass());
    this->fpm->add(llvm::createInstructionCombiningPass());
    this->fpm->add(llvm::createReassociatePass());
    this->fpm->add(llvm::createGVNPass());
    this->fpm->add(llvm::createCFGSimplificationPass());
    this->fpm->add(llvm::createDeadStoreEliminationPass());
    this->fpm->add(llvm::createAggressiveDCEPass());
    
    this->fpm->doInitialization();

    this->constants = {
        {"null", llvm::ConstantInt::getNullValue(llvm::Type::getInt1PtrTy(this->context))},
        {"nullptr", llvm::ConstantPointerNull::get(llvm::Type::getInt1PtrTy(this->context))}
    };

    this->functions = {};
    this->structs = {};
    this->namespaces = {};
}

void Visitor::cleanup() {
    auto remove = [this](Function* func) {
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

        for (auto branch : func->branches) {
            delete branch;
        }

        delete func;
    };

    Function* entry = this->functions[this->entry];
    entry->used = true;

    for (auto pair : this->functions) {
        if (!pair.second) {
            continue;
        }

        remove(pair.second);
    }

    for (auto pair : this->structs) {
        for (auto method : pair.second->methods) {
            if (!method.second) {
                continue;
            }

            remove(method.second);
        }
    }

    // TODO: segfault involving `using` expr. Maybe copy objects?
    for (auto pair : this->namespaces) {
        for (auto func : pair.second->functions) {
            if (!func.second) {
                continue;
            }

            remove(func.second);
        }
    }
}

void Visitor::free() {
    for (auto pair : this->structs) { 
        delete pair.second;
    }

    for (auto pair : this->namespaces) {
        delete pair.second;
    }

    for (auto pair : this->enums) {
        delete pair.second;
    }

    this->structs.clear();
    this->namespaces.clear();
    this->enums.clear();
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
            structure = tuple->to_llvm_type(this->context);
            this->tuples[hash] = structure;
        }

        return structure;
    }

    return type->to_llvm_type(this->context);
}

Type* Visitor::from_llvm_type(llvm::Type* ty) {
    return Type::from_llvm_type(ty);
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

llvm::Value* Visitor::load(llvm::Type* type, llvm::Value* value) {
    if (type->isAggregateType()) {
        return value;
    }

    return this->builder->CreateLoad(type, value);
}

llvm::Value* Visitor::load(llvm::Value* value) {
    llvm::Type* type = value->getType();
    if (type->isPointerTy()) {
        type = type->getNonOpaquePointerElementType();
        if (type->isAggregateType()) {
            return this->builder->CreateLoad(type, value);
        }
    }

    return value;
}

std::vector<llvm::Value*> Visitor::unpack(Location location, llvm::Value* value, uint32_t n) {
    llvm::Type* type = value->getType();
    if (!type->isPointerTy()) {
        if (!type->isStructTy()) {
            std::string name = Type::from_llvm_type(type)->name();
            ERROR(location, "Cannot unpack value of type '{s}'", name);
        }

        llvm::StringRef name = type->getStructName();
        if (!name.startswith("__tuple")) {
            ERROR(location, "Cannot unpack value of type '{s}'", name);
        }

        uint32_t elements = type->getStructNumElements();
        if (n > elements) {
            ERROR(location, "Not enough elements to unpack. Expected {i} but got {i}", n, elements);
        }

        std::vector<llvm::Value*> values;
        for (uint32_t i = 0; i < n; i++) {
            llvm::Value* val = this->builder->CreateExtractValue(value, i);
            values.push_back(val);
        }

        return values;
    }

    type = type->getNonOpaquePointerElementType();
    if (!type->isAggregateType()) {
        std::string name = Type::from_llvm_type(type)->name();
        ERROR(location, "Cannot unpack value of type '{s}'", name);
    }

    if (type->isArrayTy()) {
        uint32_t elements = type->getArrayNumElements();
        if (n > elements) {
            ERROR(location, "Not enough elements to unpack. Expected {i} but got {i}", n, elements);
        }

        llvm::Type* ty = type->getArrayElementType();
        std::vector<llvm::Value*> values;
        for (uint32_t i = 0; i < n; i++) {
            std::vector<llvm::Value*> idx = {this->builder->getInt32(0), this->builder->getInt32(i)};
            llvm::Value* ptr = this->builder->CreateGEP(type, value, idx);

            llvm::Value* val = this->builder->CreateLoad(ty, ptr);
            values.push_back(val);
        }

        return values;
    }

    llvm::StringRef name = type->getStructName();
    // Maybe i should allow it either way??
    if (!name.startswith("__tuple")) {
        ERROR(location, "Cannot unpack value of type '{s}'", name);
    }

    uint32_t elements = type->getStructNumElements();
    if (n > elements) {
        ERROR(location, "Not enough elements to unpack. Expected {i} but got {i}", n, elements);
    }

    std::vector<llvm::Value*> values;
    for (uint32_t i = 0; i < n; i++) {
        llvm::Value* ptr = this->builder->CreateStructGEP(type, value, i);
        llvm::Value* val = this->builder->CreateLoad(type->getStructElementType(i), ptr);

        values.push_back(val);
    }

    return values;
}

uint32_t Visitor::getsizeof(llvm::Value* value) {
    return this->getsizeof(value->getType());
}

uint32_t Visitor::getallocsize(llvm::Type* type) {
    llvm::TypeSize tsize = this->module->getDataLayout().getTypeAllocSize(type);
    return tsize.getFixedSize();
}

uint32_t Visitor::getsizeof(llvm::Type* type) {
    if (type->isPointerTy()) {
        return this->getallocsize(type);
    }

    if (type->isArrayTy()) {
        uint32_t tsize = type->getArrayElementType()->getPrimitiveSizeInBits() / 8;
        return type->getArrayNumElements() * tsize;
    } else if (type->isStructTy()) {
        return this->getallocsize(type);
    }

    return type->getPrimitiveSizeInBits() / 8;
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
    return Value(
        this->builder->getInt(llvm::APInt(expr->bits, expr->value, true)),
        true
    );
}

Value Visitor::visit(ast::FloatExpr* expr) {
    return Value(
        llvm::ConstantFP::get(this->context, llvm::APFloat(expr->value)), 
        true
    );
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
    llvm::Value* value = expr->value->accept(*this).unwrap(expr->start);

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
        llvm::Value* value = expr->value->accept(*this).unwrap(expr->start);
        size = this->getsizeof(value);
    } else {
        llvm::Type* type = this->get_llvm_type(expr->type);
        size = this->getsizeof(type);
    }

    return llvm::ConstantInt::get(llvm::Type::getInt32Ty(this->context), size);
}

Value Visitor::visit(ast::OffsetofExpr* expr) {
    Value value = expr->value->accept(*this);
    if (!value.structure) {
        ERROR(expr->start, "Expected a structure");
    }

    Struct* structure = value.structure;
    int index = structure->get_field_index(expr->field);

    if (index < 0) {
        ERROR(expr->start, "Field '{s}' does not exist in struct '{s}'", expr->field, structure->name);
    }

    StructField field = structure->fields[expr->field];
    return this->builder->getInt32(field.offset);
}