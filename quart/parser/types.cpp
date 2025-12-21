#include <quart/parser/ast.h>
#include <quart/language/state.h>
#include <quart/target.h>

#define MATCH_TYPE(Type, Func, ...) case ast::BuiltinType::Type: return context().Func(__VA_ARGS__);

namespace quart {

Type* State::get_type_from_builtin(ast::BuiltinType value) {
    size_t word_size = Target::build().word_size();
    switch (value) {
        MATCH_TYPE(Void, void_type);
        MATCH_TYPE(f32, f32);
        MATCH_TYPE(f64, f64);

        MATCH_TYPE(Bool, create_int_type, 1, true);
        MATCH_TYPE(i8, create_int_type, 8, true);
        MATCH_TYPE(i16, create_int_type, 16, true);
        MATCH_TYPE(i32, create_int_type, 32, true);
        MATCH_TYPE(i64, create_int_type, 64, true);
        MATCH_TYPE(i128, create_int_type, 128, true);

        MATCH_TYPE(u8, create_int_type, 8, false);
        MATCH_TYPE(u16, create_int_type, 16, false);
        MATCH_TYPE(u32, create_int_type, 32, false);
        MATCH_TYPE(u64, create_int_type, 64, false);
        MATCH_TYPE(u128, create_int_type, 128, false);

        MATCH_TYPE(usize, create_int_type, word_size, false);
        MATCH_TYPE(isize, create_int_type, word_size, true);

        default:
            return nullptr;
    }

    return nullptr;
}

}

namespace quart::ast {

ErrorOr<Type*> BuiltinTypeExpr::evaluate(State& state) const {
    return state.get_type_from_builtin(m_value);
}

ErrorOr<Type*> NamedTypeExpr::evaluate(State& state) const {
    auto scope = TRY(state.resolve_scope_path(span(), m_path));
    auto* symbol = scope->resolve(m_path.name());

    if (!symbol) {
        return err(span(), "Unknown identifier '{}'", m_path.format());
    }

    if (!symbol->is_public() && symbol->module() != state.module()) {
        return err(span(), "Cannot access private symbol '{}'", m_path.format());
    }

    switch (symbol->type()) {
        case Symbol::Struct: {
            auto* structure = symbol->as<Struct>();
            return structure->underlying_type();
        }
        case Symbol::Enum: {
            auto* enumeration = symbol->as<Enum>();
            return enumeration->underlying_type();
        }
        case Symbol::TypeAlias: {
            auto* alias = symbol->as<TypeAlias>();
            Type* underlying_type = alias->underlying_type();

            auto& last = m_path.last();
            if (!underlying_type && !alias->all_parameters_have_default()) {
                if (!last.has_generic_arguments()) {
                    return err(span(), "Type '{}' is generic and requires type arguments", m_path.format());
                }

                underlying_type = TRY(alias->evaluate(state, last.arguments()));
            } else if (!underlying_type) {
                if (last.has_generic_arguments()) {
                    underlying_type = TRY(alias->evaluate(state, last.arguments()));
                } else {
                    underlying_type = TRY(alias->evaluate(state));
                }
            }

            return underlying_type;
        }
        case Symbol::Trait: {
            auto* trait = symbol->as<Trait>();
            if (trait->has_generic_parameters()) {
                return err(span(), "Trait '{}' requires type arguments", trait->name());
            }

            return trait->underlying_type();
        }
        default: break;
    }

    return err(span(), "'{}' does not refer to a type", m_path.format());
}

ErrorOr<Type*> ArrayTypeExpr::evaluate(State& state) const {
    Constant* constant = TRY(state.constant_evaluator().evaluate(*m_size));
    if (!constant->is<ConstantInt>()) {
        return err(m_size->span(), "Array size must be an integer not {}", constant->type()->str());
    }

    u64 size = constant->as<ConstantInt>()->value();
    auto* element_type = TRY(m_type->evaluate(state));

    if (element_type->is_void()) {
        return err(m_type->span(), "Array elements cannot have void type");
    }

    return ArrayType::get(state.context(), element_type, size);
}

ErrorOr<Type*> FunctionTypeExpr::evaluate(State& state) const{
    Vector<Type*> parameters;
    parameters.reserve(m_parameters.size());

    for (auto& expr : m_parameters) {
        Type* type = TRY(expr->evaluate(state));
        if (type->is_void()) {
            return err(expr->span(), "Function parameters cannot have void type");
        }

        parameters.push_back(type);
    }

    Type* return_type = state.context().void_type();
    if (m_return_type) {
        return_type = TRY(m_return_type->evaluate(state));
    }

    auto* type = FunctionType::get(state.context(), return_type, parameters, false);
    return type->get_pointer_to();
}

ErrorOr<Type*> TupleTypeExpr::evaluate(State& state) const {
    Vector<Type*> elements;
    elements.reserve(m_types.size());

    for (auto& expr : m_types) {
        Type* type = TRY(expr->evaluate(state));
        if (type->is_void()) {
            return err(expr->span(), "Function parameters cannot have void type");
        }

        elements.push_back(type);
    }

    return TupleType::get(state.context(), elements);
}

ErrorOr<Type*> PointerTypeExpr::evaluate(State& state) const {
    Type* pointee = TRY(m_pointee->evaluate(state));
    return pointee->get_pointer_to(m_is_mutable);
}

ErrorOr<Type*> ReferenceTypeExpr::evaluate(State& state) const {
    Type* type = TRY(m_type->evaluate(state));
    if (type->is_void()) {
        return err(m_type->span(), "Cannot create a reference to void type");
    }

    return type->get_reference_to(m_is_mutable);
}

ErrorOr<Type*> IntegerTypeExpr::evaluate(State&) const {
    // FIXME: Implement
    return nullptr;
}

ErrorOr<Type*> GenericTypeExpr::evaluate(State& state) const {
    auto& path = m_parent->path();
    auto scope = TRY(state.resolve_scope_path(m_parent->span(), path));

    auto* symbol = scope->resolve(path.name());
    if (!symbol) {
        return err(m_parent->span(), "Unknown identifier '{}'", path.name());
    }

    Vector<Type*> args;
    for (auto& expr : m_args) {
        args.push_back(TRY(expr->evaluate(state)));
    }

    switch (symbol->type()) {
        case Symbol::TypeAlias: {
            auto* type_alias = symbol->as<TypeAlias>();
            if (!type_alias->is_generic()) {
                return err(span(), "Type '{}' is not generic", type_alias->name());
            }

            return type_alias->evaluate(state, args);
        }
        case Symbol::Trait: {
            auto* trait = symbol->as<Trait>();
            if (!trait->has_generic_parameters()) {
                return err(span(), "Trait '{}' is not generic", trait->name());
            } else if (args.size() != trait->generic_parameters().size()) {
                return err(
                    m_parent->span(),
                    "Trait '{}' expects {} generic arguments but {} were provided",
                    trait->name(),
                    trait->generic_parameters().size(),
                    args.size()
                );
            }

            auto [_, type] = TRY(trait->create_scope(state, args));
            return type;
        }
        default: break;
    }

    return err(m_parent->span(), "'{}' is not a generic type", path.format());
}

}