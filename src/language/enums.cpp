#include <quart/language/enums.h>
#include <quart/language/scopes.h>

using namespace quart;

Enum::Enum(const std::string& name, quart::Type* type) : name(name), type(type), scope(nullptr) {}

void Enum::add_enumerator(const std::string& name, llvm::Constant* value, const Span& span) {
    this->enumerators[name] = { name, value, this->type };
    this->scope->constants[name] = { name, this->type, nullptr, value, span };

}

bool Enum::has_enumerator(const std::string& name) {
    return this->enumerators.find(name) != this->enumerators.end();
}

Enumerator* Enum::get_enumerator(const std::string& name) {
    auto iterator = this->enumerators.find(name);
    if (iterator == this->enumerators.end()) {
        return nullptr;
    }

    return &iterator->second;
}
