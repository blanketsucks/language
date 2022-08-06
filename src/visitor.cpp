#include "visitor.h"

#include "utils.h"
#include "lexer.h"
#include "llvm.h"

std::vector<Struct*> expand(Struct* structure) {
    std::vector<Struct*> parents;

    parents.push_back(structure);
    for (Struct* parent : structure->parents) {
        parents.push_back(parent);

        std::vector<Struct*> expanded = expand(parent);
        parents.insert(parents.end(), expanded.begin(), expanded.end());
    }

    return parents;
}

Visitor::Visitor(std::string module) {
    this->module = std::make_unique<llvm::Module>(llvm::StringRef(module), this->context);
    this->builder = std::make_unique<llvm::IRBuilder<>>(this->context);
    this->fpm = std::make_unique<llvm::legacy::FunctionPassManager>(this->module.get());

    // Promote allocas to registers.
    this->fpm->add(llvm::createPromoteMemoryToRegisterPass());

    // Do simple "peephole" optimizations
    this->fpm->add(llvm::createInstructionCombiningPass());

    // Reassociate expressions.
    this->fpm->add(llvm::createReassociatePass());

    // Eliminate Common SubExpressions.
    this->fpm->add(llvm::createGVNPass());

    // Simplify the control flow graph (deleting unreachable blocks etc).
    this->fpm->add(llvm::createCFGSimplificationPass());

    this->fpm->doInitialization();

    this->constants = {
        {"true", llvm::ConstantInt::getTrue(this->context)},
        {"false", llvm::ConstantInt::getFalse(this->context)},
        {"null", llvm::ConstantInt::getNullValue(llvm::Type::getInt1PtrTy(this->context))},
        {"nullptr", llvm::ConstantPointerNull::get(llvm::Type::getInt1PtrTy(this->context))}
    };
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

    return type->toLLVMType(this->context);
}

Type* Visitor::from_llvm_type(llvm::Type* ty) {
    Type* type = Type::fromLLVMType(ty);
    this->allocated_types.push_back(type);

    Type* holder = type;
    while (holder->hasContainedType()) {
        holder = holder->getContainedType();
        this->allocated_types.push_back(holder);
    }

    return type;
}

std::pair<llvm::Value*, bool> Visitor::get_variable(std::string name) {
    if (this->current_function) {
        llvm::Value* value = this->current_function->locals[name];
        if (value) {
            return std::make_pair(value, false);
        }

        value = this->current_function->constants[name];
        if (value) {
            return std::make_pair(value, true);
        }
    }

    return std::make_pair(nullptr, false);
}

Value Visitor::get_function(std::string name) {
    if (this->current_struct) {
        Function* func = this->current_struct->methods[name];
        if (func) {
            func->used = true;
            return Value::with_function(this->module->getFunction(func->name), func);
        }
    }

    if (this->current_namespace) {
        Function* func = this->current_namespace->functions[name];
        if (func) {
            func->used = true;
            return Value::with_function(this->module->getFunction(func->name), func);
        }
    }

    if (this->functions.find(name) != this->functions.end()) {
        Function* func = this->functions[name];
        func->used = true;

        return Value::with_function(this->module->getFunction(this->is_intrinsic(name).first), func);
    }

    return nullptr;
}

llvm::Value* Visitor::cast(llvm::Value* value, Type* type) {
    return this->cast(value, type->toLLVMType(this->context));
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

llvm::Function* Visitor::create_function(
    std::string name, llvm::Type* ret, std::vector<llvm::Type*> args, bool has_varargs, llvm::Function::LinkageTypes linkage
) {
    llvm::FunctionType* type = llvm::FunctionType::get(ret, args, has_varargs);
    return llvm::Function::Create(type, linkage, name, this->module.get());
}

llvm::Value* Visitor::unwrap(ast::Expr* expr) {
    return expr->accept(*this).unwrap(this, expr->start);
}

void Visitor::visit(std::unique_ptr<ast::Program> program) {
    for (auto& expr : program->ast) {
        if (!expr) {
            continue;
        }

        expr->accept(*this);
    }
}

Value Visitor::visit(ast::IntegerExpr* expr) {
    return llvm::ConstantInt::get(this->context, llvm::APInt(expr->bits, expr->value, true));
}

Value Visitor::visit(ast::FloatExpr* expr) {
    return llvm::ConstantFP::get(this->context, llvm::APFloat(expr->value));
}

Value Visitor::visit(ast::StringExpr* expr) {
    return this->builder->CreateGlobalStringPtr(expr->value, ".str");
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

Value Visitor::visit(ast::VariableExpr* expr) {
    if (this->structs.find(expr->name) != this->structs.end()) {
        return Value::with_struct(this->structs[expr->name]);
    } else if (this->namespaces.find(expr->name) != this->namespaces.end()) {
        return Value::with_namespace(this->namespaces[expr->name]);
    }

    if (this->current_function) {
        llvm::AllocaInst* variable = this->current_function->locals[expr->name];
        if (variable) {
            return this->builder->CreateLoad(variable->getAllocatedType(), variable);;
        }

        llvm::GlobalVariable* constant = this->current_function->constants[expr->name];
        if (constant) {
            return this->builder->CreateLoad(constant->getValueType(), constant);
        }
    }

    if (this->current_struct) {
        llvm::Value* value = this->current_struct->locals[expr->name];
        if (value) {
            return value;
        }
    }

    llvm::Constant* constant = this->constants[expr->name];
    if (constant) {
        return constant;
    }

    Value function = this->get_function(expr->name);
    if (function.function) {
        return function;
    }

    ERROR(expr->start, "Undefined variable '{s}'", expr->name); exit(1);
}

Value Visitor::visit(ast::VariableAssignmentExpr* expr) {
    llvm::Type* type;
    if (expr->external) {
        type = this->get_llvm_type(expr->type);
        this->module->getOrInsertGlobal(expr->name, type);

        llvm::GlobalVariable* global = this->module->getGlobalVariable(expr->name);
        global->setLinkage(llvm::GlobalValue::ExternalLinkage);

        return global;
    }

    llvm::Value* value;
    if (!expr->value) {
        type = this->get_llvm_type(expr->type);
        value = llvm::ConstantInt::getNullValue(type);
    } else {
        value = expr->value->accept(*this).unwrap(this, expr->start);
        if (!expr->type) {
            type = value->getType();
        } else {
            type = this->get_llvm_type(expr->type);
        }
    }

    if (!this->current_function) {
        this->module->getOrInsertGlobal(expr->name, type);
        llvm::GlobalVariable* variable = this->module->getNamedGlobal(expr->name);

        // TODO: let users specify other options somehow and this applies to other stuff too, not just this
        variable->setLinkage(llvm::GlobalValue::ExternalLinkage);
        variable->setInitializer((llvm::Constant*)value);

        return value;
    }

    llvm::Function* func = this->builder->GetInsertBlock()->getParent();
    llvm::AllocaInst* alloca_inst = this->create_alloca(func, type);

    this->builder->CreateStore(value, alloca_inst);
    this->current_function->locals[expr->name] = alloca_inst;

    return alloca_inst;
}

Value Visitor::visit(ast::ConstExpr* expr) {
    llvm::Value* value = expr->value->accept(*this).unwrap(this, expr->start);
    llvm::Type* type;

    if (!expr->type) {
        type = value->getType();
    } else {
        type = this->get_llvm_type(expr->type);
    }

    std::string name = expr->name;
    if (this->current_namespace) {
        name = this->current_namespace->name + "." + name;
    } else if (this->current_function) {
        name = this->current_function->name + "." + name;
    }

    name = "__const_" + name;
    this->module->getOrInsertGlobal(name, type);

    llvm::GlobalVariable* global = this->module->getNamedGlobal(name);
    global->setInitializer((llvm::Constant*)value);

    if (this->current_namespace) {
        this->current_namespace->locals[expr->name] = global;
    } else if (this->current_function) {
        this->current_function->constants[expr->name] = global;
    } else {
        this->constants[expr->name] = global;
    }

    return global;
}

Value Visitor::visit(ast::ArrayExpr* expr) {
    std::vector<Value> elements;
    for (auto& element : expr->elements) {
        Value value = element->accept(*this);
        elements.push_back(value);
    }

    if (elements.size() == 0) {
        return llvm::Constant::getNullValue(llvm::ArrayType::get(llvm::Type::getInt32Ty(this->context), 0));
    }

    // The type of an array is determined from the first element.
    llvm::Type* etype = elements[0].type();
    for (size_t i = 1; i < elements.size(); i++) {
        if (elements[i].type() != etype) {
            ERROR(expr->start, "All elements of an array must be of the same type");
        }
    }

    llvm::Function* parent = this->builder->GetInsertBlock()->getParent();

    llvm::ArrayType* type = llvm::ArrayType::get(etype, elements.size());
    llvm::AllocaInst* array = this->create_alloca(parent, type);

    for (size_t i = 0; i < elements.size(); i++) {
        std::vector<llvm::Value*> indices = {
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(this->context), llvm::APInt(32, 0, true)),
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(this->context), llvm::APInt(32, i))
        };

        llvm::Value* ptr = this->builder->CreateGEP(type, array, indices);
        this->builder->CreateStore(elements[i].unwrap(this, expr->start), ptr);
    }

    return array;
}

Value Visitor::visit(ast::UnaryOpExpr* expr) {
    llvm::Value* value = expr->value->accept(*this).unwrap(this, expr->start);

    bool is_floating_point = value->getType()->isFloatingPointTy();
    bool is_numeric = value->getType()->isIntegerTy() || is_floating_point;

    std::string name = Type::fromLLVMType(value->getType())->name();
    switch (expr->op) {
        case TokenType::Add:
            if (!is_numeric) {
                ERROR(expr->start, "Unsupported unary operator '+' for type: '{s}'", name);
            }

            return value;
        case TokenType::Minus:
            if (!is_numeric) {
                ERROR(expr->start, "Unsupported unary operator '-' for type: '{s}'", name);
            }

            if (is_floating_point) {
                return this->builder->CreateFNeg(value);
            } else {
                return this->builder->CreateNeg(value);
            }
        case TokenType::Not:
            return this->builder->CreateICmpEQ(this->cast(value, BooleanType), this->constants["true"]);
        case TokenType::BinaryNot:
            return this->builder->CreateNot(value);
        case TokenType::Mul: {
            if (!value->getType()->isPointerTy()) {
                ERROR(expr->start, "Unsupported unary operator '*' for type: '{s}'", name);
            }

            llvm::Type* type = value->getType()->getPointerElementType();
            return this->builder->CreateLoad(type, value);
        }
        case TokenType::BinaryAnd: {
            llvm::AllocaInst* alloca_inst = this->builder->CreateAlloca(value->getType());
            this->builder->CreateStore(value, alloca_inst);

            return alloca_inst;
        }
        case TokenType::Inc: {
            if (!is_numeric) {
                ERROR(expr->start, "Unsupported unary operator '++' for type: '{s}'", name);
            }

            llvm::Value* one = llvm::ConstantInt::get(value->getType(), llvm::APInt(value->getType()->getIntegerBitWidth(), 1, true));
            llvm::Value* result = this->builder->CreateAdd(value, one);

            this->builder->CreateStore(result, value);
            return result;
        }
        case TokenType::Dec: {
            if (!is_numeric) {
                ERROR(expr->start, "Unsupported unary operator '--' for type: '{s}'", name);
            }

            llvm::Value* one = llvm::ConstantInt::get(value->getType(), llvm::APInt(value->getType()->getIntegerBitWidth(), 1, true));
            return this->builder->CreateSub(value, one);
        }
        default:
            _UNREACHABLE
    }

    return nullptr;
}

Value Visitor::visit(ast::BinaryOpExpr* expr) {
    // Assingment is a special case.
    if (expr->op == TokenType::Assign) {
        if (expr->left->kind == ast::ExprKind::Attribute) {
            ast::AttributeExpr* attribute = expr->cast<ast::AttributeExpr>();
            llvm::Value* parent = attribute->parent->accept(*this).unwrap(this, expr->start);
            
            std::string name = parent->getType()->getPointerElementType()->getStructName().str();
            Struct* structure = this->structs[name];
            int index = structure->get_field_index(attribute->attribute);

            if (index < 0) {
                ERROR(expr->start, "Attribute '{s}' does not exist in structure '{s}'", attribute->attribute, name);
            }

            llvm::Value* value = expr->right->accept(*this).unwrap(this, expr->start);
            llvm::Value* pointer = this->builder->CreateStructGEP(structure->type, parent, index);

            this->builder->CreateStore(value, pointer);
            return value;
        } else if (expr->left->kind == ast::ExprKind::Element) {
            ast::ElementExpr* element = expr->cast<ast::ElementExpr>();
            llvm::Value* parent = element->value->accept(*this).unwrap(this, expr->start);

            llvm::Value* index = element->index->accept(*this).unwrap(this, expr->start);
            if (!index->getType()->isIntegerTy()) {
                ERROR(element->index->start, "Indicies must be integers");
            }

            llvm::Type* ty = parent->getType();

            llvm::Value* value = expr->right->accept(*this).unwrap(this, expr->start);
            llvm::Value* pointer = this->builder->CreateGEP(ty, parent, index);

            this->builder->CreateStore(value, pointer);
            return value;
        }

        // We directly use `dynamic_cast` to avoid the assert in Expr::cast.
        ast::VariableExpr* variable = dynamic_cast<ast::VariableExpr*>(expr->left.get());
        if (!variable) {
            ERROR(expr->start, "Left side of assignment must be a variable");
        }

        auto pair = this->get_variable(variable->name);
        if (pair.second) {
            ERROR(expr->start, "Cannot assign to constant");
        }

        llvm::Value* value = expr->right->accept(*this).unwrap(this, expr->start);
        this->builder->CreateStore(value, pair.first);

        return value;
    }

    llvm::Value* left = expr->left->accept(*this).unwrap(this, expr->start);
    llvm::Value* right = expr->right->accept(*this).unwrap(this, expr->start);

    llvm::Type* ltype = left->getType();
    llvm::Type* rtype = right->getType();

    if (ltype != rtype) {
        Type* lhs = Type::fromLLVMType(ltype);
        Type* rhs = Type::fromLLVMType(rtype);

        if (!(lhs->isPointer() && rhs->isInteger())) {
            if (lhs->is_compatible(rtype)) {
                right = this->cast(right, ltype);
            } else {
                std::string lname = lhs->name();
                std::string rname = rhs->name();

                std::string operation = Token::getTokenTypeValue(expr->op);
                ERROR(expr->start, "Unsupported operation '{s}' for types '{s}' and '{s}'", operation, lname, rname);
            }
        } else {
            left = this->builder->CreatePtrToInt(left, rtype);
        }
    }

    bool is_floating_point = ltype->isFloatingPointTy();
    switch (expr->op) {
        case TokenType::Add:
            if (is_floating_point) {
                return this->builder->CreateFAdd(left, right);
            } else {
                return this->builder->CreateAdd(left, right);
            }
        case TokenType::Minus:
            if (is_floating_point) {
                return this->builder->CreateFSub(left, right);
            } else {
                return this->builder->CreateSub(left, right);
            }
        case TokenType::Mul:
            if (is_floating_point) {
                return this->builder->CreateFMul(left, right);
            } else {
                return this->builder->CreateMul(left, right);
            }
        case TokenType::Div:
            if (is_floating_point) {
                return this->builder->CreateFDiv(left, right);
            } else {
                return this->builder->CreateSDiv(left, right);
            }
        case TokenType::Eq:
            if (is_floating_point) {
                return this->builder->CreateFCmpOEQ(left, right);
            } else {
                return this->builder->CreateICmpEQ(left, right);
            }
        case TokenType::Neq:
            if (is_floating_point) {
                return this->builder->CreateFCmpONE(left, right);
            } else {
                return this->builder->CreateICmpNE(left, right);
            }
        case TokenType::Gt:
            if (is_floating_point) {
                return this->builder->CreateFCmpOGT(left, right);
            } else {
                return this->builder->CreateICmpSGT(left, right);
            }
        case TokenType::Lt:
            if (is_floating_point) {
                return this->builder->CreateFCmpOLT(left, right);
            } else {
                return this->builder->CreateICmpSLT(left, right);
            }
        case TokenType::Gte:
            if (is_floating_point) {
                return this->builder->CreateFCmpOGE(left, right);
            } else {
                return this->builder->CreateICmpSGE(left, right);
            }
        case TokenType::Lte:
            if (is_floating_point) {
                return this->builder->CreateFCmpOLE(left, right);
            } else {
                return this->builder->CreateICmpSLE(left, right);
            }
        case TokenType::And:
            return this->builder->CreateAnd(this->cast(left, BooleanType), this->cast(right, BooleanType));
        case TokenType::Or:
            return this->builder->CreateOr(this->cast(left, BooleanType), this->cast(right, BooleanType));
        case TokenType::BinaryAnd:
            return this->builder->CreateAnd(left, right);
        case TokenType::BinaryOr:
            return this->builder->CreateOr(left, right);
        case TokenType::Xor:
            return this->builder->CreateXor(left, right);
        case TokenType::Lsh:
            return this->builder->CreateShl(left, right);
        case TokenType::Rsh:
            return this->builder->CreateLShr(left, right);
        default:
            _UNREACHABLE
    }

    return nullptr;
}

Value Visitor::visit(ast::CallExpr* expr) {
    Value callable = expr->callee->accept(*this);
    Function* func = callable.function;

    size_t argc = expr->args.size();
    bool is_constructor = false;

    if (callable.structure) {
        Struct* structure = callable.structure;
        llvm::AllocaInst* instance = this->builder->CreateAlloca(structure->type);

        if (!structure->has_method("constructor")) {
            if (structure->fields.size() != argc) {
                ERROR(expr->start, "Wrong number of arguments for constructor");
            }

            int i = 0;
            for (auto& argument : expr->args) {
                llvm::Value* arg = argument->accept(*this).unwrap(this, argument->start);
                llvm::Value* ptr = this->builder->CreateStructGEP(structure->type, instance, i);

                this->builder->CreateStore(arg, ptr);
                structure->locals[arg->getName().str()] = ptr;

                i++;
            }

            return instance;
        }

        callable.parent = instance; // `self` parameter for the constructor
        func = structure->methods["constructor"];

        callable.value = this->module->getFunction(func->name);
        is_constructor = true;
    } else {
        if (callable.type()->isPointerTy()) {
            llvm::Type* type = callable.type()->getPointerElementType();
            if (!type->isFunctionTy()) {
                ERROR(expr->start, "Cannot call non-function type");
            }

        } else if (!callable.type()->isFunctionTy()) {
            ERROR(expr->start, "Cannot call non-function type");
        }
    }

    if (callable.parent) {
        argc++;
    }

    llvm::Function* function = (llvm::Function*)callable.value;
    if (function->arg_size() != argc) {
        if (function->isVarArg()) {
            if (argc < 1) {
                ERROR(expr->start, "Function '{s}' expects at least one argument", callable.name());
            }
        } else {
            ERROR(expr->start, "Function '{s}' expects {i} arguments", callable.name(), function->arg_size());
        }
    }

    
    std::vector<llvm::Value*> args;
    if (callable.parent) {
        args.push_back(callable.parent);
    }

    int i = callable.parent ? 1 : 0;
    for (auto& arg : expr->args) {
        llvm::Value* value = arg->accept(*this).unwrap(this, expr->start);

        if ((function->isVarArg() && i == 0) || !function->isVarArg()) {
            llvm::Value* argument = function->getArg(i);

            Type* expected = Type::fromLLVMType(argument->getType());
            Type* type = Type::fromLLVMType(value->getType());

            if (!expected->is_compatible(type)) {
                std::string name = argument->getName().str();
                ERROR(expr->start, "Argument '{s}' of type '{t}' does not match expected type '{t}'", name, type, expected);
            } else {
                value = this->cast(value, expected);
            }
        }

        args.push_back(value);
        i++;
    }

    if (this->current_function) {
        this->current_function->calls.push_back(func);
    }

    llvm::Value* ret = this->builder->CreateCall(function, args);
    if (is_constructor) {
        return callable.parent;
    }

    return ret;
}

Value Visitor::visit(ast::ReturnExpr* expr) {
    if (!this->current_function) {
        ERROR(expr->start, "Cannot return from top-level code");
    }

    Function* func = this->current_function;
    if (expr->value) {
        llvm::Value* value = expr->value->accept(*this).unwrap(this, expr->start);
        func->branch->has_return = true;

        // Got rid of the typechecking for now.
        this->builder->CreateStore(value, func->return_value);
        this->builder->CreateBr(func->return_block);

        return nullptr;
    } else {
        if (!func->ret->isVoidTy()) {
            ERROR(expr->start, "Function '{s}' expects a return value", func->name);
        }

        func->branch->has_return = true;
        
        this->builder->CreateBr(func->return_block);
        return nullptr;
    }
}

Value Visitor::visit(ast::PrototypeExpr* expr) {
    std::string original = this->format_name(expr->name);
    auto pair = this->is_intrinsic(original);

    if (expr->linkage_specifier == ast::ExternLinkageSpecifier::C) {
        if (!pair.second) {
            pair.first = expr->name;
        }
    }

    llvm::Type* ret = this->get_llvm_type(expr->return_type);

    std::vector<llvm::Type*> args;
    for (auto& arg : expr->args) {
        args.push_back(this->get_llvm_type(arg.type));
    }

    llvm::Function* function = this->create_function(pair.first, ret, args, expr->has_varargs, llvm::Function::ExternalLinkage);
    int i = 0;
    for (auto& arg : function->args()) {
        arg.setName(expr->args[i++].name);
    }

    Function* func = new Function(pair.first, args, ret, pair.second, expr->attributes);
    if (this->current_struct) {
        func->parent = this->current_struct;
        this->current_struct->methods[expr->name] = func;
    } else if (this->current_namespace) {
        this->current_namespace->functions[expr->name] = func;
    } else {
        this->functions[original] = func;
    }

    func->value = function;
    return function;
}

Value Visitor::visit(ast::FunctionExpr* expr) {
    std::string name = this->format_name(expr->prototype->name);
    llvm::Function* function = this->module->getFunction(name);
    if (!function) {
        function = (llvm::Function*)(expr->prototype->accept(*this).value);
    }

    if (!function->empty()) { 
        ERROR(expr->start, "Function '{s}' already defined", name);
    }

    Function* func;
    if (this->current_struct) {
        func = this->current_struct->methods[expr->prototype->name];
    } else if (this->current_namespace) {
        func = this->current_namespace->functions[expr->prototype->name];
    } else {
        func = this->functions[name];
    }

    if (func->is_intrinsic) {
        ERROR(expr->start, "Cannot define intrinsic function '{s}'", name);
    }

    llvm::BasicBlock* block = llvm::BasicBlock::Create(this->context, "", function);
    this->builder->SetInsertPoint(block);

    Branch* branch = func->create_branch(name);
    func->branch = branch;

    func->return_value = this->builder->CreateAlloca(function->getReturnType());
    func->return_block = llvm::BasicBlock::Create(this->context);

    for (auto& arg : function->args()) {
        std::string name = arg.getName().str();

        llvm::AllocaInst* alloca_inst = this->create_alloca(function, arg.getType());
        this->builder->CreateStore(&arg, alloca_inst);

        func->locals[name] = alloca_inst;
    }
    
    this->current_function = func;
    if (!expr->body) {
        if (!func->ret->isVoidTy()) {
            ERROR(expr->start, "Function '{s}' expects a return value", name);
        }

        this->builder->CreateRetVoid();
    } else {
        expr->body->accept(*this);

        if (!func->has_return()) {
            if (func->ret->isVoidTy()) {
                this->builder->CreateRetVoid();
            } else {
                ERROR(expr->start, "Function '{s}' expects a return value", name);
            }
        } else {
            function->getBasicBlockList().push_back(func->return_block);
            this->builder->SetInsertPoint(func->return_block);

            if (func->ret->isVoidTy()) {
                this->builder->CreateRetVoid();
            } else {
                llvm::Value* value = this->builder->CreateLoad(function->getReturnType(), func->return_value);
                this->builder->CreateRet(value);
            }
        }
    }
    
    this->current_function = nullptr;

    bool error = llvm::verifyFunction(*function, &llvm::errs());
    assert((!error) && "Error while verifying function IR. Most likely a compiler bug.");

    this->fpm->run(*function);
    return function;
}

Value Visitor::visit(ast::IfExpr* expr) {
    llvm::Value* condition = expr->condition->accept(*this).unwrap(this, expr->condition->start);

    Function* func = this->current_function;
    llvm::Function* function = this->builder->GetInsertBlock()->getParent();

    llvm::BasicBlock* then = llvm::BasicBlock::Create(this->context, "", function);
    llvm::BasicBlock* else_ = llvm::BasicBlock::Create(this->context);

    this->builder->CreateCondBr(this->cast(condition, BooleanType), then, else_);
    this->builder->SetInsertPoint(then);

    Branch* branch = func->branch;
    func->branch = func->create_branch("if.then");

    expr->body->accept(*this);

    /*

    There are a couple of cases to take in consideration:

    1. There is an if body and no else body:
        1.1 The if body contains a return statement. 
            - In this case, we push the else block and set it as the insert point.
        1.2 The if body doesn't contain a return statement. 
            - In this case, we branch to the else block and set it as the insert point.
    2. There is an else body:
        2.1 The if body contains a return statement. 
            - In this case we push the else block and set it as the insert point and generate code for the else body.
        2.2 The if body doesn't contain a return statement.
            - In this case, we branch to a merge block and then set the else block as the insert point.
        2.3 The else body doesn't contain a return statement.
            - In this case, we branch to the merge block and set it as the insert point.
        2.4 The else body contains a return statement.
            - In this case, unlike 2.3 we don't branch to the merge block, we just set it as the insert point.

    */
    if (!expr->ebody) {
        if (!func->branch->has_return) {
            this->builder->CreateBr(else_);

            function->getBasicBlockList().push_back(else_);
            this->builder->SetInsertPoint(else_);

            func->branch = branch;
            return nullptr; 
        } else {
            function->getBasicBlockList().push_back(else_);
            this->builder->SetInsertPoint(else_);
        
            func->branch = branch;
            return nullptr;
        }
    }

    if (func->branch->has_return) {
        function->getBasicBlockList().push_back(else_);
        this->builder->SetInsertPoint(else_);

        expr->ebody->accept(*this);
        func->branch = branch;

        return nullptr;
    }

    llvm::BasicBlock* merge = llvm::BasicBlock::Create(this->context);
    this->builder->CreateBr(merge);

    function->getBasicBlockList().push_back(else_);
    this->builder->SetInsertPoint(else_);
    
    func->branch = func->create_branch("if.else");
    expr->ebody->accept(*this);

    if (func->branch->has_return) {
        function->getBasicBlockList().push_back(merge);
        this->builder->SetInsertPoint(merge);

        func->branch = branch;
        return nullptr;
    }

    this->builder->CreateBr(merge);

    function->getBasicBlockList().push_back(merge);
    this->builder->SetInsertPoint(merge);

    func->branch = branch;
    return nullptr;
}

Value Visitor::visit(ast::WhileExpr* expr) {
    llvm::Value* condition = expr->condition->accept(*this).unwrap(this, expr->condition->start);
    llvm::Function* function = this->builder->GetInsertBlock()->getParent();
    Function* func = this->current_function;

    llvm::BasicBlock* loop = llvm::BasicBlock::Create(this->context, "", function);
    llvm::BasicBlock* end = llvm::BasicBlock::Create(this->context);

    Branch* branch = func->branch;
    this->builder->CreateCondBr(condition, loop, end);

    function->getBasicBlockList().push_back(loop);
    this->builder->SetInsertPoint(loop);

    func->branch = func->create_branch("while.loop");
    expr->body->accept(*this);

    if (func->branch->has_return) {
        function->getBasicBlockList().push_back(end);
        this->builder->SetInsertPoint(end);

        func->branch = branch;
        return this->constants["null"];
    }

    loop = this->builder->GetInsertBlock();

    condition = expr->condition->accept(*this).unwrap(this, expr->condition->start);
    this->builder->CreateCondBr(condition, loop, end);

    function->getBasicBlockList().push_back(end);
    this->builder->SetInsertPoint(end);

    func->branch = branch;
    return this->constants["null"];
}

Value Visitor::visit(ast::StructExpr* expr) {
    if (expr->opaque) {
        llvm::StructType* type = llvm::StructType::create(this->context, expr->name);
        
        this->structs[expr->name] = new Struct(expr->name, true, type, {});
        return this->constants["null"];
    }

    std::vector<Struct*> parents;
    std::vector<llvm::Type*> types;
    std::map<std::string, llvm::Type*> fields;

    // We do this to avoid an infinite loop when the struct has a field of the same type as the struct itself.
    llvm::StructType* type = llvm::StructType::create(this->context, {}, expr->name, expr->packed);

    Struct* structure = new Struct(expr->name, false, type, {});
    this->structs[expr->name] = structure;

    for (auto& parent : expr->parents) {
        Value value = parent->accept(*this);
        if (!value.structure) {
            ERROR(parent->start, "Expected a structure");
        }

        std::vector<Struct*> expanded = expand(value.structure);
        parents.insert(parents.end(), expanded.begin(), expanded.end());

        structure->parents.push_back(value.structure);
        value.structure->children.push_back(structure);
    }

    if (this->current_namespace) {
        this->current_namespace->structs[expr->name] = structure;
    }

    for (auto& parent : parents) {
        for (auto& pair : parent->fields) {
            types.push_back(pair.second);
            fields[pair.first] = types.back();
        }

        for (auto& pair : parent->methods) {
            structure->methods[pair.first] = pair.second;
        }

        parent->children.push_back(structure);
    }
    
    for (auto& pair : expr->fields) {
        types.push_back(this->get_llvm_type(pair.second.type));
        fields[pair.first] = types.back();
    }

    type->setBody(types);
    structure->fields = fields;

    this->current_struct = structure;
    for (auto& method : expr->methods) { 
        assert(method->kind == ast::ExprKind::Function && "Expected a function. Might be a compiler bug.");
        method->accept(*this);
    }

    if (structure->has_method("constructor")) {
        Function* constructor = structure->methods["constructor"];
        if (constructor->ret != this->builder->getVoidTy()) {
            ERROR(expr->start, "Constructor must return void");
        }
    }

    this->current_struct = nullptr;
    return nullptr;
}

Value Visitor::visit(ast::ConstructorExpr* expr) {
    Value parent = expr->parent->accept(*this);
    if (!parent.structure) {
        ERROR(expr->start, "Parent must be a struct");
    }

    Struct* structure = parent.structure;
    std::map<int, llvm::Value*> args;

    int index = 0;
    for (auto& pair : expr->fields) {
        llvm::Value* value = pair.second->accept(*this).unwrap(this, pair.second->start);
        int i = index;
        if (!pair.first.empty()) {
            i = structure->get_field_index(pair.first);
            if (i < 0) {
                ERROR(expr->start, "Field '{s}' does not exist in struct '{s}'", pair.first, structure->name);
            }

            if (args.find(i) != args.end()) {
                ERROR(expr->start, "Field '{s}' already initialized", pair.first);
            }
        }

        args[i] = value;
        index++;
    }

    if (args.size() != structure->fields.size()) {
        ERROR(expr->start, "Expected {i} fields, found {i}", structure->fields.size(), args.size());
    }

    llvm::Value* instance = this->builder->CreateAlloca(structure->type);
    for (auto& pair : args) {
        llvm::Value* pointer = this->builder->CreateStructGEP(structure->type, instance, pair.first);
        this->builder->CreateStore(pair.second, pointer);
    }

    return instance;
}

Value Visitor::visit(ast::AttributeExpr* expr) {
    llvm::Value* value = expr->parent->accept(*this).unwrap(this, expr->parent->start);
    llvm::Type* type = value->getType();

    bool is_pointer = false;
    if (!type->isStructTy()) {
        if (!(type->isPointerTy() && type->getPointerElementType()->isStructTy())) { 
            ERROR(expr->start, "Expected a struct or a pointer to a struct");
        }

        type = type->getPointerElementType();
        is_pointer = true;
    }

    std::string name = type->getStructName().str();
    Struct* structure = this->structs[name];

    if (structure->has_method(expr->attribute)) {
        Function* function = structure->methods[expr->attribute];
        if (function->parent != structure) {
            Struct* parent = function->parent;
            value = this->builder->CreateBitCast(value, parent->type->getPointerTo());
        }

        llvm::Function* func = this->module->getFunction(function->name);
        
        function->used = true;
        return Value(func, value, function);
    }

    int index = structure->get_field_index(expr->attribute);
    if (index < 0) {
        ERROR(expr->start, "Field '{s}' does not exist in struct '{s}'", expr->attribute, name);
    }
    
    if (is_pointer) {
        llvm::Type* element = type->getStructElementType(index);

        llvm::Value* ptr = this->builder->CreateStructGEP(type, value, index);
        return this->builder->CreateLoad(element, ptr);
    } else {
        return this->builder->CreateExtractValue(value, index);
    }
}

Value Visitor::visit(ast::ElementExpr* expr) {
    llvm::Value* value = expr->value->accept(*this).unwrap(this, expr->value->start);
    llvm::Type* type = value->getType();

    bool is_pointer = false;
    if (!type->isArrayTy()) {
        if (!type->isPointerTy()) {
            ERROR(expr->start, "Expected an array or a pointer");
        }

        type = type->getPointerElementType();
        is_pointer = true;
    }

    llvm::Value* index = expr->index->accept(*this).unwrap(this, expr->index->start);
    if (is_pointer) {
        llvm::Type* element = type;
        if (type->isArrayTy()) {
            element = type->getArrayElementType();
        } 

        llvm::Value* ptr = this->builder->CreateGEP(type, value, index);
        return this->builder->CreateLoad(element, ptr);
    }
    
    return nullptr;
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
    llvm::Type* type = this->get_llvm_type(expr->type);
    llvm::TypeSize size = this->module->getDataLayout().getTypeAllocSize(type);

    return llvm::ConstantInt::get(llvm::Type::getInt32Ty(this->context), size);
}

Value Visitor::visit(ast::InlineAssemblyExpr* expr) {
    llvm::InlineAsm::AsmDialect dialect = llvm::InlineAsm::AD_ATT;

    bool align_stack = expr->attributes.has("alignstack");
    bool has_side_effects = expr->attributes.has("sideeffect");

    if (expr->attributes.has("intel")) {
        dialect = llvm::InlineAsm::AD_Intel;
    }

    std::string assembly = expr->assembly;
    std::string constraints;

    std::vector<llvm::Value*> args;
    std::vector<llvm::Type*> types;

    for (auto& pair : expr->inputs) {
        llvm::Value* value = pair.second->accept(*this).unwrap(this, pair.second->start);

        args.push_back(value);
        types.push_back(value->getType());

        constraints += pair.first + ",";
        delete pair.second; // Free the expression since we release it from the unique_ptr in the parser
    }

    llvm::Type* ret = this->builder->getVoidTy();

    for (auto& pair : expr->outputs) {
        llvm::Value* value = pair.second->accept(*this).unwrap(this, pair.second->start);
        ret = value->getType();

        constraints += pair.first + ",";
        delete pair.second;
    }

    if (!constraints.empty()) {
        constraints.pop_back();
    }

    llvm::FunctionType* type = llvm::FunctionType::get(ret, types, false);

    llvm::InlineAsm* inst = llvm::InlineAsm::get(type, assembly, constraints, has_side_effects, align_stack, dialect);
    return this->builder->CreateCall(inst, args);
}

Value Visitor::visit(ast::NamespaceExpr* expr) {
    std::string name = this->format_name(expr->name);
    if (this->namespaces.find(name) != this->namespaces.end()) {
        this->current_namespace = this->namespaces[name];
    } else {
        Namespace* ns = new Namespace(name);

        if (this->current_namespace) {
            this->current_namespace->namespaces[expr->name] = ns;
        }

        this->namespaces[name] = ns;
        this->current_namespace = ns;
    }

    for (auto& member : expr->members) {
        member->accept(*this);
    }

    this->current_namespace = nullptr;
    return nullptr;
}

Value Visitor::visit(ast::NamespaceAttributeExpr* expr) {
    // TODO: do this in a better way
    Value value = expr->parent->accept(*this);
    if (!value.ns && !value.structure) {
        ERROR(expr->start, "Expected a namespace or a struct");
    }

    if (value.structure) {
        if (value.structure->has_method(expr->attribute)) {
            Function* function = value.structure->methods[expr->attribute];
            function->used = true;

            return this->module->getFunction(function->name);;
        }

        std::string name = value.structure->name;
        ERROR(expr->start, "Field '{s}' does not exist in struct '{s}'", expr->attribute, name);
    }

    Namespace* ns = value.ns;
    if (ns->structs.find(expr->attribute) != ns->structs.end()) {
        return Value::with_struct(ns->structs[expr->attribute]);
    } else if (ns->functions.find(expr->attribute) != ns->functions.end()) {
        Function* func = ns->functions[expr->attribute];
        func->used = true;

        return Value::with_function(this->module->getFunction(func->name), func);
    } else if (ns->namespaces.find(expr->attribute) != ns->namespaces.end()) {
        return Value::with_namespace(ns->namespaces[expr->attribute]);
    } else if (ns->locals.find(expr->attribute) != ns->locals.end()) {
        return ns->locals[expr->attribute];
    } else {
        ERROR(expr->start, "Attribute '{s}' does not exist in namespace '{s}'", expr->attribute, ns->name); exit(1);
    }

    return nullptr;
}

Value Visitor::visit(ast::UsingExpr* expr) {
   Value parent = expr->parent->accept(*this);
   if (!parent.ns) {
        ERROR(expr->start, "Expected a namespace");
    }

    Namespace* ns = parent.ns;
    for (auto member : expr->members) {
        if (ns->structs.find(member) != ns->structs.end()) {
            this->structs[member] = ns->structs[member];
        } else if (ns->functions.find(member) != ns->functions.end()) {
            this->functions[member] = ns->functions[member];
        } else if (ns->namespaces.find(member) != ns->namespaces.end()) {
            this->namespaces[member] = ns->namespaces[member];
        } else {
            ERROR(expr->start, "Member '{s}' does not exist in namespace '{s}'", member, ns->name);
        }
    }

    return nullptr;
}