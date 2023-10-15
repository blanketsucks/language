#include <quart/language/functions.h>

using namespace quart;

Function::Function(
    llvm::Function* value,
    quart::Type* type,
    const std::string& name,
    std::vector<Parameter> params,
    std::map<std::string, Parameter> kwargs,
    quart::Type* return_type,
    u16 flags,
    const Span& span,
    const ast::Attributes& attrs
) : value(value), type(type), name(name), params(std::move(params)), kwargs(std::move(kwargs)), flags(flags), span(span), attrs(attrs) {
    this->ret.type = return_type;
    this->parent = nullptr;

    if (attrs.has(Attribute::Noreturn)) {
        this->flags |= Function::NoReturn;
    }
}

FunctionRef Function::create(
    llvm::Function* value,
    quart::Type* type,
    const std::string& name,
    std::vector<Parameter> params,
    std::map<std::string, Parameter> kwargs,
    quart::Type* return_type,
    u16 flags,
    const Span& span,
    const ast::Attributes& attrs
) {
    return std::shared_ptr<Function>(new Function(value, type, name, params, kwargs, return_type, flags, span, attrs));
}

u32 Function::argc() {
    return this->params.size() + this->kwargs.size();
}

bool Function::is_c_variadic() {
    return this->value->isVarArg();
}

bool Function::is_variadic() {
    return false;
}

bool Function::has_any_default_value() {
    return llvm::any_of(this->params, [](Parameter& param) { return param.has_default_value(); });
}

u32 Function::get_default_arguments_count() {
    return llvm::count_if(this->params, [](Parameter& param) { return param.has_default_value(); });
}

bool Function::has_keyword_parameter(const std::string& name) {
    return this->kwargs.find(name) != this->kwargs.end();
}