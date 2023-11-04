#include <quart/language/typealias.h>
#include <quart/language/scopes.h>
#include <quart/visitor.h>

bool quart::TypeAlias::is_instantiable_without_args() const {
    return llvm::all_of(this->parameters, [](const auto& p) { return p.default_type != nullptr; });
}

quart::Type* quart::TypeAlias::instantiate(Visitor& visitor) {
    if (!this->is_instantiable_without_args()) {
        return nullptr;
    }

    std::vector<quart::Type*> args;
    for (const auto& param : this->parameters) {
        args.push_back(param.default_type);
    }

    return this->instantiate(visitor, args);
}

quart::Type* quart::TypeAlias::instantiate(Visitor& visitor, const std::vector<quart::Type*>& args) {
    auto iterator = this->cache.find(args);
    if (iterator != this->cache.end()) {
        return iterator->second;
    }

    Scope* scope = new Scope("generic", ScopeType::Anonymous);
    scope->parent = visitor.scope;

    for (auto entry : llvm::zip(this->parameters, args)) {
        const GenericTypeParameter& paremeter = std::get<0>(entry);
        quart::Type* type = std::get<1>(entry);

        // TODO: Apply constraints

        scope->type_aliases[paremeter.name] = quart::TypeAlias(paremeter.name, type, paremeter.span);
    }

    visitor.scope = scope;
    quart::Type* ty = this->expr->accept(visitor);

    visitor.scope = scope->parent;
    delete scope;

    this->cache[args] = ty;
    return ty;
}

