#include "objects.h"
#include <algorithm>

Function::Function(
    std::string name,
    std::vector<FunctionArgument> args,
    std::map<std::string, FunctionArgument> kwargs,
    llvm::Type* ret,
    llvm::Function* value,
    bool is_entry,
    bool is_intrinsic,
    bool is_anonymous,
    ast::Attributes attrs
) : name(name), ret(ret), value(value), args(args), kwargs(kwargs), attrs(attrs), 
    is_entry(is_entry), is_intrinsic(is_intrinsic), is_anonymous(is_anonymous) {
    this->used = false;
    this->is_private = attrs.has("private");
    this->noreturn = attrs.has("noreturn");
    this->attrs = attrs;
    this->return_block = nullptr;
    this->calls = {};
    this->is_finalized = false;
}

Branch* Function::create_branch(std::string name, llvm::BasicBlock* loop, llvm::BasicBlock* end) {
    Branch* branch = new Branch(name);
    
    branch->loop = loop;
    branch->end = end;

    this->branches.push_back(branch);
    return branch;
}

bool Function::has_return() {
    for (Branch* branch : this->branches) {
        if (branch->has_return) {
            return true;
        }
    }

    return false;
}

void Function::defer(Visitor& visitor, bool is_noreturn) {
    for (auto& defer : this->defers) {
        if (defer.ignore_noreturn_calls && is_noreturn) {
            continue;
        }

        defer.expr->accept(visitor);
    }
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