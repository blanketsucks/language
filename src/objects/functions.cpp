#include "objects.h"

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
    this->attrs = attrs;
    
    this->calls = {};
    this->defers = {};
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

void Function::defer(Visitor& visitor) {
    for (auto& expr : this->defers) {
        expr->accept(visitor);
    }
}

bool Function::has_kwarg(std::string name) {
    return this->kwargs.find(name) != this->kwargs.end();
}

std::vector<FunctionArgument> Function::get_all_args() {
    std::vector<FunctionArgument> all_args = this->args;
    for (auto& kwarg : this->kwargs) {
        all_args.push_back(kwarg.second);
    }

    return all_args;
}