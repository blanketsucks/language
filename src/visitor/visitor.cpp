#include <quart/visitor.h>
#include <quart/parser/ast.h>

using namespace quart;

Visitor::Visitor(const std::string& name, CompilerOptions& options) : options(options) {
    this->name = name;

    this->context = std::make_unique<llvm::LLVMContext>();
    this->module = std::make_unique<llvm::Module>(name, *this->context);
    this->builder = std::make_unique<llvm::IRBuilder<>>(*this->context);
    this->fpm = std::make_unique<llvm::legacy::FunctionPassManager>(this->module.get());

    this->registry = TypeRegistry::create(*this->context);

    this->fpm->add(llvm::createPromoteMemoryToRegisterPass());
    this->fpm->add(llvm::createInstructionCombiningPass());
    this->fpm->add(llvm::createReassociatePass());
    this->fpm->add(llvm::createGVNPass());
    this->fpm->add(llvm::createCFGSimplificationPass());
    this->fpm->add(llvm::createDeadStoreEliminationPass());

    this->fpm->doInitialization();

    this->global_scope = new Scope(name, ScopeType::Global);
    this->scope = this->global_scope;

    Builtins::init(*this);
}

void Visitor::finalize() {
    this->fpm->doFinalization();

    this->global_scope->finalize(true);
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

    this->registry->clear();
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

quart::Scope* Visitor::create_scope(const std::string& name, ScopeType type) {
    Scope* scope = new quart::Scope(name, type, this->scope);
    this->scope->children.push_back(scope);

    this->scope = scope;
    return scope;
}

std::string Visitor::format_symbol(const std::string& name) {
    std::string symbol;
    if (this->current_module) { 
        symbol += this->current_module->to_string() + ".";
    }

    if (this->current_impl) symbol += this->current_impl->name + ".";
    if (this->current_struct) symbol += this->current_struct->name + ".";
    if (this->current_function) symbol += this->current_function->name;

    if (symbol.front() == '.') {
        symbol.erase(0, 1);
    }

    if (!symbol.empty() && symbol.back() == '.') {
        symbol.pop_back();
    }

    return symbol.empty() ? name : FORMAT("{0}.{1}", symbol, name);
}

llvm::Constant* Visitor::to_str(llvm::StringRef str) {
    return this->builder->CreateGlobalStringPtr(str, ".str", 0, this->module.get());
}

llvm::Constant* Visitor::to_str(const char* str) { return this->to_str(llvm::StringRef(str)); }
llvm::Constant* Visitor::to_str(const std::string& str) { return this->to_str(llvm::StringRef(str)); }

llvm::Constant* Visitor::to_int(u64 value, u32 bits) {
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

std::vector<Value> Visitor::unpack(const Value& value, u32 n, const Span& span) {
    quart::Type* type = value.type;
    if (!type->is_pointer()) {
        if (!type->is_tuple()) {
            ERROR(span, "Cannot unpack value of type '{0}'", type->get_as_string());
        }

        if (n > type->get_tuple_size()) {
            ERROR(span, "Not enough elements to unpack. Expected {0} but got {1}", n, type->get_tuple_size());
        }

        std::vector<Value> values;
        values.reserve(n);

        if (llvm::isa<llvm::ConstantStruct>(value.inner)) {
            llvm::ConstantStruct* constant = llvm::cast<llvm::ConstantStruct>(value.inner);
            for (u32 i = 0; i < n; i++) {
                Value element = Value(
                    constant->getAggregateElement(i), 
                    type->get_tuple_element(i), 
                    Value::Constant
                );

                values.push_back(element);
            }
        } else {
            for (u32 i = 0; i < n; i++) {
                llvm::Value* val = this->builder->CreateExtractValue(value, i);
                Value element = Value(val, type->get_tuple_element(i));

                values.push_back(element);
            }
        }

        return values;
    }

    type = type->get_pointee_type();
    if (!type->is_tuple() && !type->is_array()) {
        ERROR(span, "Cannot unpack value of type '{0}'", type->get_as_string());
    }

    llvm::Type* ltype = type->to_llvm_type();
    if (type->is_array()) {
        u32 elements = type->get_array_size();
        if (n > elements) {
            ERROR(span, "Not enough elements to unpack. Expected {0} but got {1}", n, elements);
        }

        quart::Type* element_type = type->get_array_element_type();

        std::vector<Value> values;
        values.reserve(n);
        for (u32 i = 0; i < n; i++) {
            std::vector<llvm::Value*> idx = {this->builder->getInt32(0), this->builder->getInt32(i)};

            llvm::Value* ptr = this->builder->CreateGEP(ltype, value, idx);
            Value element = Value(this->load(ptr), element_type);

            values.push_back(element);
        }

        return values;
    }

    u32 elements = type->get_tuple_size();
    if (n > elements) {
        ERROR(span, "Not enough elements to unpack. Expected {0} but got {1}", n, elements);
    }

    std::vector<Value> values;
    values.reserve(n);

    for (u32 i = 0; i < n; i++) {
        llvm::Value* ptr = this->builder->CreateStructGEP(ltype, value, i);
        Value element = Value(this->load(ptr), type->get_tuple_element(i));

        values.push_back(element);
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

ScopeLocal Visitor::as_reference(ast::Expr& expr, bool require_ampersand) {
    if (require_ampersand) {
        if (expr.kind() != ast::ExprKind::UnaryOp) {
            ERROR(expr.span, "Expected a reference or '&' before expression");
        }

        ast::UnaryOpExpr* unary = expr.as<ast::UnaryOpExpr>();
        if (unary->op != UnaryOp::BinaryAnd) {
            ERROR(expr.span, "Expected a reference '&' before expression");
        }

        return this->as_reference(*unary->value);
    }

    switch (expr.kind()) {
        case ast::ExprKind::Variable: {
            ast::VariableExpr* variable = expr.as<ast::VariableExpr>();
            ScopeLocal local = this->scope->get_local(variable->name);

            if (local.is_null()) {
                ERROR(variable->span, "Name '{0}' does not exist in this scope", variable->name);
            }

            return local;
        }
        case ast::ExprKind::Index: {
            ast::IndexExpr* idx = expr.as<ast::IndexExpr>();

            ScopeLocal parent = this->as_reference(*idx->value);
            if (parent.is_null()) {
                return parent;
            }

            quart::Type* type = parent.type;
            if (!type->is_pointer() && !type->is_array()) {
                ERROR(idx->value->span, "Cannot index into value of type '{0}'", type->get_as_string());
            }

            if (type->get_pointer_depth() > 1) {
                parent.value = this->load(parent.value);
            }

            quart::Value index = idx->index->accept(*this);
            if (index.is_empty_value()) {
                ERROR(idx->index->span, "Expected an expression");
            }

            if (!index.type->is_int()) {
                ERROR(idx->index->span, "Indicies must be integers");
            }

            llvm::Value* result = nullptr;
            llvm::Type* ty = type->to_llvm_type();

            if (type->is_array()) {
                result = this->builder->CreateGEP(ty, parent.value, {this->builder->getInt32(0), index});
            } else {
                result = this->builder->CreateGEP(ty, parent.value, index);
            }

            return ScopeLocal::from_scope_local(parent, result);
        }
        case ast::ExprKind::Attribute: {
            ast::AttributeExpr* attribute = expr.as<ast::AttributeExpr>();

            ScopeLocal parent = this->as_reference(*attribute->parent);
            if (parent.is_null()) {
                return parent;
            }

            llvm::Value* value = parent.value;
            quart::Type* type = parent.type;

            if (type->get_pointer_depth() >= 1) {
                value = this->load(value);
                type = type->get_pointee_type();
            }
            
            bool is_struct = quart::is_structure_type(type);
            if (!is_struct) {
                return ScopeLocal::null();
            }

            auto structure = this->get_struct_from_type(type);
            if (!structure) {
                ERROR(attribute->parent->span, "Cannot access attribute of type '{0}'", type->get_as_string());
            }

            int index = structure->get_field_index(attribute->attribute);
            if (index < 0) {
                ERROR(expr.span, "Field '{0}' does not exist in struct '{1}'", attribute->attribute, structure->name);
            }

            StructField& field = structure->fields[attribute->attribute];
            llvm::Type* ty = structure->type->to_llvm_type();

            auto ref = ScopeLocal::from_scope_local(
                parent,
                this->builder->CreateStructGEP(ty, value, index),
                field.type
            );

            if (this->current_struct != structure && field.flags & StructField::Mutable) {
                ref.flags |= ScopeLocal::Mutable;
            }

            return ref;
        }
        case ast::ExprKind::UnaryOp: {
            ast::UnaryOpExpr* unary = expr.as<ast::UnaryOpExpr>();
            if (unary->op != UnaryOp::BinaryAnd) {
                return ScopeLocal::null();
            }

            return this->as_reference(*unary->value);
        }
        case ast::ExprKind::Maybe: {
            ast::MaybeExpr* maybe = expr.as<ast::MaybeExpr>();
            return this->as_reference(*maybe->value);
        }
        default:
            return ScopeLocal::null();
    }
}

void Visitor::create_global_constructors(llvm::Function::LinkageTypes linkage) {
    if (this->early_function_calls.empty()) {
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
    for (auto& call : this->early_function_calls) {
        llvm::Value* value = this->call(
            call.function, call.args, call.self, false, nullptr
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
    Variable* variable = this->scope->get_variable(name);
    variable->flags |= Variable::Mutated;
}

void Visitor::mark_as_mutated(const ScopeLocal& local) {
    if (local.name.empty()) {
        return;
    }

    this->mark_as_mutated(local.name);
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
    llvm::Constant* constant = nullptr;
    llvm::StringRef str(expr->value);

    u8 radix = 10;
    if (str.startswith("0x")) {
        str = str.drop_front(2); radix = 16;
    } else if (str.startswith("0b")) {
        str = str.drop_front(2); radix = 2;
    }

    quart::Type* type = nullptr;
    if (expr->is_float) {
        type = expr->bits == 32 ? this->registry->get_f32_type() : this->registry->get_f64_type();
        constant = llvm::ConstantFP::get(type->to_llvm_type(), expr->value);
    } else {
        u32 bits = expr->bits;
        if (this->inferred && this->inferred->is_int()) {
            type = this->inferred;
            bits = type->get_int_bit_width();
        } else {
            type = this->registry->create_int_type(expr->bits, true);
        }

        u32 needed = llvm::APInt::getBitsNeeded(str, radix);
        if (needed > bits) {
            ERROR(expr->span, "Integer literal requires {0} bits but only {1} are available", needed, bits);
        }

        llvm::APInt value(bits, str, radix);
        constant = this->builder->getInt(value);
    }

    return Value(constant, type, Value::Constant);
}

Value Visitor::visit(ast::CharExpr* expr) {
    quart::Type* type = this->registry->create_int_type(8, true);
    return Value(this->builder->getInt8(expr->value), type, Value::Constant);
}

Value Visitor::visit(ast::FloatExpr* expr) {
    llvm::Type* type = nullptr;
    if (expr->is_double) {
        type = this->builder->getDoubleTy();
    } else {
        type = this->builder->getFloatTy();
    }

    return Value(
        llvm::ConstantFP::get(type, expr->value),
        this->registry->wrap(type),
        Value::Constant
    );
}

Value Visitor::visit(ast::StringExpr* expr) {
    llvm::Value* str = this->builder->CreateGlobalStringPtr(expr->value, ".str", 0, this->module.get());
    return Value(
        str,
        this->registry->create_int_type(8, true)->get_pointer_to(false),
        Value::Constant
    );
}

Value Visitor::visit(ast::BlockExpr* expr) {
    Scope* prev = this->scope;
    Scope* scope = this->create_scope("block", ScopeType::Anonymous);

    Value last = nullptr;
    for (auto& stmt : expr->block) {
        if (!stmt) {
            continue;
        }

        last = stmt->accept(*this);
    }

    for (auto& defer : scope->defers) {
        defer->accept(*this);
    }

    this->scope = prev;
    return last;
}

Value Visitor::visit(ast::ExternBlockExpr* expr) {
    for (auto& stmt : expr->block) {
        if (!stmt) continue;
        stmt->accept(*this);
    }
    
    return EMPTY_VALUE;
}

Value Visitor::visit(ast::OffsetofExpr* expr) {
    Value value = expr->value->accept(*this);
    if (!(value.flags & Value::Struct)) {
        ERROR(expr->value->span, "Expected a structure type");
    }

    auto structure = value.as<quart::Struct*>();
    int index = structure->get_field_index(expr->field);

    if (index < 0) {
        ERROR(expr->span, "Field '{1}' does not exist in struct '{0}'", expr->field, structure->name);
    }

    StructField& field = structure->fields[expr->field];
    return Value(
        this->builder->getInt32(field.offset),
        this->registry->create_int_type(32, true),
        Value::Constant
    );
}

Value Visitor::visit(ast::StaticAssertExpr* expr) {
    Value value = expr->condition->accept(*this);
    if (value.is_empty_value()) {
        ERROR(expr->condition->span, "Expected an expression");
    }

    llvm::ConstantInt* constant = llvm::dyn_cast<llvm::ConstantInt>(value.inner);
    if (!constant) {
        ERROR(
            expr->condition->span, 
            "Expected a constant integer expression but got an expression of type '{0}'",
            value.type->get_as_string()
        );
    }

    if (constant->isZero()) {
        if (!expr->message.empty()) {
            ERROR(expr->condition->span, "Static assertion failed: {0}", expr->message);
        } else {
            ERROR(expr->condition->span, "Static assertion failed");
        }
    }

    return EMPTY_VALUE;
}

Value Visitor::visit(ast::MaybeExpr* expr) {
    return EMPTY_VALUE;
}