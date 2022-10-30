#include "visitor.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"

#include <string>

Visitor::Visitor(std::string name, std::string entry, bool with_optimizations) {
    this->name = name;
    this->entry = entry;
    this->with_optimizations = with_optimizations;

    this->context = utils::make_ref<llvm::LLVMContext>();
    this->module = utils::make_ref<llvm::Module>(name, *this->context);
    this->builder = utils::make_ref<llvm::IRBuilder<>>(*this->context);
    this->fpm = utils::make_ref<llvm::legacy::FunctionPassManager>(this->module.get());

    this->fpm->add(llvm::createPromoteMemoryToRegisterPass());
    this->fpm->add(llvm::createInstructionCombiningPass());
    this->fpm->add(llvm::createReassociatePass());
    this->fpm->add(llvm::createGVNPass());
    this->fpm->add(llvm::createCFGSimplificationPass());
    this->fpm->add(llvm::createDeadStoreEliminationPass());

    this->fpm->doInitialization();

    this->global_scope = new Scope(name, ScopeType::Global);
    this->scope = this->global_scope;

    this->functions = {};
    this->structs = {};
    this->namespaces = {};
}

void Visitor::free() {
    this->fpm->doFinalization();

    auto remove = [this](Function* func) {
        if (!func->used && !func->is_entry && !func->is_anonymous) {
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
        
        for (auto branch : func->branches) delete branch;
        delete func;
    };

    std::function<void(Scope*)> remove_scope = [&](Scope* scope) {
        for (auto& func : scope->functions) {
            remove(func.second);
        }

        for (auto pair : scope->structs) {
            delete pair.second;
        }

        for (auto pair : scope->namespaces) {
            delete pair.second;
        }

        for (auto pair : scope->enums) {
            delete pair.second;
        }

        for (auto child : scope->children) {
            remove_scope(child);
        }

        delete scope;
    };

    remove_scope(this->global_scope);
}

void Visitor::dump(llvm::raw_ostream& stream) {
    this->module->print(stream, nullptr);
}

void Visitor::set_insert_point(llvm::BasicBlock* block, bool push) {
    if (this->current_function) {
        Function* func = this->current_function;
        func->current_block = block;

        if (push) {
            func->value->getBasicBlockList().push_back(block);
        }
    }

    this->builder->SetInsertPoint(block);
}

Scope* Visitor::create_scope(std::string name, ScopeType type) {
    Scope* scope = new Scope(name, type, this->scope);
    this->scope->children.push_back(scope);

    this->scope = scope;
    return scope;
}

std::pair<std::string, bool> Visitor::format_intrinsic_function(std::string name) {
    bool is_intrinsic = false;
    if (name.substr(0, 17) == "__llvm_intrinsic_") {
        is_intrinsic = true;
        name = name.substr(17);
        for (size_t i = 0; i < name.size(); i++) {
            if (name[i] == '_') {
                name[i] = '.';
            }
        }
    }

    return std::make_pair(name, is_intrinsic);
}

std::string Visitor::format_name(std::string name) {
    if (this->current_module) { name = this->current_module->qualified_name + "." + name; }
    if (this->current_namespace) { name = this->current_namespace->qualified_name + "." + name; }
    
    return name;
}

llvm::AllocaInst* Visitor::create_alloca(llvm::Type* type) {
    llvm::BasicBlock* block = this->builder->GetInsertBlock();
    llvm::Function* function = block->getParent();

    assert(function && "`create_alloca` cannot be called from a global scope");

    llvm::IRBuilder<> tmp(&function->getEntryBlock(), function->getEntryBlock().begin());
    return tmp.CreateAlloca(type, nullptr);
}

llvm::Value* Visitor::load(llvm::Value* value, llvm::Type* type) {
    if (type) {
        return this->builder->CreateLoad(type, value);
    }

    type = value->getType();
    if (type->isPointerTy()) {
        type = type->getNonOpaquePointerElementType();
        return this->builder->CreateLoad(type, value);
    }

    return value;
}

std::vector<llvm::Value*> Visitor::unpack(llvm::Value* value, uint32_t n, Location location) {
    llvm::Type* type = value->getType();
    if (!type->isPointerTy()) {
        if (!type->isStructTy()) {
            ERROR(location, "Cannot unpack value of type '{0}'", this->get_type_name(type));
        }

        llvm::StringRef name = type->getStructName();
        if (!name.startswith("__tuple")) {
            ERROR(location, "Cannot unpack value of type '{0}'", this->get_type_name(type));
        }

        uint32_t elements = type->getStructNumElements();
        if (n > elements) {
            ERROR(location, "Not enough elements to unpack. Expected {0} but got {1}", n, elements);
        }

        std::vector<llvm::Value*> values;
        if (llvm::isa<llvm::ConstantStruct>(value)) {
            llvm::ConstantStruct* constant = llvm::cast<llvm::ConstantStruct>(value);
            for (uint32_t i = 0; i < n; i++) {
                values.push_back(constant->getAggregateElement(i));
            }
        } else {
            for (uint32_t i = 0; i < n; i++) {
                llvm::Value* val = this->builder->CreateExtractValue(value, i);
                values.push_back(val);
            }
        }

        return values;
    }

    type = type->getNonOpaquePointerElementType();
    if (!type->isAggregateType()) {
        ERROR(location, "Cannot unpack value of type '{0}'", this->get_type_name(type));
    }

    if (type->isArrayTy()) {
        uint32_t elements = type->getArrayNumElements();
        if (n > elements) {
            ERROR(location, "Not enough elements to unpack. Expected {0} but got {1}", n, elements);
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
        ERROR(location, "Cannot unpack value of type '{0}'", this->get_type_name(type));
    }

    uint32_t elements = type->getStructNumElements();
    if (n > elements) {
        ERROR(location, "Not enough elements to unpack. Expected {0} but got {1}", n, elements);
    }

    std::vector<llvm::Value*> values;
    for (uint32_t i = 0; i < n; i++) {
        llvm::Value* ptr = this->builder->CreateStructGEP(type, value, i);
        llvm::Value* val = this->builder->CreateLoad(type->getStructElementType(i), ptr);

        values.push_back(val);
    }

    return values;
}


Scope::Local Visitor::get_pointer_from_expr(ast::Expr* expr) {
    if (expr->kind() == ast::ExprKind::Variable) {
        ast::VariableExpr* var = expr->cast<ast::VariableExpr>();
        return this->scope->get_local(var->name);
    } else if (expr->kind() == ast::ExprKind::Element) {
        ast::ElementExpr* element = expr->cast<ast::ElementExpr>();;
        return this->get_pointer_from_expr(element->value.get());
    } else if (expr->kind() == ast::ExprKind::Attribute) {
        ast::AttributeExpr* attr = expr->cast<ast::AttributeExpr>();;
        llvm::Value* parent = this->get_pointer_from_expr(attr->parent.get()).first;

        if (!parent) {
            return {nullptr, false};
        }

        llvm::Type* type = parent->getType()->getNonOpaquePointerElementType();
        if (!type->isStructTy()) {
            return {nullptr, false};
        }

        std::string name = type->getStructName().str();
        Struct* structure = this->structs[name];

        int index = structure->get_field_index(attr->attribute);
        if (index < 0) {
            ERROR(expr->start, "Field '{0}' does not exist in struct '{1}'", attr->attribute, structure->name);
        }

        return {this->builder->CreateStructGEP(structure->type, parent, index), false};
    }

    return {nullptr, false};
}

void Visitor::create_global_initializers() {
    if (this->initializers.empty()) {
        return;
    }

    llvm::Function* function = this->create_function(
        "__global_variable_initializer", 
        this->builder->getVoidTy(),
        {},
        false,
        llvm::Function::LinkageTypes::InternalLinkage
    );

    function->setSection(".text.startup");

    this->builder->SetInsertPoint(llvm::BasicBlock::Create(*this->context, "", function));

    for (auto call : this->initializers) {
        llvm::Value* value = this->call(
            call->function, call->args, false, nullptr, nullptr, call->start
        );

        this->builder->CreateStore(value, call->store);
        delete call;
    }

    this->builder->CreateRetVoid();

    llvm::StructType* type = llvm::StructType::create(
        {this->builder->getInt32Ty(), function->getType(), this->builder->getInt8PtrTy()}
    );
    llvm::Constant* init = llvm::ConstantStruct::get(
        type, {
            this->builder->getInt32(65535), 
            function, 
            llvm::ConstantPointerNull::get(this->builder->getInt8PtrTy())
        }
    );

    llvm::ArrayType* array = llvm::ArrayType::get(type, 1);
    this->module->getOrInsertGlobal( "llvm.global_ctors", array);

    llvm::GlobalVariable* global = this->module->getNamedGlobal("llvm.global_ctors");

    global->setInitializer(llvm::ConstantArray::get(array, init));
    global->setLinkage(llvm::GlobalValue::AppendingLinkage);
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
    llvm::Type* type = nullptr;
    if (expr->is_double) {
        type = this->builder->getDoubleTy();
    } else {
        type = this->builder->getFloatTy();
    }


    return Value(llvm::ConstantFP::get(type, expr->value), true);
}

Value Visitor::visit(ast::StringExpr* expr) {
    return Value(
        this->builder->CreateGlobalStringPtr(expr->value, ".str", 0, this->module.get()),
        true
    );
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

Value Visitor::visit(ast::OffsetofExpr* expr) {
    Value value = expr->value->accept(*this);
    if (!value.structure) {
        ERROR(expr->start, "Expected a structure");
    }

    Struct* structure = value.structure;
    int index = structure->get_field_index(expr->field);

    if (index < 0) {
        ERROR(expr->start, "Field '{1}' does not exist in struct '{0}'", expr->field, structure->name);
    }

    StructField field = structure->fields[expr->field];
    return Value(this->builder->getInt32(field.offset), true);
}

Value Visitor::visit(ast::WhereExpr* expr) {
    Value value = expr->expr->accept(*this);
    Location location;

    if (value.function) {
        location = value.function->start;
    } else if (value.structure) {
        location = value.structure->start;
    } else if (value.ns) {
        location = value.ns->start;
    } else if (value.enumeration) {
        location = value.enumeration->start;
    } else {
        location = expr->expr->start;
    }

    return Value(this->builder->CreateGlobalStringPtr(location.format(), ".str"), true);
}