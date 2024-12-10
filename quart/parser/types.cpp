#include <quart/parser/ast.h>
#include <quart/language/state.h>
#include <quart/target.h>

#define MATCH_TYPE(Type, Func, ...) case ast::BuiltinType::Type: return types().Func(__VA_ARGS__);

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

ErrorOr<Type*> BuiltinTypeExpr::evaluate(State& state) {
    return state.get_type_from_builtin(m_value);
}

ErrorOr<Type*> NamedTypeExpr::evaluate(State& state) {
    Scope* scope = TRY(state.resolve_scope_path(span(), m_path));
    auto* symbol = scope->resolve(m_path.last.name);

    if (!symbol) {
        return err(span(), "Undefined identifier '{0}'", m_path.last.name);
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

            if (!underlying_type && !alias->all_parameters_have_default()) {
                return err(span(), "Type '{0}' is generic and requires type arguments", m_path.last.name);
            } else if (!underlying_type) {
                underlying_type = TRY(alias->evaluate(state));
            }

            return underlying_type;
        }
        default: break;
    }

    return err(span(), "'{0}' does not name a type", m_path.last.name);
}

ErrorOr<Type*> ArrayTypeExpr::evaluate(State& state) {
    auto option = TRY(m_size->generate(state));
    if (!option.has_value()) {
        return err(m_size->span(), "Expected an expression");
    }

    bytecode::Operand operand = option.value();
    if (!operand.is_immediate()) {
        return err(m_size->span(), "Array size must be a constant");
    }

    if (!operand.value_type()->is_int()) {
        return err(m_size->span(), "Array size must be an integer");
    }

    auto* element_type = TRY(m_type->evaluate(state));
    if (element_type->is_void()) {
        return err(m_type->span(), "Array elements cannot have void type");
    }

    return state.types().create_array_type(element_type, operand.value());
}

ErrorOr<Type*> FunctionTypeExpr::evaluate(State& state) {
    Vector<Type*> parameters;
    parameters.reserve(m_parameters.size());

    for (auto& expr : m_parameters) {
        Type* type = TRY(expr->evaluate(state));
        if (type->is_void()) {
            return err(expr->span(), "Function parameters cannot have void type");
        }

        parameters.push_back(type);
    }

    Type* return_type = TRY(m_return_type->evaluate(state));
    return state.types().create_function_type(return_type, parameters)->get_pointer_to();
}

ErrorOr<Type*> TupleTypeExpr::evaluate(State& state) {
    Vector<Type*> elements;
    elements.reserve(m_types.size());

    for (auto& expr : m_types) {
        Type* type = TRY(expr->evaluate(state));
        if (type->is_void()) {
            return err(expr->span(), "Function parameters cannot have void type");
        }

        elements.push_back(type);
    }

    return state.types().create_tuple_type(elements);
}

ErrorOr<Type*> PointerTypeExpr::evaluate(State& state) {
    Type* pointee = TRY(m_pointee->evaluate(state));
    return state.types().create_pointer_type(pointee, m_is_mutable);
}

ErrorOr<Type*> ReferenceTypeExpr::evaluate(State& state) {
    Type* type = TRY(m_type->evaluate(state));
    if (type->is_void()) {
        return err(m_type->span(), "Cannot create a reference to void type");
    }

    return state.types().create_reference_type(type, m_is_mutable);
}

ErrorOr<Type*> IntegerTypeExpr::evaluate(State&) {
    // FIXME: Implement
    return nullptr;
}

ErrorOr<Type*> GenericTypeExpr::evaluate(State& state) {
    auto& path = m_parent->path();
    auto* scope = TRY(state.resolve_scope_path(m_parent->span(), path));

    auto* symbol = scope->resolve(path.last.name);
    if (!symbol) {
        return err(m_parent->span(), "Unknown identifier '{0}'", path.last.name);
    }

    Vector<Type*> args;
    for (auto& expr : m_args) {
        args.push_back(TRY(expr->evaluate(state)));
    }

    switch (symbol->type()) {
        case Symbol::TypeAlias: {
            auto* type_alias = symbol->as<TypeAlias>();
            if (!type_alias->is_generic()) {
                return err(span(), "Type '{0}' is not generic", type_alias->name());
            }

            return type_alias->evaluate(state, args);
        }
    }

    return nullptr;
}

}