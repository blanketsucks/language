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

llvm::Type* Visitor::get_type(Type type) {
    if (type.name == "int") {
        return llvm::Type::getInt64Ty(this->context);
    } else if (type.name == "float") {
        return llvm::Type::getFloatTy(this->context);
    } else if (type.name == "double") {
        return llvm::Type::getDoubleTy(this->context);
    } else {
        return llvm::Type::getVoidTy(this->context);
    }
}

void Visitor::visit(std::unique_ptr<ast::Program> program) {
    for (auto& expr : program->ast) {
        expr->accept(*this);
    }
}

llvm::Value* Visitor::visit_IntegerExpr(ast::IntegerExpr* expr) {
    return llvm::ConstantFP::get(this->context, llvm::APFloat((double)expr->value));
}

llvm::Value* Visitor::visit_VariableExpr(ast::VariableExpr* expr) {
    llvm::Value* value = this->variables[expr->name];
    if (!value) {
        std::cerr << "Variable " << expr->name << " not found" << std::endl;
        exit(1);
    }

    return value;
}

llvm::Value* Visitor::visit_ListExpr(ast::ListExpr* expr) {
    (void)expr;
    return nullptr;
}

llvm::Value* Visitor::visit_BinaryOpExpr(ast::BinaryOpExpr* expr) {
    llvm::Value* left = expr->left->accept(*this);
    llvm::Value* right = expr->right->accept(*this);

    switch (expr->op) {
        case TokenType::PLUS:
            return this->builder->CreateFAdd(left, right, "addtmp");
        case TokenType::MINUS:
            return this->builder->CreateFSub(left, right, "subtmp");
        case TokenType::MUL:
            return this->builder->CreateFMul(left, right, "multmp");
        case TokenType::DIV:
            return this->builder->CreateFDiv(left, right, "divtmp");
        case TokenType::BINARY_AND:
            return this->builder->CreateAnd(left, right, "andtmp");
        case TokenType::BINARY_OR:
            return this->builder->CreateOr(left, right, "ortmp");
        case TokenType::XOR:
            return this->builder->CreateXor(left, right, "xortmp");
        case TokenType::LSH:
            return this->builder->CreateShl(left, right, "lshtmp");
        case TokenType::RSH:
            return this->builder->CreateLShr(left, right, "rshtmp");
        case TokenType::EQ:
            return this->builder->CreateFCmpOEQ(left, right, "eqtmp");
        case TokenType::NEQ:
            return this->builder->CreateFCmpUNE(left, right, "neqtmp");
        case TokenType::GT:
            return this->builder->CreateFCmpOGT(left, right, "gttmp");
        case TokenType::LT:
            return this->builder->CreateFCmpOLT(left, right, "lttmp");
        case TokenType::GTE:
            return this->builder->CreateFCmpOGE(left, right, "gte");
        case TokenType::LTE:
            return this->builder->CreateFCmpOLE(left, right, "lte");
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

    return this->builder->CreateCall(function, args, "calltmp");
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

    llvm::Type* return_type = this->get_type(prototype->return_type);
    std::vector<llvm::Type*> args;

    for (auto& arg : prototype->args) {
        args.push_back(this->get_type(arg.type));
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