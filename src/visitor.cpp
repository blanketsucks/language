#include "visitor.h"

#include "utils.hpp"
#include "llvm.h"

Visitor::Visitor(std::string module) {
    this->module = std::make_unique<llvm::Module>(llvm::StringRef(module), this->context);
    this->builder = std::make_unique<llvm::IRBuilder<>>(this->context);
}

void Visitor::dump(llvm::raw_ostream& stream) {
    this->module->print(stream, nullptr);
}

void Visitor::visit(std::unique_ptr<ast::Program> program) {
    for (auto& expr : program->ast) {
        expr->accept(*this);
    }
}

llvm::Value* Visitor::visit_IntegerExpr(ast::IntegerExpr* expr) {
    return llvm::ConstantInt::get(this->context, llvm::APInt(32, expr->value, true));
}

llvm::Value* Visitor::visit_VariableExpr(ast::VariableExpr* expr) {
    llvm::Value* value = this->variables[expr->name];
    if (!value) {
        std::cerr << "Variable " << expr->name << " not found" << std::endl;
        exit(1);
    }

    return value;
}

llvm::Value* Visitor::visit_ArrayExpr(ast::ArrayExpr* expr) {
    // TODO: Actually implement this
    llvm::ConstantArray::get(llvm::ArrayType::get(nullptr, 1), llvm::ArrayRef<llvm::Constant*>());
    return nullptr;
}

llvm::Value* Visitor::visit_BinaryOpExpr(ast::BinaryOpExpr* expr) {
    llvm::Value* left = expr->left->accept(*this);
    llvm::Value* right = expr->right->accept(*this);

    llvm::Type* ltype = left->getType();
    llvm::Type* rtype = right->getType();

    if (ltype != rtype) {
        if (Type::from_llvm_type(ltype).is_compatible(rtype)) {
            // TODO: Type cast
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

llvm::Value* Visitor::visit_CallExpr(ast::CallExpr* expr) {
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

llvm::Value* Visitor::visit_ReturnExpr(ast::ReturnExpr* expr) {
    if (expr->value) {
        llvm::Value* value = expr->value->accept(*this);
        return this->builder->CreateRet(value);
    } else {
        if (!this->current_function.return_type->isVoidTy()) {
            std::cerr << "Function " << this->current_function.name << " expects a return value" << std::endl;
            exit(1);
        }

        return this->builder->CreateRetVoid();
    }
}

llvm::Value* Visitor::visit_PrototypeExpr(ast::PrototypeExpr* prototype) {
    std::string name = prototype->name;

    llvm::Type* return_type = prototype->return_type.to_llvm_type(this->context);
    std::vector<llvm::Type*> args;

    for (auto& arg : prototype->args) {
        args.push_back(arg.type.to_llvm_type(this->context));
    }

    llvm::FunctionType* function_t = llvm::FunctionType::get(return_type, args, false);
    llvm::Function* function = llvm::Function::Create(function_t, llvm::Function::ExternalLinkage, name, this->module.get());

    unsigned i = 0;
    for (auto& arg : function->args()) {
        arg.setName(prototype->args[i++].name);
    }
    
    this->functions[name] = {name, args, return_type};
    return function;
}

llvm::Value* Visitor::visit_FunctionExpr(ast::FunctionExpr* expr) {
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

    this->variables.clear();
    for (auto& arg : function->args()) {
        std::string name = arg.getName().str();
        this->variables[name] = &arg;
    }

    this->current_function = this->functions[name];
    for (auto& expr : expr->body) {
        expr->accept(*this);
    }

    llvm::verifyFunction(*function);
    return function;
}