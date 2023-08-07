#include <quart/objects/functions.h>
#include <quart/visitor.h>

#include <algorithm>

Function::Function(
    const std::string& name,
    std::vector<FunctionArgument> args,
    std::map<std::string, FunctionArgument> kwargs,
    Type return_type,
    llvm::Function* value,
    bool is_entry,
    bool is_intrinsic,
    bool is_anonymous,
    bool is_operator,
    Span span,
    ast::Attributes attrs
) : name(name), value(value), ret(return_type, nullptr, nullptr), args(args), kwargs(kwargs), attrs(attrs), 
    is_entry(is_entry), is_intrinsic(is_intrinsic), is_anonymous(is_anonymous), is_operator(is_operator) {

    this->used = false;
    this->is_finalized = false;
    this->current_block = nullptr;
    this->current_branch = nullptr;

    this->is_private = false;
    this->noreturn = attrs.has(Attribute::Noreturn);
    this->span = span;

    this->calls = {};
}

std::string Function::get_mangled_name() {
    return this->value->getName().str();
}

uint32_t Function::argc() {
    return this->args.size() + this->kwargs.size();
}

bool Function::is_c_variadic() {
    return this->value->isVarArg();
}

bool Function::is_variadic() {
    return std::any_of(this->args.begin(), this->args.end(), [](FunctionArgument& arg) {
        return arg.is_variadic;
    });
}

Branch* Function::create_branch(llvm::BasicBlock* loop, llvm::BasicBlock* end) {
    Branch* branch = new Branch();
    
    branch->loop = loop;
    branch->end = end;

    this->branches.push_back(branch);
    return branch;
}

bool Function::has_return() {
    return std::any_of(
        this->branches.begin(), this->branches.end(), [](auto& branch) {
            return branch->has_return;
        }
    );
}

bool Function::has_kwarg(const std::string& name) {
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

void Function::destruct(Visitor& visitor) {
    for (auto& destructor : this->destructors) {
        auto& method = destructor.structure->scope->functions["destructor"];
        visitor.call(method->value, {}, destructor.self);
    }
}