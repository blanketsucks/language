#include "objects/functions.h"

#include <algorithm>

Function::Function(
    std::string name,
    std::vector<FunctionArgument> args,
    std::map<std::string, FunctionArgument> kwargs,
    Type return_type,
    llvm::Function* value,
    bool is_entry,
    bool is_intrinsic,
    bool is_anonymous,
    bool is_operator,
    ast::Attributes attrs
) : name(name), value(value), ret(return_type, nullptr, nullptr), args(args), kwargs(kwargs), attrs(attrs), 
    is_entry(is_entry), is_intrinsic(is_intrinsic), is_anonymous(is_anonymous), is_operator(is_operator) {
    this->used = false;
    this->is_finalized = false;
    this->current_block = nullptr;

    this->is_private = attrs.has("private");
    this->noreturn = attrs.has("noreturn");
    this->attrs = attrs;

    this->calls = {};
}

std::string Function::get_mangled_name() {
    return this->value->getName().str();
}

uint32_t Function::argc() {
    return this->args.size() + this->kwargs.size();
}

bool Function::is_variadic() {
    return this->value->isVarArg();
}

Branch* Function::create_branch(std::string name, llvm::BasicBlock* loop, llvm::BasicBlock* end) {
    Branch* branch = new Branch(name);
    
    branch->loop = loop;
    branch->end = end;

    this->branches.push_back(branch);
    return branch;
}

bool Function::has_return() {
    return std::any_of(this->branches.begin(), this->branches.end(), [](Branch* branch) {
        return branch->has_return;
    });
}

bool Function::has_kwarg(std::string name) {
    return this->kwargs.find(name) != this->kwargs.end();
}

std::vector<FunctionArgument> Function::params() {
    auto params = this->args;
    for (auto& kwarg : this->kwargs) {
        params.push_back(kwarg.second);
    }

    std::sort(params.begin(), params.end(), [](FunctionArgument a, FunctionArgument b) {
        return a.index < b.index;
    });

    return params;
}

bool Function::has_any_default_value() {
    auto params = this->params();
    return std::any_of(params.begin(), params.end(), [](FunctionArgument arg) {
        return arg.default_value != nullptr;
    });
}

uint32_t Function::get_default_arguments_count() {
    auto params = this->params();
    return std::count_if(params.begin(), params.end(), [](FunctionArgument arg) {
        return arg.default_value != nullptr;
    });
}