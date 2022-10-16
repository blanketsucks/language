#include "visitor.h"

#include <stdint.h>
#include "llvm/IR/InstIterator.h"

llvm::Function* Visitor::create_function(
    std::string name, 
    llvm::Type* ret, 
    std::vector<llvm::Type*> args, 
    bool is_variadic, 
    llvm::Function::LinkageTypes linkage
) {
    llvm::FunctionType* type = llvm::FunctionType::get(ret, args, is_variadic);
    return llvm::Function::Create(type, linkage, name, this->module.get());
}

std::vector<llvm::Value*> Visitor::typecheck_function_call(
    llvm::Function* function, std::vector<llvm::Value*>& args, uint32_t start, Location location
) {
    std::string fn = function->getName().str();
    
    for (auto& value : args) {
        if ((function->isVarArg() && start == 0) || !function->isVarArg()) {
            llvm::Argument* argument = function->getArg(start);
            if (!this->is_compatible(value->getType(), argument->getType())) {
                std::string name = argument->getName().str();
                if (fn.empty()) {
                    ERROR(
                        location, 
                        "Argument of index {0} of type '{1}' does not match expected type '{2}'",
                        start, this->get_type_name(value->getType()), this->get_type_name(argument->getType())
                    );
                }

                if (name.empty()) {
                    ERROR(
                        location, 
                        "Argument of index {0} of type '{1}' does not match expected type '{2}' in function '{3}'.", 
                        start, this->get_type_name(value->getType()), this->get_type_name(argument->getType()), fn
                    );
                }

                ERROR(
                    location, 
                    "Argument '{0}' of type '{1}' does not match expected type '{2}' in function '{3}'.", 
                    name, this->get_type_name(value->getType()), this->get_type_name(argument->getType()), fn
                );
            } else {
                args[start] = this->cast(value, argument->getType());
            }
        }

        start++;
    }

    return args;
}

llvm::Value* Visitor::call(
    llvm::Function* function, 
    std::vector<llvm::Value*> args, 
    bool is_constructor,
    llvm::Value* self, 
    llvm::FunctionType* type,
    Location location
) {
    if (!type) {
        type = function->getFunctionType();
    }

    uint32_t i = self ? 1 : 0;
    std::string fn = function->getName().str();

    if (self) {
        args.emplace(args.begin(), self);
    }

    for (auto& value : args) {
        if ((function->isVarArg() && i == 0) || !function->isVarArg()) {
            llvm::Argument* argument = function->getArg(i);
            if (!this->is_compatible(value->getType(), argument->getType())) {
                std::string name = argument->getName().str();
                if (fn.empty()) {
                    ERROR(
                        location, 
                        "Argument of index {0} of type '{1}' does not match expected type '{2}'",
                        i, this->get_type_name(value->getType()), this->get_type_name(argument->getType())
                    );
                }

                if (name.empty()) {
                    ERROR(
                        location, 
                        "Argument of index {0} of type '{1}' does not match expected type '{2}' in function '{3}'.", 
                        i, this->get_type_name(value->getType()), this->get_type_name(argument->getType()), fn
                    );
                }

                ERROR(
                    location, 
                    "Argument '{0}' of type '{1}' does not match expected type '{2}' in function '{3}'.", 
                    name, this->get_type_name(value->getType()), this->get_type_name(argument->getType()), fn
                );
            } else {
                args[i] = this->cast(value, argument->getType());
            }
        }

        i++;
    }


    llvm::Value* ret = this->builder->CreateCall({type, function}, args);
    if (is_constructor) {
        return self;
    }

    return ret;
}

Value Visitor::visit(ast::PrototypeExpr* expr) {
    if (!this->current_struct && expr->attributes.has("private")) {
        utils::error(expr->start, "Cannot declare private function outside of a struct");
    }

    std::string name;
    bool is_anonymous = false;

    if (expr->name.empty()) {
        name = FORMAT("__anon.{0}", this->id++);
        is_anonymous = true;
    } else {
        name = this->format_name(expr->name);
    }

    auto pair = this->format_intrinsic_function(name);
    switch (expr->linkage) {
        case ast::ExternLinkageSpecifier::C:
            pair.first = expr->name;
        default: break;
    }

    llvm::Type* ret = this->get_llvm_type(expr->return_type);
    if (name == this->entry) {
        if (!ret->isVoidTy() && !ret->isIntegerTy()) {
            utils::error(expr->start, "Entry point function must either return void or an integer");
        }

        if (ret->isVoidTy()) {
            ret = this->builder->getInt32Ty();
        }
    }

    std::vector<llvm::Type*> args;
    for (auto& arg : expr->args) {
        args.push_back(this->get_llvm_type(arg.type));
    }

    llvm::Function::LinkageTypes linkage = llvm::Function::LinkageTypes::ExternalLinkage;
    if (expr->attributes.has("internal")) {
        linkage = llvm::Function::LinkageTypes::InternalLinkage;
    }

    llvm::Function* function = this->create_function(
        pair.first, ret, args, expr->is_variadic, linkage
    );

    int i = 0;
    for (auto& arg : function->args()) {
        arg.setName(expr->args[i++].name);
    }

    Function* func = new Function(
        pair.first, 
        args, 
        ret, 
        function,
        (name == this->entry), 
        pair.second,
        is_anonymous,
        expr->attributes
    );
    
    func->start = expr->start;
    func->end = expr->end;

    this->scope->functions[expr->name] = func; 
    return function;
}

Value Visitor::visit(ast::FunctionExpr* expr) {
    std::string name = this->format_name(expr->prototype->name);
    llvm::Function* function = this->module->getFunction(name);
    if (!function) {
        function = llvm::cast<llvm::Function>(expr->prototype->accept(*this).value);
    }

    Function* func = this->scope->functions[expr->prototype->name];
    if (!function->empty()) {
        NOTE(func->start, "Function '{0}' was previously defined here", name);
        ERROR(expr->start, "Function '{0}' is already defined", name);
    }

    if (func->is_intrinsic) {
        ERROR(expr->start, "Cannot define intrinsic function '{0}'", name);
    }

    func->end = expr->end;

    llvm::BasicBlock* block = llvm::BasicBlock::Create(*this->context, "", function);
    this->set_insert_point(block, false);

    Branch* branch = func->create_branch(name);
    func->branch = branch;

    if (!func->ret->isVoidTy()) {
        func->return_value = this->builder->CreateAlloca(func->ret);
    }

    func->return_block = llvm::BasicBlock::Create(*this->context);

    func->scope = new Scope(func->name, ScopeType::Function, this->scope);
    this->scope = func->scope;

    uint32_t i = 0;
    for (auto& arg : function->args()) {
        std::string name = arg.getName().str();
        llvm::Type* type = func->args[i];

        llvm::AllocaInst* inst = this->create_alloca(type);
        if (type->isStructTy() && arg.getType()->isIntegerTy(64)) {
            llvm::Value* cast = this->builder->CreateBitCast(inst, this->builder->getInt32Ty()->getPointerTo());
            this->builder->CreateStore(&arg, cast);
        } else {
            this->builder->CreateStore(&arg, inst);
        }

        func->scope->variables[name] = inst;
        i++;
    }
    

    Function* outer = this->current_function;
    this->current_function = func;

    if (!expr->body) {
        if (!func->ret->isVoidTy()) {
            ERROR(expr->start, "Function '{0}' expects a return value", name);
        }

        this->builder->CreateRetVoid();
    } else {
        expr->body->accept(*this);

        if (!func->has_return()) {
            if (func->ret->isVoidTy() || func->is_entry) {
                func->defer(*this);
                if (func->is_entry) {
                    this->builder->CreateRet(this->builder->getInt32(0));
                } else {
                    this->builder->CreateRetVoid();
                }
            } else {
                ERROR(expr->start, "Function '{0}' expects a return value", name);
            }
        } else {
            this->set_insert_point(func->return_block);

            if (func->ret->isVoidTy()) {
                this->builder->CreateRetVoid();
            } else {
                llvm::Value* value = this->builder->CreateLoad(function->getReturnType(), func->return_value);
                this->builder->CreateRet(value);
            }
        }
    }

    for (auto defer : func->defers) {
        delete defer;
    }
    
    bool error = llvm::verifyFunction(*function, &llvm::errs());
    assert((!error) && "Error while verifying function IR. Most likely a compiler bug.");

    if (this->with_optimizations) {
        this->fpm->run(*function);
    }

    this->scope->exit(this);

    this->current_function = outer;
    if (outer) {
        this->builder->SetInsertPoint(outer->current_block);
    }

    return Value(function, true);
}

Value Visitor::visit(ast::ReturnExpr* expr) {
    Function* func = this->current_function;
    if (expr->value) {
        if (func->ret->isVoidTy()) {
            utils::error(expr->start, "Cannot return a value from void function");
        }

        this->ctx = func->ret;
        llvm::Value* value = expr->value->accept(*this).unwrap(expr->start);

        if (!this->is_compatible(value->getType(), func->ret)) {
            func->ret->dump();

            ERROR(
                expr->start, "Cannot return value of type '{0}' from function expecting '{1}'", 
                this->get_type_name(value->getType()), this->get_type_name(func->ret)
            );
        } else {
            value = this->cast(value, func->ret);
        }

        func->branch->has_return = true;

        this->builder->CreateStore(value, func->return_value);
        func->defer(*this);

        this->builder->CreateBr(func->return_block);
        this->ctx = nullptr;

        return nullptr;
    } else {
        if (!func->ret->isVoidTy()) {
            ERROR(expr->start, "Function '{0}' expects a return value", func->name);
        }

        func->branch->has_return = true;
        func->defer(*this);

        this->builder->CreateBr(func->return_block);
        return nullptr;
    }
}

Value Visitor::visit(ast::DeferExpr* expr) {
    Function* func = this->current_function;
    if (!func) {
        utils::error(expr->start, "Defer statement outside of function");
    }

    func->defers.push_back(expr->expr.release());
    return nullptr;
}

Value Visitor::visit(ast::CallExpr* expr) {
    Value callable = expr->callee->accept(*this);
    Function* func = callable.function;

    size_t argc = expr->args.size();
    bool is_constructor = false;

    llvm::Type* type = nullptr;
    if (callable.structure) {
        Struct* structure = callable.structure;
        
        llvm::AllocaInst* instance = this->builder->CreateAlloca(structure->type);
        callable.parent = instance; // `self` parameter for the constructor
    
        func = structure->methods["constructor"];
        func->used = true;

        callable.value = this->module->getFunction(func->name);
        is_constructor = true;

        type = func->value->getFunctionType();
    } else {
        type = callable.type();
        if (type->isPointerTy()) {
            type = type->getNonOpaquePointerElementType();
            if (!type->isFunctionTy()) {
                utils::error(expr->start, "Cannot call non-function");
            }
        } else {
            if (!callable.function) {
                utils::error(expr->start, "Cannot call non-function");
            }
        }
    }

    if (callable.parent) {
        argc++;
    }

    llvm::Function* function = (llvm::Function*)callable.value;

    std::string name = function->getName().str();
    if (function->getName() == this->entry) {
        utils::error(expr->start, "Cannot call the main entry function");
    }

    llvm::FunctionType* ftype = nullptr;
    if (type->isFunctionTy()) {
        ftype = llvm::cast<llvm::FunctionType>(type);
    } else {
        type = type->getNonOpaquePointerElementType();
        if (type->isFunctionTy()) {
            ftype = llvm::cast<llvm::FunctionType>(type);
        } else {
            utils::error(expr->start, "Cannot call non-function");
        }
    }

    if (ftype->getNumParams() != argc) {
        if (ftype->isVarArg()) {
            if (argc < 1) {
                if (name.empty()) {
                    ERROR(expr->start, "Function call expects at least one argument", name);
                }

                ERROR(expr->start, "Function '{0}' expects at least one argument", name);
            }
        } else {
            if (name.empty()) {
                ERROR(
                    expr->start, 
                    "Function call expects {0} arguments but got {1}", 
                    function->arg_size(), argc
                );
            }

            ERROR(
                expr->start, 
                "Function '{s}' expects {0} arguments but got {1}", 
                name, function->arg_size(), argc
            );
        }
    }

    std::vector<llvm::Value*> args;
    uint32_t i = 0;

    for (auto& arg : expr->args) {
        // Only case where this can be false is with variadic functions
        if (i < argc) {
            this->ctx = ftype->getParamType(i);
        }

        args.push_back(arg->accept(*this).unwrap(arg->start));
        this->ctx = nullptr;

        i++;
    }

    return this->call(
        function, args, is_constructor, callable.parent, ftype, expr->start
    );
}