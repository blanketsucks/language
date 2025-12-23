#include <quart/language/impl.h>
#include <quart/language/state.h>

namespace quart {

static bool match_impl_type(
    HashMap<String, Type*>& args, Type* impl, Type* target
) {
    switch (target->kind()) {
        case quart::TypeKind::Empty: {
            auto& name = target->get_empty_name();
            auto iterator = args.find(name);

            if (iterator != args.end()) {
                return iterator->second == impl;
            }

            args[name] = impl;
            return true;
        }
        case quart::TypeKind::Pointer: {
            if (!impl->is_pointer()) {
                return false;
            }

            return match_impl_type(
                args,
                impl->get_pointee_type(),
                target->get_pointee_type()
            );
        }
        case quart::TypeKind::Reference: {
            if (!impl->is_reference()) {
                return false;
            }

            return match_impl_type(
                args,
                impl->get_reference_type(),
                target->get_reference_type()
            );
        }
        case quart::TypeKind::Function: {
            if (!impl->is_function()) {
                return false;
            }

            auto& p1 = impl->get_function_params();
            auto& p2 = target->get_function_params();

            if (p1.size() != p2.size()) {
                return false;
            }

            for (size_t i = 0; i < p1.size(); ++i) {
                if (!match_impl_type(args, p1[i], p2[i])) {
                    return false;
                }
            }

            return match_impl_type(
                args,
                impl->get_function_return_type(),
                target->get_function_return_type()
            );
        }
        case quart::TypeKind::Array: {
            if (!impl->is_array()) {
                return false;
            }

            if (impl->get_array_size() != target->get_array_size()) {
                return false;
            }

            return match_impl_type(
                args,
                impl->get_array_element_type(),
                target->get_array_element_type()
            );
        }
        case quart::TypeKind::Tuple: {
            if (!impl->is_tuple()) {
                return false;
            }

            auto& p1 = impl->get_tuple_types();
            auto& p2 = target->get_tuple_types();

            if (p1.size() != p2.size()) {
                return false;
            }

            for (size_t i = 0; i < p1.size(); ++i) {
                if (!match_impl_type(args, p1[i], p2[i])) {
                    return false;
                }
            }

            return true;
        }
        default:
            return impl == target;
    }
}

ErrorOr<RefPtr<Scope>> Impl::make(State& state, Type* type) {
    if (!this->is_generic()) {
        return { nullptr };
    }

    auto iterator = m_impls.find(type);
    if (iterator != m_impls.end()) {
        return iterator->second;
    }

    HashMap<String, Type*> args;
    if (!match_impl_type(args, type, m_underlying_type)) {
        return { nullptr };
    }

    auto scope = Scope::create(format("<{}>", type->str()), ScopeType::Impl, m_scope->parent());
    for (auto& [name, ty] : args) {
        scope->add_symbol(TypeAlias::create(name, ty, false)); 
    }

    auto current_block = state.current_block();
    state.switch_to(nullptr);

    auto current_scope = state.scope();
    state.set_current_scope(scope);

    state.set_self_type(type);
    TRY(m_body->generate(state, {}));

    state.set_current_scope(current_scope);
    state.switch_to(current_block);
    state.set_self_type(nullptr);

    m_impls[type] = scope;
    return scope;
}

}