#include <quart/language/trait.h>
#include <quart/language/scopes.h>
#include <quart/language/state.h>

namespace quart {

Function const* Trait::get_method(const String& name) const {
    return m_scope->resolve<quart::Function>(name);
}

ErrorOr<Trait::GenericTraitScope> Trait::create_scope(State& state, const Vector<Type*>& types) {
    String name = format("{}<{}>", this->name(), format_range(types, [](auto* type) { return type->str(); }));
    auto* type = TraitType::get(state.context(), name);

    auto iterator = m_scopes.find(type);
    if (iterator != m_scopes.end()) {
        return GenericTraitScope { iterator->second.scope, type };
    }

    auto scope = m_scope->clone(name);
    size_t index = 0;

    for (auto& [name, _] : m_generic_parameters) {
        scope->add_symbol(TypeAlias::create(name, types[index], false));
        index++;
    }

    auto current_scope = state.scope();

    state.set_current_scope(scope);
    state.set_self_type(type);

    m_scopes[type] = { scope, types };
    state.add_trait(type, state.get_trait(this->underlying_type()));

    for (auto& expr : m_body) {
        TRY(state.type_checker().type_check(*expr));
    }

    state.set_current_scope(current_scope);
    state.set_self_type(nullptr);

    return GenericTraitScope { scope, type };
}

}