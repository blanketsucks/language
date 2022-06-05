#include "visitor.h"

#include "utils.hpp"
#include "llvm.h"

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

llvm::AllocaInst* Visitor::create_alloca(llvm::Function* function, llvm::Type* type, llvm::StringRef name) {
    llvm::IRBuilder<> tmp(&function->getEntryBlock(), function->getEntryBlock().begin());
    return tmp.CreateAlloca(type, nullptr, name);
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

llvm::Value* Visitor::cast(llvm::Value* value, Type type) {
    llvm::Type* target = type.to_llvm_type(this->context);
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

llvm::Value* Visitor::visit(ast::VariableExpr* expr) {
    if (this->current_function) {
        llvm::AllocaInst* variable = this->current_function->locals[expr->name];
        if (variable) {
            return this->builder->CreateLoad(variable->getAllocatedType(), variable, expr->name);;
        }
    }
    
    llvm::GlobalVariable* global = this->module->getGlobalVariable(expr->name);
    if (global) {
        return this->builder->CreateLoad(global->getValueType(), global, expr->name);
    }

    std::cout << "Variable " << expr->name << " is not defined" << std::endl;
    exit(1);
}

llvm::Value* Visitor::visit(ast::VariableAssignmentExpr* expr) {
    llvm::Value* value = expr->value->accept(*this);
    llvm::Type* type;

    if (expr->type.is_unknown()) {
        type = value->getType();
    } else {
        type = expr->type.to_llvm_type(this->context);
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

    llvm::AllocaInst* alloca_inst = this->create_alloca(func, type, expr->name);
    this->builder->CreateStore(value, alloca_inst);

    this->current_function->locals[expr->name] = alloca_inst;
    return value;
}

llvm::Value* Visitor::visit(ast::ArrayExpr* expr) {
    std::vector<llvm::Constant*> elements;
    for (auto& element : expr->elements) {
        auto constant = (llvm::Constant*)element->accept(*this);
        elements.push_back(constant);
    }

    if (elements.size() == 0) {
        return llvm::Constant::getNullValue(llvm::ArrayType::get(llvm::Type::getInt32Ty(this->context), 0));
    }

    // The type of an array is determined from the first element.
    auto type = elements[0]->getType();
    for (int i = 1; i < elements.size(); i++) {
        if (elements[i]->getType() != type) {
            std::cerr << "Array elements must be of the same type" << std::endl;
            exit(1);
        }
    }

    return llvm::ConstantArray::get(llvm::ArrayType::get(type, elements.size()), elements);
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
                return this->builder->CreateFNeg(value, "fneg");
            } else {
                return this->builder->CreateNeg(value, "neg");
            }
        case TokenType::NOT:
            return this->builder->CreateNot(this->cast(value, BooleanType), "not");
        case TokenType::BINARY_NOT:
            return this->builder->CreateNot(value, "not");
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
            std::cerr << "Left hand side of assignment must be a variable" << std::endl;
            exit(1);
        }

        llvm::Value* variable = this->get_variable(lhs->name);
        if (!variable) {
            std::cerr << "Variable " << lhs->name << " is not defined" << std::endl;
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
        if (Type::from_llvm_type(ltype).is_compatible(rtype)) {
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
                return this->builder->CreateFAdd(left, right, "fadd");
            } else {
                return this->builder->CreateAdd(left, right, "add");
            }
        case TokenType::MINUS:
            if (is_floating_point) {
                return this->builder->CreateFSub(left, right, "fsub");
            } else {
                return this->builder->CreateSub(left, right, "sub");
            }
        case TokenType::MUL:
            if (is_floating_point) {
                return this->builder->CreateFMul(left, right, "fmul");
            } else {
                return this->builder->CreateMul(left, right, "mul");
            }
        case TokenType::DIV:
            if (is_floating_point) {
                return this->builder->CreateFDiv(left, right, "fdiv");
            } else {
                return this->builder->CreateSDiv(left, right, "div");
            }
        case TokenType::EQ:
            if (is_floating_point) {
                return this->builder->CreateFCmpOEQ(left, right, "feq");
            } else {
                return this->builder->CreateICmpEQ(left, right, "eq");
            }
        case TokenType::NEQ:
            if (is_floating_point) {
                return this->builder->CreateFCmpONE(left, right, "fneq");
            } else {
                return this->builder->CreateICmpNE(left, right, "neq");
            }
        case TokenType::GT:
            if (is_floating_point) {
                return this->builder->CreateFCmpOGT(left, right, "fgt");
            } else {
                return this->builder->CreateICmpSGT(left, right, "gt");
            }
        case TokenType::LT:
            if (is_floating_point) {
                return this->builder->CreateFCmpOLT(left, right, "flt");
            } else {
                return this->builder->CreateICmpSLT(left, right, "lt");
            }
        case TokenType::GTE:
            if (is_floating_point) {
                return this->builder->CreateFCmpOGE(left, right, "fge");
            } else {
                return this->builder->CreateICmpSGE(left, right, "ge");
            }
        case TokenType::LTE:
            if (is_floating_point) {
                return this->builder->CreateFCmpOLE(left, right, "fle");
            } else {
                return this->builder->CreateICmpSLE(left, right, "le");
            }
        case TokenType::AND:
            return this->builder->CreateAnd(this->cast(left, BooleanType), this->cast(right, BooleanType), "and");
        case TokenType::OR:
            return this->builder->CreateOr(this->cast(left, BooleanType), this->cast(right, BooleanType), "or");
        case TokenType::BINARY_AND:
            return this->builder->CreateAnd(left, right, "and");
        case TokenType::BINARY_OR:
            return this->builder->CreateOr(left, right, "or");
        case TokenType::XOR:
            return this->builder->CreateXor(left, right, "xor");
        case TokenType::LSH:
            return this->builder->CreateShl(left, right, "lsh");
        case TokenType::RSH:
            return this->builder->CreateLShr(left, right, "rsh");
        default:
            std::cerr << "Unknown binary operator" << std::endl;
            exit(1);
    }
}

llvm::Value* Visitor::visit(ast::CallExpr* expr) {
    llvm::Function* function = this->module->getFunction(expr->callee);
    if (!function) {
        std::cerr << "Function " << expr->callee << " not found" << std::endl;
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

    return this->builder->CreateCall(function, args, "call");
}

llvm::Value* Visitor::visit(ast::ReturnExpr* expr) {
    if (!this->current_function) {
        std::cerr << "Return outside of function" << std::endl;
        exit(1);
    }

    Function* func = this->current_function;
    if (expr->value) {
        llvm::Value* value = expr->value->accept(*this);
        if (
            Type::from_llvm_type(func->ret).is_compatible(value->getType())
        ) {
            return this->builder->CreateRet(value);
        } else {
            std::cerr << "Incompatible return type" << std::endl;
            exit(1);
        }
    } else {
        if (!func->ret->isVoidTy()) {
            std::cerr << "Function " << func->name << " expects a return value" << std::endl;
            exit(1);
        }

        return this->builder->CreateRetVoid();
    }
}

llvm::Value* Visitor::visit(ast::PrototypeExpr* expr) {
    std::string name = expr->name;

    llvm::Type* ret = expr->return_type.to_llvm_type(this->context);
    std::vector<llvm::Type*> args;

    for (auto& arg : expr->args) {
        args.push_back(arg.type.to_llvm_type(this->context));
    }

    llvm::FunctionType* function_t = llvm::FunctionType::get(ret, args, false);
    llvm::Function* function = llvm::Function::Create(function_t, llvm::Function::ExternalLinkage, name, this->module.get());

    unsigned i = 0;
    for (auto& arg : function->args()) {
        arg.setName(expr->args[i++].name);
    }
    
    this->functions[name] = new Function(name, args, ret);
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

    llvm::BasicBlock* block = llvm::BasicBlock::Create(this->context, "entry", function);
    this->builder->SetInsertPoint(block);

    Function* func = this->functions[name];
    for (auto& arg : function->args()) {
        std::string name = arg.getName().str();

        llvm::AllocaInst* alloca_inst = this->create_alloca(function, arg.getType(), name);
        this->builder->CreateStore(&arg, alloca_inst);

        func->locals[name] = alloca_inst;
    }
    
    this->current_function = func;
    if (expr->body.size() == 0) {
        if (!func->ret->isVoidTy()) {
            std::cerr << "Function " << name << " expects a return value" << std::endl;
            exit(1);
        }

        this->builder->CreateRetVoid();
    } else {
        for (auto& expr : expr->body) {
            expr->accept(*this);
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

    for (auto& expr : expr->body) {
        expr->accept(*this);
    }

    this->builder->CreateBr(merge);
    then = this->builder->GetInsertBlock();

    function->getBasicBlockList().push_back(else_);
    this->builder->SetInsertPoint(else_);

    for (auto& expr : expr->ebody) {
        expr->accept(*this);
    }

    this->builder->CreateBr(merge);
    else_ = this->builder->GetInsertBlock();

    function->getBasicBlockList().push_back(merge);
    this->builder->SetInsertPoint(merge);

    llvm::PHINode* phi = this->builder->CreatePHI(BooleanType.to_llvm_type(this->context), 2, "if");

    phi->addIncoming(llvm::ConstantInt::getTrue(this->context), then);
    phi->addIncoming(llvm::ConstantInt::getFalse(this->context), else_);

    return phi;
}

llvm::Value* Visitor::visit(ast::StructExpr* expr) {
    std::vector<llvm::Type*> fields;
    for (auto& field : expr->fields) {
        fields.push_back(field.type.to_llvm_type(this->context));
    }

    auto type = llvm::StructType::create(this->context, fields, expr->name, expr->packed);
    return this->module->getOrInsertGlobal(expr->name, type);
}