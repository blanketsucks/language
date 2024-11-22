#include <quart/language/impl.h>
#include <quart/language/state.h>

namespace quart {

ImplCondition::Result ImplCondition::is_satisfied(quart::Type* ty) {
    switch (type) {
        case ImplCondition::Pointer:
            if (ty->is_pointer()) {
                return { true, ty->get_pointee_type(), name };
            }

            break;
        case ImplCondition::Reference:
            if (ty->is_reference()) {
                return { true, ty->get_reference_type(), name };
            }

            break;
        case ImplCondition::FunctionParameter: {
            if (ty->is_function()) {
                auto* param = ty->get_function_param(index);
                if (!inner) {
                    return { true, param, name };
                }

                return inner->is_satisfied(param);
            }

            break;
        }
        case ImplCondition::FunctionReturn: {
            if (ty->is_function()) {
                auto* ret = ty->get_function_return_type();
                if (!inner) {
                    return { true, ret, name };
                }

                return inner->is_satisfied(ret);
            }

            break;
        }
    }

    return { false, nullptr, name };
}

ErrorOr<Scope*> Impl::make(State& state, Type* type) {
    if (!this->is_generic()) {
        return nullptr;
    }

    auto iterator = m_impls.find(type);
    if (iterator != m_impls.end()) {
        return iterator->second;
    }

    HashMap<String, Type*> args;
    for (auto& condition : m_conditions) {
        auto result = condition->is_satisfied(type);
        if (!result.satisfied) {
            return nullptr;
        }

        auto iterator = args.find(result.name);
        if (iterator != args.end()) {
            if (iterator->second != result.type) {
                return nullptr;
            }
        }

        args[result.name] = result.type;
    }

    Scope* scope = Scope::create(type->str(), ScopeType::Impl, m_parent_scope);
    for (auto& [name, ty] : args) {
        scope->add_symbol(TypeAlias::create(name, ty)); 
    }

    Scope* current_scope = state.scope();
    state.set_current_scope(scope);

    state.set_self_type(type);
    TRY(m_body->generate(state, {}));

    state.set_current_scope(current_scope);
    state.set_self_type(nullptr);

    m_impls[type] = scope;
    return scope;
}

}