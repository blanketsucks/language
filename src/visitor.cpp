#include "visitor.h"

#include <sys/stat.h>
#include "utils.hpp"
#include "lexer.h"
#include "llvm.h"

bool file_exists(const std::string& name) {
    struct stat buffer;
    return (stat(name.c_str(), &buffer) == 0);
}

std::pair<std::string, bool> function_is_internal(std::string name) {
    bool is_internal = false;
    if (name.substr(0, 11) == "__internal_") {
        is_internal = true;
        name = name.substr(11);
        for (int i = 0; i < name.size(); i++) {
            if (name[i] == '_') {
                name[i] = '.';
            }
        }
    }

    return std::make_pair(name, is_internal);
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
}

void Visitor::dump(llvm::raw_ostream& stream) {
    this->module->print(stream, nullptr);
}

llvm::AllocaInst* Visitor::create_alloca(llvm::Function* function, llvm::Type* type) {
    llvm::IRBuilder<> tmp(&function->getEntryBlock(), function->getEntryBlock().begin());
    return tmp.CreateAlloca(type, nullptr);
}

llvm::Type* Visitor::get_llvm_type(Type* type) {
    std::string name = type->get_name();
    if (this->structs.find(name) != this->structs.end()) {
        return this->structs[name].type;
    }

    return type->to_llvm_type(this->context);
}

llvm::Value* Visitor::get_variable(std::string name) {
    if (this->current_function) {
        llvm::Value* value = this->current_function->locals[name];
        if (value) {
            return value;
        }
    }

    return this->module->getGlobalVariable(name);
}

llvm::Value* Visitor::cast(llvm::Value* value, Type* type) {
    llvm::Type* target = type->to_llvm_type(this->context);
    if (value->getType() == target) {
        return value;
    }

    return this->builder->CreateBitCast(value, target);
}

void Visitor::visit(std::unique_ptr<ast::Program> program) {
    for (auto& expr : program->ast) {
        expr->accept(*this);
    }
}

llvm::Value* Visitor::visit(ast::IntegerExpr* expr) {
    return llvm::ConstantInt::get(this->context, llvm::APInt(32, expr->value, true));
}

llvm::Value* Visitor::visit(ast::StringExpr* expr) {
    return llvm::ConstantDataArray::getString(this->context, expr->value);
}

llvm::Value* Visitor::visit(ast::BlockExpr* expr) {
    llvm::Value* last = nullptr;
    for (auto& stmt : expr->block) {
        if (stmt) {
            last = stmt->accept(*this);
        }
    }

    return last;
}

llvm::Value* Visitor::visit(ast::VariableExpr* expr) {
    if (this->current_function) {
        llvm::AllocaInst* variable = this->current_function->locals[expr->name];
        if (variable) {
            return this->builder->CreateLoad(variable->getAllocatedType(), variable);;
        }
    }
    
    llvm::GlobalVariable* global = this->module->getGlobalVariable(expr->name);
    if (global) {
        return this->builder->CreateLoad(global->getValueType(), global);
    }

    std::cerr << "Symbol " << expr->name << " is not defined" << std::endl;
    exit(1);
}

llvm::Value* Visitor::visit(ast::VariableAssignmentExpr* expr) {
    llvm::Value* value = expr->value->accept(*this);
    llvm::Type* type;

    if (!expr->type) {
        type = value->getType();
    } else {
        type = expr->type->to_llvm_type(this->context);
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

    return value;
}

llvm::Value* Visitor::visit(ast::ArrayExpr* expr) {
    std::vector<llvm::Value*> elements;
    for (auto& element : expr->elements) {
        auto constant = element->accept(*this);
        elements.push_back(constant);
    }

    if (elements.size() == 0) {
        return llvm::Constant::getNullValue(llvm::ArrayType::get(llvm::Type::getInt32Ty(this->context), 0));
    }

    // The type of an array is determined from the first element.
    llvm::Type* etype = elements[0]->getType();
    for (int i = 1; i < elements.size(); i++) {
        if (elements[i]->getType() != etype) {
            std::cerr << "Array elements must be of the same type" << std::endl;
            exit(1);
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

        auto ptr = this->builder->CreateGEP(type, array, indices);
        this->builder->CreateStore(elements[i], ptr);
    }

    return array;
}

llvm::Value* Visitor::visit(ast::UnaryOpExpr* expr) {
    llvm::Value* value = expr->value->accept(*this);

    bool is_floating_point = value->getType()->isFloatingPointTy();
    if (!(value->getType()->isIntegerTy() || is_floating_point)) {
        std::cerr << "Unary operators can only be applied to numeric types" << std::endl;
        exit(1);
    }
    switch (expr->op) {
        case TokenType::PLUS:
            return value;
        case TokenType::MINUS:
            if (is_floating_point) {
                return this->builder->CreateFNeg(value);
            } else {
                return this->builder->CreateNeg(value);
            }
        case TokenType::NOT:
            return this->builder->CreateNot(this->cast(value, BooleanType));
        case TokenType::BINARY_NOT:
            return this->builder->CreateNot(value);
        default:
            std::cerr << "Unary operator not supported" << std::endl;
            exit(1);
    }
}

llvm::Value* Visitor::visit(ast::BinaryOpExpr* expr) {
    // Assingment is a special case.
    if (expr->op == TokenType::ASSIGN) {
        ast::VariableAssignmentExpr* lhs = dynamic_cast<ast::VariableAssignmentExpr*>(expr->left.get());
        if (!lhs) {
            std::cerr << "Left side of assignment must be a variable" << std::endl;
            exit(1);
        }

        llvm::Value* variable = this->get_variable(lhs->name);
        if (!variable) {
            std::cerr << "Symbol " << lhs->name << " is not defined" << std::endl;
            exit(1);
        }

        llvm::Value* value = expr->right->accept(*this);

        this->builder->CreateStore(value, variable);
        return value;
    }

    llvm::Value* left = expr->left->accept(*this);
    llvm::Value* right = expr->right->accept(*this);

    llvm::Type* ltype = left->getType();
    llvm::Type* rtype = right->getType();

    if (ltype != rtype) {
        if (Type::from_llvm_type(ltype)->is_compatible(rtype)) {
            // TODO: Type cast or something idk
        } else {
            std::cerr << "Incompatible types" << std::endl;
            exit(1);
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
            std::cerr << "Unknown binary operator" << std::endl;
            exit(1);
    }
}

llvm::Value* Visitor::visit(ast::CallExpr* expr) {
    llvm::Function* function = this->module->getFunction(function_is_internal(expr->callee).first);
    if (!function) {
        std::cerr << "Function " << expr->callee << " is not defined" << std::endl;
        exit(1);
    }

    if (function->arg_size() != expr->args.size()) {
        std::cerr << "Function " << expr->callee << " expects " << function->arg_size() << " arguments" << std::endl;
        exit(1);
    }

    std::vector<llvm::Value*> args;
    for (auto& arg : expr->args) {
        args.push_back(arg->accept(*this));
    }

    return this->builder->CreateCall(function, args);
}

llvm::Value* Visitor::visit(ast::ReturnExpr* expr) {
    if (!this->current_function) {
        std::cerr << "Return outside of function" << std::endl;
        exit(1);
    }

    Function* func = this->current_function;
    if (expr->value) {
        llvm::Value* value = expr->value->accept(*this);
        func->has_return = true;

        // Got rid of the typechecking for now.
        return this->builder->CreateRet(value);
    } else {
        if (!func->ret->isVoidTy()) {
            std::cerr << "Function " << func->name << " expects a return value" << std::endl;
            exit(1);
        }

        func->has_return = true;
        return this->builder->CreateRetVoid();
    }
}

llvm::Value* Visitor::visit(ast::PrototypeExpr* expr) {
    std::string original = expr->name;
    auto pair = function_is_internal(expr->name);

    std::string name = pair.first;
    bool is_internal = pair.second;

    llvm::Type* ret = this->get_llvm_type(expr->return_type);

    std::vector<llvm::Type*> args;
    for (auto& arg : expr->args) {
        args.push_back(this->get_llvm_type(arg.type));
    }

    llvm::FunctionType* function_t = llvm::FunctionType::get(ret, args, false);
    llvm::Function* function = llvm::Function::Create(function_t, llvm::Function::ExternalLinkage, name, this->module.get());

    int i = 0;
    for (auto& arg : function->args()) {
        arg.setName(expr->args[i++].name);
    }
    
    this->functions[original] = new Function(name, args, ret, false);
    return function;
}

llvm::Value* Visitor::visit(ast::FunctionExpr* expr) {
    std::string name = expr->prototype->name;
    llvm::Function* function = this->module->getFunction(name);
    if (!function) {
        function = (llvm::Function*)expr->prototype->accept(*this);
    }

    if (!function->empty()) { 
        std::cerr << "Function " << name << " already defined" << std::endl;
        exit(1);
    }

    Function* func = this->functions[name];
    if (func->is_internal) {
        std::cerr << "Function " << name << " is internal" << std::endl;
        exit(1);
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
            std::cerr << "Function " << name << " expects a return value" << std::endl;
            exit(1);
        }

        this->builder->CreateRetVoid();
    } else {
        llvm::Value* value = expr->body->accept(*this);
        if (!func->has_return && value) {          
            if (value->getType() != func->ret) {
                std::cerr << "Incompatible return type" << std::endl;
                exit(1);
            }

            this->builder->CreateRet(value);
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

llvm::Value* Visitor::visit(ast::IfExpr* expr) {
    llvm::Value* condition = expr->condition->accept(*this);
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

llvm::Value* Visitor::visit(ast::StructExpr* expr) {
    std::vector<llvm::Type*> types;
    std::map<std::string, llvm::Type*> fields;
    
    for (auto& pair : expr->fields) {
        types.push_back(pair.second.type->to_llvm_type(this->context));
        fields[pair.first] = types.back();
    }

    auto type = llvm::StructType::create(this->context, types, expr->name, expr->packed);
    type->setName(expr->name);

    this->structs[expr->name] = {expr->name, type, fields};
    return llvm::ConstantInt::getNullValue(llvm::IntegerType::getInt1Ty(this->context));;
}

llvm::Value* Visitor::visit(ast::ConstructorExpr* expr) {
    std::string name = expr->name;
    Struct structure = this->structs[name];

    std::vector<llvm::Value*> args;
    for (auto& arg : expr->args) {
        args.push_back(arg->accept(*this));
    }

    llvm::Function* parent = this->builder->GetInsertBlock()->getParent();
    llvm::AllocaInst* alloca_inst = this->create_alloca(parent, structure.type);
    int index = 0;
    for (auto& arg : args) {
        std::vector<llvm::Value*> indices = {
            llvm::ConstantInt::get(this->context, llvm::APInt(32, 0)),
            llvm::ConstantInt::get(this->context, llvm::APInt(32, index))
        };

        auto ptr = this->builder->CreateGEP(structure.type, alloca_inst, indices);
        this->builder->CreateStore(arg, ptr);

        index++;
    }

    return alloca_inst;
}

llvm::Value* Visitor::visit(ast::AttributeExpr* expr) {
    TODO("AttributeExpr");
    return nullptr;
}

llvm::Value* Visitor::visit(ast::ElementExpr* expr) {
    llvm::Value* value = expr->value->accept(*this);
    // TODO: Check the type of the value

    ast::IntegerExpr* index = dynamic_cast<ast::IntegerExpr*>(expr->index.get());
    if (!index) {
        std::cerr << "Expected an integer" << std::endl;
        exit(1);
    }

    if (value->getType()->isPointerTy()) {
        llvm::Type* type = value->getType()->getPointerElementType();
        llvm::Value* idx = llvm::ConstantInt::get(this->context, llvm::APInt(32, index->value));

        auto ptr = this->builder->CreateGEP(type, value, idx);
        return this->builder->CreateLoad(type->getArrayElementType(), ptr);
    }

    return this->builder->CreateExtractValue(value, index->value);
}

llvm::Value* Visitor::visit(ast::IncludeExpr* expr) {
    std::string path = expr->path;
    if (!file_exists(path)) {
        if (!file_exists("std/" + path)) {
            std::cerr << "File " << path << " not found" << std::endl;
            exit(1);
        } else {
            path = "std/" + path;
        }
    }
    
    auto placeholder = llvm::ConstantInt::getNullValue(llvm::IntegerType::getInt1Ty(this->context));
    Module module = {path, ModuleState::Initialized};

    if (this->includes.find(path) != this->includes.end()) {
        module = this->includes[path];
        if (!module.is_ready()) {
            std::cerr << "Circular dependency detected" << std::endl;
            exit(1);
        }

        return placeholder;
    }

    this->includes[path] = module;

    std::ifstream file(path);
    Lexer lexer(file, path);

    auto program = Parser(lexer.lex()).statements();
    
    this->visit(std::move(program));
    module.state = ModuleState::Compiled;

    return placeholder;
}