#include <quart/visitor.h>
#include <quart/parser/ast.h>

Visitor::Visitor(const std::string& name, CompilerOptions& options) : options(options) {
    Builtins::init(*this);

    this->name = name;

    this->context = utils::make_scope<llvm::LLVMContext>();
    this->module = utils::make_scope<llvm::Module>(name, *this->context);
    this->builder = utils::make_scope<llvm::IRBuilder<>>(*this->context);
    this->fpm = utils::make_scope<llvm::legacy::FunctionPassManager>(this->module.get());

    this->fpm->add(llvm::createPromoteMemoryToRegisterPass());
    this->fpm->add(llvm::createInstructionCombiningPass());
    this->fpm->add(llvm::createReassociatePass());
    this->fpm->add(llvm::createGVNPass());
    this->fpm->add(llvm::createCFGSimplificationPass());
    this->fpm->add(llvm::createDeadStoreEliminationPass());

    this->fpm->doInitialization();

    this->global_scope = new Scope(name, ScopeType::Global);
    this->scope = this->global_scope;
}

void Visitor::finalize() {
    this->fpm->doFinalization();

    this->global_scope->finalize(this->options.opts.dead_code_elimination);
    delete this->global_scope;

    auto& globals = this->module->getGlobalList();
    for (auto it = globals.begin(); it != globals.end();) {
        if (it->getName().startswith("llvm.")) {
            it++; continue;
        }

        if (it->use_empty()) {
            it = globals.erase(it);
        } else {
            it++;
        }
    }
    
    for (auto& entry : this->finalizers) {
        entry(*this);
    }
}

void Visitor::add_finalizer(Visitor::Finalizer finalizer) {
    this->finalizers.push_back(finalizer);
}

void Visitor::dump(llvm::raw_ostream& stream) {
    this->module->print(stream, nullptr);
}

void Visitor::set_insert_point(llvm::BasicBlock* block, bool push) {
    if (this->current_function) {
        auto function = this->current_function;
        function->current_block = block;

        if (push) {
            function->value->getBasicBlockList().push_back(block);
        }
    }

    this->builder->SetInsertPoint(block);
}

Scope* Visitor::create_scope(const std::string& name, ScopeType type) {
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

        name = "llvm." + name;
    }

    return std::make_pair(name, is_intrinsic);
}

std::string Visitor::format_symbol(const std::string& name) {
    std::string formatted;
    if (this->current_module) { 
        formatted += this->current_module->get_clean_path_name(true) + ".";
    }

    if (this->current_namespace) { formatted += this->current_namespace->name + "."; }
    if (this->current_struct) { formatted += this->current_struct->name + "."; }
    if (this->current_function) { formatted += this->current_function->name; }

    if (!formatted.empty() && formatted.back() == '.') {
        formatted.pop_back();
    }

    return formatted.empty() ? name : FORMAT("{0}.{1}", formatted, name);
}

llvm::Constant* Visitor::to_str(const char* str) {
    return this->builder->CreateGlobalStringPtr(str, ".str", 0, this->module.get());
}

llvm::Constant* Visitor::to_str(const std::string& str) {
    return this->builder->CreateGlobalStringPtr(str, ".str", 0, this->module.get());
}

llvm::Constant* Visitor::to_int(uint64_t value, uint32_t bits) {
    return this->builder->getIntN(bits, value);
}

llvm::Constant* Visitor::to_float(double value) {
    return llvm::ConstantFP::get(*this->context, llvm::APFloat(value));
}

llvm::AllocaInst* Visitor::alloca(llvm::Type* type) {
    llvm::BasicBlock* block = this->builder->GetInsertBlock();
    llvm::Function* function = block->getParent();

    assert(function && "`alloca` cannot be called from the global scope");

    llvm::IRBuilder<> tmp(&function->getEntryBlock(), function->getEntryBlock().begin());
    return tmp.CreateAlloca(type, nullptr);
}

llvm::Value* Visitor::load(llvm::Value* value, llvm::Type* type) {
    if (type) {
        return this->builder->CreateLoad(type, value);
    }

    type = value->getType();
    if (type->isPointerTy()) {
        type = type->getPointerElementType();
        return this->builder->CreateLoad(type, value);
    }

    return value;
}

std::vector<llvm::Value*> Visitor::unpack(llvm::Value* value, uint32_t n, Span span) {
    llvm::Type* type = value->getType();
    if (!type->isPointerTy()) {
        if (!type->isStructTy()) {
            ERROR(span, "Cannot unpack value of type '{0}'", this->get_type_name(type));
        }

        llvm::StringRef name = type->getStructName();
        if (!name.startswith("__tuple")) {
            ERROR(span, "Cannot unpack value of type '{0}'", this->get_type_name(type));
        }

        uint32_t elements = type->getStructNumElements();
        if (n > elements) {
            ERROR(span, "Not enough elements to unpack. Expected {0} but got {1}", n, elements);
        }

        std::vector<llvm::Value*> values;
        values.reserve(n);

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

    type = type->getPointerElementType();
    if (!type->isAggregateType()) {
        ERROR(span, "Cannot unpack value of type '{0}'", this->get_type_name(type));
    }

    if (type->isArrayTy()) {
        uint32_t elements = type->getArrayNumElements();
        if (n > elements) {
            ERROR(span, "Not enough elements to unpack. Expected {0} but got {1}", n, elements);
        }

        llvm::Type* ty = type->getArrayElementType();

        std::vector<llvm::Value*> values;
        values.reserve(n);

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
        ERROR(span, "Cannot unpack value of type '{0}'", this->get_type_name(type));
    }

    uint32_t elements = type->getStructNumElements();
    if (n > elements) {
        ERROR(span, "Not enough elements to unpack. Expected {0} but got {1}", n, elements);
    }

    std::vector<llvm::Value*> values;
    values.reserve(n);

    for (uint32_t i = 0; i < n; i++) {
        llvm::Value* ptr = this->builder->CreateStructGEP(type, value, i);
        llvm::Value* val = this->builder->CreateLoad(type->getStructElementType(i), ptr);

        values.push_back(val);
    }

    return values;
}

llvm::Value* Visitor::as_reference(llvm::Value* value) {
    if (value->getType()->isPointerTy()) {
        return value;
    }

    if (!llvm::isa<llvm::LoadInst>(value)) {
        return nullptr;
    }
    
    llvm::LoadInst* load = llvm::cast<llvm::LoadInst>(value);
    return load->getPointerOperand();
}

ScopeLocal Visitor::as_reference(utils::Scope<ast::Expr>& expr, bool require_ampersand) {
    if (require_ampersand) {
        if (expr->kind() != ast::ExprKind::UnaryOp) {
            ERROR(expr->span, "Expected a reference or '&' before expression");
        }

        ast::UnaryOpExpr* unary = expr->as<ast::UnaryOpExpr>();
        if (unary->op != TokenKind::BinaryAnd) {
            ERROR(expr->span, "Expected a reference '&' before expression");
        }

        return this->as_reference(unary->value);
    }

    switch (expr->kind()) {
        case ast::ExprKind::Variable: {
            ast::VariableExpr* variable = expr->as<ast::VariableExpr>();
            return this->scope->get_local(variable->name);
        }
        case ast::ExprKind::Element: {
            ast::ElementExpr* element = expr->as<ast::ElementExpr>();

            ScopeLocal parent = this->as_reference(element->value);
            if (parent.is_null()) {
                return parent;
            }

            llvm::Type* type = parent.type;
            if (!type->isPointerTy() && !type->isArrayTy()) {
                ERROR(element->value->span, "Cannot index into value of type '{0}'", this->get_type_name(type));
            }

            if (this->get_pointer_depth(type) > 1) {
                parent.value = this->load(parent.value, type);
            }

            llvm::Value* index = element->index->accept(*this).unwrap(element->index->span);
            if (!index->getType()->isIntegerTy()) {
                ERROR(element->index->span, "Indicies must be integers");
            }

            llvm::Value* ptr = nullptr;
            if (type->isArrayTy()) {
                ptr = this->builder->CreateGEP(type, parent.value, {this->builder->getInt32(0), index});
            } else {
                ptr = this->builder->CreateGEP(type, parent.value, index);
            }

            return ScopeLocal::from_scope_local(parent, ptr);
        }
        case ast::ExprKind::Attribute: {
            ast::AttributeExpr* attribute = expr->as<ast::AttributeExpr>();

            ScopeLocal parent = this->as_reference(attribute->parent);
            if (parent.is_null()) {
                return parent;
            }
            
            if (!this->is_struct(parent.type)) {
                return ScopeLocal::null();
            }

            auto structure = this->get_struct(parent.type);
            if (!structure) {
                ERROR(attribute->parent->span, "Cannot access attribute of type '{0}'", this->get_type_name(parent.type));
            }

            int index = structure->get_field_index(attribute->attribute);
            if (index < 0) {
                ERROR(expr->span, "Field '{0}' does not exist in struct '{1}'", attribute->attribute, structure->name);
            }

            llvm::Value* value = parent.value;
            if (this->get_pointer_depth(parent.type) > 0) {
                value = this->load(parent.value, parent.type);
            }

            auto ref = ScopeLocal::from_scope_local(
                parent,
                this->builder->CreateStructGEP(structure->type, value, index),
                structure->type->getStructElementType(index)
            );

            StructField& field = structure->fields[attribute->attribute];
            if (this->current_struct != structure) {
                ref.is_immutable |= field.is_readonly;
            }

            return ref;
        }
        case ast::ExprKind::UnaryOp: {
            ast::UnaryOpExpr* unary = expr->as<ast::UnaryOpExpr>();
            if (unary->op != TokenKind::BinaryAnd) {
                return ScopeLocal::null();
            }

            return this->as_reference(unary->value);
        }
        case ast::ExprKind::Maybe: {
            ast::MaybeExpr* maybe = expr->as<ast::MaybeExpr>();
            return this->as_reference(maybe->value);
        }
        default:
            return ScopeLocal::null();
    }
}

void Visitor::create_global_constructors(llvm::Function::LinkageTypes linkage) {
    if (this->constructors.empty()) {
        return;
    }

    llvm::Function* function = this->create_function(
        "__global_constructors_init",
        this->builder->getVoidTy(),
        {},
        false,
        linkage
    );

    function->setSection(".text.startup");

    this->builder->SetInsertPoint(llvm::BasicBlock::Create(*this->context, "", function));
    for (auto& call : this->constructors) {
        llvm::Value* value = this->call(
            call.function, 
            call.args, 
            call.self,
            false,
            nullptr
        );

        if (call.store) {
            this->builder->CreateStore(value, call.store);
        }
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
    this->module->getOrInsertGlobal("llvm.global_ctors", array);

    llvm::GlobalVariable* global = this->module->getNamedGlobal("llvm.global_ctors");

    global->setInitializer(llvm::ConstantArray::get(array, init));
    global->setLinkage(llvm::GlobalValue::AppendingLinkage);
}

void Visitor::mark_as_mutated(const std::string& name) {
    Variable& var = this->scope->get_variable(name);
    var.is_mutated = true;
}

void Visitor::mark_as_mutated(const ScopeLocal& local) {
    if (local.name.empty()) {
        return;
    }

    this->mark_as_mutated(local.name);
}

void Visitor::visit(std::vector<utils::Scope<ast::Expr>> statements) {
    for (auto& stmt : statements) {
        if (!stmt) {
            continue;
        }

        stmt->accept(*this);
    }
}

Value Visitor::visit(ast::IntegerExpr* expr) {
    llvm::Constant* constant = nullptr;
    llvm::StringRef str(expr->value);

    int radix = 10;
    if (str.startswith("0x")) {
        str = str.drop_front(2); radix = 16;
    } else if (str.startswith("0b")) {
        str = str.drop_front(2); radix = 2;
    }

    if (expr->is_float) {
        llvm::Type* type = expr->bits == 32 ? this->builder->getFloatTy() : this->builder->getDoubleTy();
        constant = llvm::ConstantFP::get(type, expr->value);
    } else {
        llvm::APInt value(expr->bits, str, radix);
        constant = this->builder->getInt(value);
    }

    return Value(constant, true);
}

Value Visitor::visit(ast::CharExpr* expr) {
    return Value(this->builder->getInt8(expr->value), true);
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
    llvm::Value* str = this->builder->CreateGlobalStringPtr(expr->value, ".str", 0, this->module.get());
    Value value = Value(str, true);

    value.is_immutable = true;
    return value;
}

Value Visitor::visit(ast::BlockExpr* expr) {
    // Scope* scope = this->create_scope("block", ScopeType::Anonymous);
    Value last = nullptr;

    for (auto& stmt : expr->block) {
        if (!stmt) {
            continue;
        }

        last = stmt->accept(*this);
    }

    // scope->exit(this);
    return last;
}

Value Visitor::visit(ast::OffsetofExpr* expr) {
    Value value = expr->value->accept(*this);
    if (!value.structure) {
        ERROR(expr->value->span, "Expected a structure type");
    }

    auto structure = value.structure;
    int index = structure->get_field_index(expr->field);

    if (index < 0) {
        ERROR(expr->span, "Field '{1}' does not exist in struct '{0}'", expr->field, structure->name);
    }

    StructField& field = structure->fields[expr->field];
    return Value(this->builder->getInt32(field.offset), true);
}

Value Visitor::visit(ast::StaticAssertExpr* expr) {
    llvm::Value* value = expr->condition->accept(*this).unwrap(expr->condition->span);
    if (!llvm::isa<llvm::ConstantInt>(value)) {
        ERROR(expr->condition->span, "Expected a constant integer expression");
    }

    if (llvm::cast<llvm::ConstantInt>(value)->isZero()) {
        if (!expr->message.empty()) {
            ERROR(expr->condition->span, "Static assertion failed: {0}", expr->message);
        } else {
            ERROR(expr->condition->span, "Static assertion failed");
        }
    }

    return nullptr;
}

Value Visitor::visit(ast::MaybeExpr* expr) {
    llvm::Value* value = expr->value->accept(*this).unwrap(expr->value->span);
    if (llvm::isa<llvm::Constant>(value)) {
        llvm::Constant* constant = llvm::cast<llvm::Constant>(value);
        if (constant->isZeroValue()) {
            ERROR(expr->value->span, "Expected a value of type '{0}' but got a null operand", this->get_type_name(constant->getType()));
        }

        return Value(constant, true);
    }

    llvm::BasicBlock* merge = this->create_if_statement(this->builder->CreateIsNull(value));
    
    const char* fmt = "Expected a value of type '{0}' but got got a null operand";
    this->panic(FORMAT(fmt, this->get_type_name(value->getType())), expr->value->span);

    this->set_insert_point(merge);
    return value;
}