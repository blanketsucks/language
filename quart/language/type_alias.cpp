#include <quart/language/type_alias.h>
#include <quart/language/scopes.h>
#include <quart/language/state.h>

namespace quart {

String format_generic_type_name(String name, Vector<Type*> const& args) {
    name.push_back('<');
    size_t i = 0;

    for (auto& arg : args) {
        name.append(arg->str());
        if (i != args.size()) {
            name.append(", ");
        }

        i++;
    }

    name.push_back('>');
    return name;
}

bool TypeAlias::all_parameters_have_default() const {
    return llvm::all_of(m_parameters, [](const auto& param) { return param.is_optional(); });
}

ErrorOr<Type*> TypeAlias::evaluate(State& state) {
    auto range = llvm::map_range(m_parameters, [](const auto& param) { return param.default_type; });
    return this->evaluate(state, Vector<Type*>(range.begin(), range.end()));
}

ErrorOr<Type*> TypeAlias::evaluate(State& state, const ast::ExprList<ast::TypeExpr>& args) {
    Vector<Type*> arguments;
    arguments.reserve(args.size());

    for (auto& argument : args) {
        arguments.push_back(TRY(argument->evaluate(state)));
    }

    return this->evaluate(state, arguments);
}

ErrorOr<Type*> TypeAlias::evaluate(State& state, const Vector<Type*>& args) {
    auto iterator = m_cache.find(args);
    if (iterator != m_cache.end()) {
        return iterator->second;
    }

    auto scope = Scope::create({}, ScopeType::Anonymous, state.global_scope());
    auto previous_scope = state.scope();

    state.set_current_scope(scope);

    for (auto entry : llvm::zip(m_parameters, args)) {
        const GenericTypeParameter& parameter = std::get<0>(entry);
        quart::Type* type = std::get<1>(entry);

        // TODO: Apply constraints
        scope->add_symbol(TypeAlias::create(parameter.name, type, false));
    }

    Type* type = TRY(m_expr->evaluate(state));
    state.set_current_scope(previous_scope);

    m_cache[args] = type;
    return type;
}

}