#include "visitor.h"

#include <sys/stat.h>

#include "utils.hpp"
#include "lexer.h"
#include "llvm.h"

#ifdef __GNUC__
    #define _UNREACHABLE __builtin_unreachable();
#elif _MSC_VER
    #define _UNREACHABLE __assume(false);
#else
    #define _UNREACHABLE
#endif

bool file_exists(const std::string& name) {
    struct stat buffer;
    return (stat(name.c_str(), &buffer) == 0);
}

Visitor::Visitor(std::string module) {
    this->includes[module] = {module, ModuleState::Initialized};

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

void Visitor::error(const std::string& message, Location* location) {
    // TODO: More descriptive errors.
    std::cerr << location->format() << " \u001b[1;31m" << "error: " "\u001b[0m" << message << std::endl;
    exit(1);
}

void Visitor::dump(llvm::raw_ostream& stream) {
    this->module->print(stream, nullptr);
}

std::pair<std::string, bool> Visitor::is_intrinsic(std::string name) {
    bool is_intrinsic = false;
    if (name.substr(0, 12) == "__intrinsic_") {
        is_intrinsic = true;
        name = name.substr(12);
        for (int i = 0; i < name.size(); i++) {
            if (name[i] == '_') {
                name[i] = '.';
            }
        }
    }

    return std::make_pair(name, is_intrinsic);
}

std::string Visitor::format_name(std::string name) {
    if (this->current_namespace) {
        name = this->current_namespace->name + "." + name;
    }

    if (this->current_struct) {
        name = this->current_struct->name + "." + name;
    }

    return name;
}

llvm::AllocaInst* Visitor::create_alloca(llvm::Function* function, llvm::Type* type) {
    llvm::IRBuilder<> tmp(&function->getEntryBlock(), function->getEntryBlock().begin());
    return tmp.CreateAlloca(type, nullptr);
}

llvm::Type* Visitor::get_llvm_type(Type* type) {
    std::string name = type->get_name();
    if (this->structs.find(name) != this->structs.end()) {
        return this->structs[name]->type->getPointerTo();
    }

    return type->to_llvm_type(this->context);
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

    llvm::GlobalVariable* variable = this->module->getGlobalVariable(name);
    return std::make_pair(variable, variable->isConstant());
}

llvm::Function* Visitor::get_function(std::string name) {
    if (this->current_struct) {
        Function* func = this->current_struct->methods[name];
        if (func) {
            func->used = true;
            return this->module->getFunction(func->name);
        }
    }

    if (this->current_namespace) {
        Function* func = this->current_namespace->functions[name];
        if (func) {
            func->used = true;
            return this->module->getFunction(func->name);
        }
    }

    if (this->functions.find(name) != this->functions.end()) {
        Function* func = this->functions[name];
        func->used = true;

        return this->module->getFunction(this->is_intrinsic(name).first);
    }

    return nullptr;

}

llvm::Value* Visitor::cast(llvm::Value* value, Type* type) {
    return this->cast(value, type->to_llvm_type(this->context));
}

llvm::Value* Visitor::cast(llvm::Value* value, llvm::Type* type) {
    if (value->getType() == type) {
        return value;
    }

    return this->builder->CreateBitCast(value, type);
}

llvm::Function* Visitor::create_struct_constructor(Struct* structure, std::vector<llvm::Type*> types) {
    std::string name = structure->name + ".constructor";
    llvm::Type* type = structure->type->getPointerTo();

    llvm::FunctionType* function_type = llvm::FunctionType::get(type, types, false);
    llvm::Function* constructor = llvm::Function::Create(function_type, llvm::Function::ExternalLinkage, name, this->module.get());

    int i = 0;
    for (auto field : structure->fields) {
        llvm::Argument* arg = constructor->getArg(i++);
        arg->setName(field.first);
    }

    llvm::BasicBlock* entry = llvm::BasicBlock::Create(this->context, "entry", constructor);
    this->builder->SetInsertPoint(entry);

    llvm::AllocaInst* alloca_inst = this->create_alloca(constructor, structure->type);
    for (int i = 0; i < structure->fields.size(); i++) {
        llvm::Value* ptr = this->builder->CreateStructGEP(structure->type, alloca_inst, i);
        llvm::Argument* arg = constructor->getArg(i);

        this->builder->CreateStore(arg, ptr);
        structure->locals[arg->getName().str()] = ptr;
    }

    this->builder->CreateRet(alloca_inst);

    this->functions[name] = new Function(name, types, structure->type, false);
    return constructor;
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
    return llvm::ConstantInt::get(this->context, llvm::APInt(32, expr->value, true));
}

Value Visitor::visit(ast::FloatExpr* expr) {
    return llvm::ConstantFP::get(this->context, llvm::APFloat(expr->value));
}

Value Visitor::visit(ast::StringExpr* expr) {
    return this->builder->CreateGlobalString(expr->value, ".str");
}

Value Visitor::visit(ast::BlockExpr* expr) {
    Value last = NULL;
    for (auto& stmt : expr->block) {
        if (!stmt) {
            continue;
        }

        last = stmt->accept(*this);
    }

    return last;
}

Value Visitor::visit(ast::VariableExpr* expr) {
    if (this->structs.count(expr->name)) {
        return this->structs[expr->name]->constructor;
    } else if (this->namespaces.count(expr->name)) {
        return this->constants["null"];
    }

    if (this->current_function) {
        llvm::AllocaInst* variable = this->current_function->locals[expr->name];
        if (variable) {
            return this->builder->CreateLoad(variable->getAllocatedType(), variable);;
        }
    }

    if (this->current_struct) {
        llvm::Value* variable = this->current_struct->locals[expr->name];
        if (variable) {
            return variable;
        }
    }

    llvm::Function* function = this->get_function(expr->name);
    if (function) {
        return function;
    }
    
    llvm::GlobalVariable* global = this->module->getGlobalVariable(expr->name);
    if (global) {
        return this->builder->CreateLoad(global->getValueType(), global);
    }

    this->error("Symbol " + expr->name + " is not defined", expr->start);
}

Value Visitor::visit(ast::VariableAssignmentExpr* expr) {
    Value value = expr->value->accept(*this);
    llvm::Type* type;

    if (!expr->type) {
        type = value.type();
    } else {
        type = expr->type->to_llvm_type(this->context);
    }

    if (!this->current_function) {
        this->module->getOrInsertGlobal(expr->name, type);
        llvm::GlobalVariable* variable = this->module->getNamedGlobal(expr->name);

        // TODO: let users specify other options somehow and this applies to other stuff too, not just this
        variable->setLinkage(llvm::GlobalValue::ExternalLinkage);
        variable->setInitializer((llvm::Constant*)value.value);

        return value;
    }

    llvm::Function* func = this->builder->GetInsertBlock()->getParent();
    llvm::AllocaInst* alloca_inst = this->create_alloca(func, type);

    this->builder->CreateStore(value.value, alloca_inst);
    this->current_function->locals[expr->name] = alloca_inst;

    return alloca_inst;
}

Value Visitor::visit(ast::ConstExpr* expr) {
    Value value = expr->value->accept(*this);
    llvm::Type* type;

    if (!expr->type) {
        type = value.type();
    } else {
        type = expr->type->to_llvm_type(this->context);
    }

    if (!this->current_function) {
        this->module->getOrInsertGlobal(expr->name, type);
        llvm::GlobalVariable* variable = this->module->getNamedGlobal(expr->name);

        variable->setConstant(true);
        variable->setInitializer((llvm::Constant*)value.value);

        this->constants[expr->name] = variable;
        return variable;
    } else {
        llvm::Function* func = this->builder->GetInsertBlock()->getParent();
        llvm::AllocaInst* alloca_inst = this->create_alloca(func, type);

        this->builder->CreateStore(value.value, alloca_inst);
        this->current_function->constants[expr->name] = alloca_inst;

        return alloca_inst;
    }
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
    for (int i = 1; i < elements.size(); i++) {
        if (elements[i].type() != etype) {
            this->error("Array elements must be of the same type", expr->start);
        }
    }

    llvm::Function* parent = this->builder->GetInsertBlock()->getParent();

    llvm::ArrayType* type = llvm::ArrayType::get(etype, elements.size());
    llvm::AllocaInst* array = this->create_alloca(parent, type);

    for (int i = 0; i < elements.size(); i++) {
        std::vector<llvm::Value*> indices = {
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(this->context), llvm::APInt(32, 0, true)),
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(this->context), llvm::APInt(32, i))
        };

        llvm::Value* ptr = this->builder->CreateGEP(type, array, indices);
        this->builder->CreateStore(elements[i].value, ptr);
    }

    return array;
}

Value Visitor::visit(ast::UnaryOpExpr* expr) {
    llvm::Value* value = expr->value->accept(*this).value;
    bool is_floating_point = value->getType()->isFloatingPointTy();
    bool is_numeric = value->getType()->isIntegerTy() || is_floating_point;

    switch (expr->op) {
        case TokenType::PLUS:
            if (!is_numeric) {
                this->error("Unary operator + only be applied to numeric types", expr->start);
            }

            return value;
        case TokenType::MINUS:
            if (!is_numeric) {
                this->error("Unary operator - only be applied to numeric types", expr->start);
            }

            if (is_floating_point) {
                return this->builder->CreateFNeg(value);
            } else {
                return this->builder->CreateNeg(value);
            }
        case TokenType::NOT:
            return this->builder->CreateNot(this->cast(value, BooleanType));
        case TokenType::BINARY_NOT:
            return this->builder->CreateNot(value);
        case TokenType::MUL: {
            if (!value->getType()->isPointerTy()) {
                this->error("Unary operator * only be applied to pointer types", expr->start);
            }

            llvm::Type* type = value->getType()->getPointerElementType();
            return this->builder->CreateLoad(type, value);
        }
        case TokenType::BINARY_AND: {
            llvm::AllocaInst* alloca_inst = this->builder->CreateAlloca(value->getType());
            this->builder->CreateStore(value, alloca_inst);

            return alloca_inst;
        }
        case TokenType::INC: {
            if (!is_numeric) {
                this->error("Unary operator ++ only be applied to numeric types", expr->start);
            }

            llvm::Value* one = llvm::ConstantInt::get(value->getType(), llvm::APInt(value->getType()->getIntegerBitWidth(), 1, true));
            llvm::Value* result = this->builder->CreateAdd(value, one);

            this->builder->CreateStore(result, value);
            return result;
        }
        case TokenType::DEC: {
            if (!is_numeric) {
                this->error("Unary operator -- only be applied to numeric types", expr->start);
            }

            llvm::Value* one = llvm::ConstantInt::get(value->getType(), llvm::APInt(value->getType()->getIntegerBitWidth(), 1, true));
            return this->builder->CreateSub(value, one);
        }
        default:
            _UNREACHABLE
    }
}

Value Visitor::visit(ast::BinaryOpExpr* expr) {
    // Assingment is a special case.
    if (expr->op == TokenType::ASSIGN) {
        llvm::Value* variable = expr->left->accept(*this).value;
        llvm::Value* value = expr->right->accept(*this).value;

        this->builder->CreateStore(value, variable);
        return value;
    }

    llvm::Value* left = expr->left->accept(*this).value;
    llvm::Value* right = expr->right->accept(*this).value;

    llvm::Type* ltype = left->getType();
    llvm::Type* rtype = right->getType();

    if (ltype != rtype) {
        if (Type::from_llvm_type(ltype)->is_compatible(rtype)) {
            right = this->cast(right, ltype);
        } else {
            this->error("Incompatible types", expr->start);
        }
    }

    bool is_floating_point = ltype->isFloatingPointTy();
    switch (expr->op) {
        case TokenType::PLUS:
            if (is_floating_point) {
                return this->builder->CreateFAdd(left, right);
            } else {
                return this->builder->CreateAdd(left, right);
            }
        case TokenType::MINUS:
            if (is_floating_point) {
                return this->builder->CreateFSub(left, right);
            } else {
                return this->builder->CreateSub(left, right);
            }
        case TokenType::MUL:
            if (is_floating_point) {
                return this->builder->CreateFMul(left, right);
            } else {
                return this->builder->CreateMul(left, right);
            }
        case TokenType::DIV:
            if (is_floating_point) {
                return this->builder->CreateFDiv(left, right);
            } else {
                return this->builder->CreateSDiv(left, right);
            }
        case TokenType::EQ:
            if (is_floating_point) {
                return this->builder->CreateFCmpOEQ(left, right);
            } else {
                return this->builder->CreateICmpEQ(left, right);
            }
        case TokenType::NEQ:
            if (is_floating_point) {
                return this->builder->CreateFCmpONE(left, right);
            } else {
                return this->builder->CreateICmpNE(left, right);
            }
        case TokenType::GT:
            if (is_floating_point) {
                return this->builder->CreateFCmpOGT(left, right);
            } else {
                return this->builder->CreateICmpSGT(left, right);
            }
        case TokenType::LT:
            if (is_floating_point) {
                return this->builder->CreateFCmpOLT(left, right);
            } else {
                return this->builder->CreateICmpSLT(left, right);
            }
        case TokenType::GTE:
            if (is_floating_point) {
                return this->builder->CreateFCmpOGE(left, right);
            } else {
                return this->builder->CreateICmpSGE(left, right);
            }
        case TokenType::LTE:
            if (is_floating_point) {
                return this->builder->CreateFCmpOLE(left, right);
            } else {
                return this->builder->CreateICmpSLE(left, right);
            }
        case TokenType::AND:
            return this->builder->CreateAnd(this->cast(left, BooleanType), this->cast(right, BooleanType));
        case TokenType::OR:
            return this->builder->CreateOr(this->cast(left, BooleanType), this->cast(right, BooleanType));
        case TokenType::BINARY_AND:
            return this->builder->CreateAnd(left, right);
        case TokenType::BINARY_OR:
            return this->builder->CreateOr(left, right);
        case TokenType::XOR:
            return this->builder->CreateXor(left, right);
        case TokenType::LSH:
            return this->builder->CreateShl(left, right);
        case TokenType::RSH:
            return this->builder->CreateLShr(left, right);
        default:
            _UNREACHABLE
    }
}

Value Visitor::visit(ast::CallExpr* expr) {
    Value callable = expr->callee->accept(*this);
    if (callable.type()->isPointerTy()) {
        llvm::Type* type = callable.type()->getPointerElementType();
        if (!type->isFunctionTy()) {
            this->error("Cannot call non-function type", expr->start);
        }

    } else if (!callable.type()->isFunctionTy()) {
        this->error("Cannot call non-function type", expr->start);
    }

    Function* func = this->functions[callable.name()];
    if (func) {
        func->used = true;
    }

    size_t argc = expr->args.size();
    if (callable.parent) {
        argc++;
    }

    llvm::Function* function = this->module->getFunction(callable.name());
    if (function->arg_size() != argc) {
        if (function->isVarArg()) {
            if (argc < 1) {
                this->error("Function " + callable.name() + " expects atleast 1 argument", expr->start);
                exit(1);
            }
        } else {
            this->error("Function " + callable.name() + " expects " + std::to_string(function->arg_size()) + " arguments", expr->start);
            exit(1);
        }
    }

    std::vector<llvm::Value*> args;
    if (callable.parent) {
        args.push_back(callable.parent);
    }

    int i = 0;
    for (auto& arg : expr->args) {
        llvm::Value* value = arg->accept(*this).value;
        if ((function->isVarArg() && i == 0) || !function->isVarArg()) {
            llvm::Type* type = function->getArg(i++)->getType();

            if (Type::from_llvm_type(type)->is_compatible(value->getType())) {
                value = this->cast(value, type);
            } else {
                this->error("Incompatible types", arg->start);
            }
        }

        args.push_back(value);
    }

    return this->builder->CreateCall(function, args);
}

Value Visitor::visit(ast::ReturnExpr* expr) {
    if (!this->current_function) {
        this->error("Return outside function", expr->start);
    }

    Function* func = this->current_function;
    if (expr->value) {
        llvm::Value* value = expr->value->accept(*this).value;
        func->has_return = true;

        // Got rid of the typechecking for now.
        return this->builder->CreateRet(value);
    } else {
        if (!func->ret->isVoidTy()) {
            this->error("Function " + func->name + " expects a return value", expr->start);
        }

        func->has_return = true;
        return this->builder->CreateRetVoid();
    }
}

Value Visitor::visit(ast::PrototypeExpr* expr) {
    std::string original = this->format_name(expr->name);
    auto pair = this->is_intrinsic(original);

    llvm::Type* ret = this->get_llvm_type(expr->return_type);
    std::vector<llvm::Type*> args;

    for (auto& arg : expr->args) {
        args.push_back(this->get_llvm_type(arg.type));
    }

    llvm::FunctionType* function_t = llvm::FunctionType::get(ret, args, expr->has_varargs);
    llvm::Function* function = llvm::Function::Create(function_t, llvm::Function::ExternalLinkage, pair.first, this->module.get());

    int i = 0;
    for (auto& arg : function->args()) {
        arg.setName(expr->args[i++].name);
    }
    
    this->functions[original] = new Function(pair.first, args, ret, pair.second);
    return function;
}

Value Visitor::visit(ast::FunctionExpr* expr) {
    std::string name = this->format_name(expr->prototype->name);
    llvm::Function* function = this->module->getFunction(name);
    if (!function) {
        function = (llvm::Function*)(expr->prototype->accept(*this).value);
    }

    if (!function->empty()) { 
        this->error("Function " + name + " is already defined", expr->prototype->start);
    }

    Function* func = this->functions[name];
    if (func->is_intrinsic) {
        this->error("Function " + name + " is an LLVM intrinsic", expr->prototype->start);
    }

    llvm::BasicBlock* block = llvm::BasicBlock::Create(this->context, "entry", function);
    this->builder->SetInsertPoint(block);

    for (auto& arg : function->args()) {
        std::string name = arg.getName().str();

        llvm::AllocaInst* alloca_inst = this->create_alloca(function, arg.getType());
        this->builder->CreateStore(&arg, alloca_inst);

        func->locals[name] = alloca_inst;
    }
    
    this->current_function = func;
    if (!expr->body) {
        if (!func->ret->isVoidTy()) {
            this->error("Function " + name + " expects a return value", expr->end);
        }

        this->builder->CreateRetVoid();
    } else {
        llvm::Value* value = expr->body->accept(*this).value;
        if (!func->has_return && value) {          
            if (value->getType() == func->ret) {
                this->builder->CreateRet(value);
            }

            if (func->ret->isVoidTy()) {
                this->builder->CreateRetVoid();
            } else {
                this->error("Function " + name + " expects a return value", expr->end);
                exit(1);
            }
        }
    }
    
    this->current_function = nullptr;
    bool error = llvm::verifyFunction(*function, &llvm::errs());
    if (error) {
        exit(1); // To avoid a segfault when calling `fpm->run(*function)`
    }

    this->fpm->run(*function);
    return function;
}

Value Visitor::visit(ast::IfExpr* expr) {
    llvm::Value* condition = expr->condition->accept(*this).value;
    llvm::Function* function = this->builder->GetInsertBlock()->getParent();

    llvm::BasicBlock* then = llvm::BasicBlock::Create(this->context, "then", function);
    llvm::BasicBlock* else_ = llvm::BasicBlock::Create(this->context, "else");
    llvm::BasicBlock* merge = llvm::BasicBlock::Create(this->context, "merge");

    this->builder->CreateCondBr(condition, then, else_);
    this->builder->SetInsertPoint(then);

    expr->body->accept(*this);

    this->builder->CreateBr(merge);
    then = this->builder->GetInsertBlock();

    function->getBasicBlockList().push_back(else_);
    this->builder->SetInsertPoint(else_);

    if (expr->ebody) {
        expr->ebody->accept(*this);
    }

    this->builder->CreateBr(merge);
    else_ = this->builder->GetInsertBlock();

    function->getBasicBlockList().push_back(merge);
    this->builder->SetInsertPoint(merge);

    llvm::PHINode* phi = this->builder->CreatePHI(BooleanType->to_llvm_type(this->context), 2, "if");

    phi->addIncoming(llvm::ConstantInt::getTrue(this->context), then);
    phi->addIncoming(llvm::ConstantInt::getFalse(this->context), else_);

    return phi;
}

Value Visitor::visit(ast::WhileExpr* expr) {
    llvm::Value* condition = expr->condition->accept(*this).value;
    llvm::Function* function = this->builder->GetInsertBlock()->getParent();

    llvm::BasicBlock* loop = llvm::BasicBlock::Create(this->context, "loop", function);
    llvm::BasicBlock* end = llvm::BasicBlock::Create(this->context, "end");

    this->builder->CreateCondBr(condition, loop, end);

    function->getBasicBlockList().push_back(loop);
    this->builder->SetInsertPoint(loop);

    expr->body->accept(*this);

    loop = this->builder->GetInsertBlock();
    this->builder->CreateCondBr(condition, loop, end);

    function->getBasicBlockList().push_back(end);
    this->builder->SetInsertPoint(end);

    return this->constants["null"];
}

Value Visitor::visit(ast::StructExpr* expr) {
    std::vector<llvm::Type*> types;
    std::map<std::string, llvm::Type*> fields;
    
    for (auto& pair : expr->fields) {
        types.push_back(pair.second.type->to_llvm_type(this->context));
        fields[pair.first] = types.back();
    }

    llvm::StructType* type = llvm::StructType::create(this->context, types, expr->name, expr->packed);
    type->setName(expr->name);

    Struct* structure = new Struct(expr->name, type, fields);
    this->structs[expr->name] = structure;

    structure->constructor = this->create_struct_constructor(structure, types);
    this->current_struct = structure;

    for (auto& method : expr->methods) {
        llvm::Value* value = method->accept(*this).value;
        std::string name = this->format_name(method->prototype->name);

        structure->methods[method->prototype->name] = this->functions[name];
    }

    this->current_struct = nullptr;
    return llvm::ConstantInt::getNullValue(llvm::IntegerType::getInt1Ty(this->context));;
}

Value Visitor::visit(ast::AttributeExpr* expr) {
    llvm::Value* value = expr->parent->accept(*this).value;
    llvm::Type* type = value->getType();

    bool is_pointer = false;
    if (!type->isStructTy()) {
        if (!(type->isPointerTy() && type->getPointerElementType()->isStructTy())) { 
            std::cerr << "Attribute is not a struct" << std::endl;
            exit(1);
        }

        type = type->getPointerElementType();
        is_pointer = true;
    }

    std::string name = type->getStructName().str();
    Struct* structure = this->structs[name];
    
    if (structure->methods.find(expr->attribute) != structure->methods.end()) {
        Function* function = structure->methods[expr->attribute];
        llvm::Function* func = this->module->getFunction(function->name);
        
        return Value(func, value);
    }

    auto iter = structure->fields.find(expr->attribute);
    if (iter == structure->fields.end()) {
        std::cerr << "Attribute " << expr->attribute << " does not exist" << std::endl;
        exit(1);
    }
    
    int index = std::distance(structure->fields.begin(), iter);
    if (is_pointer) {
        llvm::Type* element = type->getStructElementType(index);

        llvm::Value* ptr = this->builder->CreateStructGEP(type, value, index);
        return this->builder->CreateLoad(element, ptr);
    } else {
        return this->builder->CreateExtractValue(value, index);
    }
}

Value Visitor::visit(ast::ElementExpr* expr) {
    llvm::Value* value = expr->value->accept(*this).value;
    llvm::Type* type = value->getType();

    bool is_pointer = false;
    if (!type->isArrayTy()) {
        if (!type->isPointerTy()) {
            std::cerr << "Indexing is only allowed on arrays and pointers" << std::endl;
            exit(1);
        }

        type = type->getPointerElementType();
        is_pointer = true;
    }

    llvm::Value* index = expr->index->accept(*this).value;
    if (is_pointer) {
        llvm::Type* element = type;
        if (type->isArrayTy()) {
            element = type->getArrayElementType();
        } 

        llvm::Value* ptr = this->builder->CreateGEP(type, value, index);
        return this->builder->CreateLoad(element, ptr);
    }
    
    TODO("Implement element indexing for non-pointer types");
}

Value Visitor::visit(ast::IncludeExpr* expr) {
    std::string path = expr->path;
    if (!file_exists(path)) {
        if (!file_exists("library/std/" + path)) {
            std::cerr << "File " << path << " not found" << std::endl;
            exit(1);
        } else {
            path = "library/std/" + path;
        }
    }
    
    Module module = {path, ModuleState::Initialized};
    if (this->includes.find(path) != this->includes.end()) {
        module = this->includes[path];
        if (!module.is_ready()) {
            std::cerr << "Circular dependency detected" << std::endl;
            exit(1);
        }

        return this->constants["null"];
    }

    this->includes[path] = module;

    std::ifstream file(path);
    Lexer lexer(file, path);

    auto program = Parser(lexer.lex()).statements();
    
    this->visit(std::move(program));
    module.state = ModuleState::Compiled;

    return this->constants["null"];
}

Value Visitor::visit(ast::NamespaceExpr* expr) {
    std::string name = this->format_name(expr->name);
    Namespace* ns = new Namespace(name);

    this->namespaces[name] = ns;
    this->current_namespace = ns;

    for (auto& function : expr->functions) {
        llvm::Value* value = function->accept(*this).value;
        std::string name = this->format_name(function->prototype->name);

        ns->functions[function->prototype->name] = this->functions[name];
    }

    for (auto& structure : expr->structs) {
        llvm::Value* value = structure->accept(*this).value;
        std::string name = this->format_name(structure->name);

        ns->structs[structure->name] = this->structs[name];
    }

    for (auto& namespace_ : expr->namespaces) {
        llvm::Value* value = namespace_->accept(*this).value;
        std::string name = this->format_name(namespace_->name);

        ns->namespaces[namespace_->name] = this->namespaces[name];
    }

    this->current_namespace = nullptr;
    return this->constants["null"];
}

Value Visitor::visit(ast::NamespaceAttributeExpr* expr) {
    // TODO: do this in a better way
    ast::VariableExpr* variable = dynamic_cast<ast::VariableExpr*>(expr->parent.get());
    if (!variable) {
        std::cerr << "Namespace attribute is not a variable" << std::endl;
        exit(1);
    }

    Namespace* ns = this->namespaces[variable->name];
    if (!ns) {
        std::cerr << "Namespace " << variable->name << " does not exist" << std::endl;
        exit(1);
    }

    if (ns->structs.count(expr->attribute)) {
        return ns->structs[expr->attribute]->constructor;
    } else if (ns->functions.count(expr->attribute)) {
        Function* func = ns->functions[expr->attribute];
        return this->module->getFunction(func->name);
    } else if (ns->namespaces.count(expr->attribute)) {
        return nullptr;

    } else {
        std::cerr << "Attribute " << expr->attribute << " does not exist" << std::endl;
        exit(1);
    }
}