#include <quart/objects/enums.h>
#include <quart/objects/scopes.h>

Enum::Enum(std::string name, llvm::Type* type) : name(name), type(type) {}

void Enum::add_field(std::string name, llvm::Constant* value) {
    this->scope->constants[name] = { name, value->getType(), nullptr, value, Span() };
}

bool Enum::has_field(std::string name) {
    return this->scope->constants.find(name) != this->scope->constants.end();
}

llvm::Value* Enum::get_field(std::string name) {
    return this->scope->constants[name].value;
}