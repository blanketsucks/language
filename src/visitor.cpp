#include "visitor.h"

#include "utils.hpp"
#include "llvm.h"

Visitor::Visitor(std::string module) {
    this->module = std::make_unique<llvm::Module>(llvm::StringRef(module), this->context);
    this->builder = std::make_unique<llvm::IRBuilder<>>(this->context);

    this->constants = {
        {"true", llvm::ConstantInt::getTrue(this->context)},
        {"false", llvm::ConstantInt::getFalse(this->context)},
    };
}

void Visitor::dump(llvm::raw_ostream& stream) {
    this->module->print(stream, nullptr);
}

llvm::AllocaInst* Visitor::create_alloca(llvm::Function* function, llvm::Type* type, std::string name) {
    llvm::IRBuilder<> tmp(&function->getEntryBlock(), function->getEntryBlock().begin());
    return tmp.CreateAlloca(type, 0, name);
}

llvm::Value* Visitor::get_variable(std::string name) {
    llvm::Value* value;
    if (this->current_function) {
        value = this->current_function->locals[name];
    } else {
        value = this->globals[name];
    }

    if (!value) {
        value = this->constants[name];
        if (!value) {
            std::cerr << "Variable " << name << " not found" << std::endl;
            exit(1);
        }
    }

    return value;
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
    llvm::Value* value = this->get_variable(expr->name);
    return this->builder->CreateLoad(value->getType(), value, expr->name);
}

llvm::Value* visit_VariableAssignmentExpr(ast::VariableAssignmentExpr* expr) {
    (void)expr;
    return nullptr;
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

llvm::Value* Visitor::visit(ast::BinaryOpExpr* expr) {
    // Assingment is a special case.
    if (expr->op == TokenType::ASSIGN) {
        ast::VariableAssignmentExpr* lhs = dynamic_cast<ast::VariableAssignmentExpr*>(expr->left.get());
        if (!lhs) {
            std::cerr << "Left hand side of assignment must be a variable" << std::endl;
            exit(1);
        }

        llvm::Value* variable = this->get_variable(lhs->name);
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

    switch (expr->op) {
        case TokenType::PLUS:
            if (ltype->isFloatingPointTy()) {
                return this->builder->CreateFAdd(left, right, "fadd");
            } else {
                return this->builder->CreateAdd(left, right, "add");
            }
        case TokenType::MINUS:
            if (ltype->isFloatingPointTy()) {
                return this->builder->CreateFSub(left, right, "fsub");
            } else {
                return this->builder->CreateSub(left, right, "sub");
            }
        case TokenType::MUL:
            if (ltype->isFloatingPointTy()) {
                return this->builder->CreateFMul(left, right, "fmul");
            } else {
                return this->builder->CreateMul(left, right, "mul");
            }
        case TokenType::DIV:
            if (ltype->isFloatingPointTy()) {
                return this->builder->CreateFDiv(left, right, "fdiv");
            } else {
                return this->builder->CreateSDiv(left, right, "div");
            }
        case TokenType::EQ:
            if (ltype->isFloatingPointTy()) {
                return this->builder->CreateFCmpOEQ(left, right, "feq");
            } else {
                return this->builder->CreateICmpEQ(left, right, "eq");
            }
        case TokenType::NEQ:
            if (ltype->isFloatingPointTy()) {
                return this->builder->CreateFCmpONE(left, right, "fneq");
            } else {
                return this->builder->CreateICmpNE(left, right, "neq");
            }
        case TokenType::GT:
            if (ltype->isFloatingPointTy()) {
                return this->builder->CreateFCmpOGT(left, right, "fgt");
            } else {
                return this->builder->CreateICmpSGT(left, right, "gt");
            }
        case TokenType::LT:
            if (ltype->isFloatingPointTy()) {
                return this->builder->CreateFCmpOLT(left, right, "flt");
            } else {
                return this->builder->CreateICmpSLT(left, right, "lt");
            }
        case TokenType::GTE:
            if (ltype->isFloatingPointTy()) {
                return this->builder->CreateFCmpOGE(left, right, "fge");
            } else {
                return this->builder->CreateICmpSGE(left, right, "ge");
            }
        case TokenType::LTE:
            if (ltype->isFloatingPointTy()) {
                return this->builder->CreateFCmpOLE(left, right, "fle");
            } else {
                return this->builder->CreateICmpSLE(left, right, "le");
            }
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
    func->locals = std::map<std::string, llvm::AllocaInst*>(this->globals);

    for (auto& arg : function->args()) {
        std::string name = arg.getName().str();

        llvm::AllocaInst* alloca_inst = this->create_alloca(function, arg.getType(), name);
        this->builder->CreateStore(&arg, alloca_inst);

        func->locals[name] = alloca_inst;
    }
    
    this->current_function = func;
    for (auto& expr : expr->body) {
        expr->accept(*this);
    }
    
    this->current_function = nullptr;
    llvm::verifyFunction(*function);

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

    if (expr->ebody.size() > 0) {
        for (auto& expr : expr->ebody) {
            expr->accept(*this);
        }
    }

    this->builder->CreateBr(merge);
    else_ = this->builder->GetInsertBlock();

    function->getBasicBlockList().push_back(merge);
    this->builder->SetInsertPoint(merge);

    llvm::PHINode* phi = this->builder->CreatePHI(llvm::Type::getInt1Ty(this->context), 2, "if");

    phi->addIncoming(llvm::ConstantInt::getTrue(this->context), then);
    phi->addIncoming(llvm::ConstantInt::getFalse(this->context), else_);

    return phi;
}