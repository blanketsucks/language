#include <quart/language/type_checker.h>
#include <quart/language/state.h>

namespace quart {

ErrorOr<Type*> TypeChecker::resolve_reference(Scope& scope, Span span, const String& name, bool is_mutable) {
    auto* symbol = scope.resolve(name);
    if (!symbol) {
        return err(span, "Unknown identifier '{}'", name);
    }

    switch (symbol->type()) {
        case Symbol::Variable: {
            auto* variable = symbol->as<Variable>();
            if (is_mutable && !variable->is_mutable()) {
                return err(span, "Cannot take a mutable reference to an immutable variable");
            }

            return variable->value_type()->get_reference_to(is_mutable);
        }
        default:
            return err(span, "Invalid reference");
    }

    return {};
}

ErrorOr<Type*> TypeChecker::resolve_reference(ast::Expr const& expr, bool is_mutable) {
    using ast::ExprKind;
    
    switch (expr.kind()) {
        case ExprKind::Identifier: {
            auto* ident = expr.as<ast::IdentifierExpr>();
            return this->resolve_reference(*m_state.scope(), ident->span(), ident->name(), is_mutable);
        }
        case ExprKind::Path: {
            auto& path = expr.as<ast::PathExpr>()->path();
            auto scope = TRY(this->m_state.resolve_scope_path(expr.span(), path));

            return this->resolve_reference(*scope, expr.span(), path.name(), is_mutable);
        }
        case ExprKind::Attribute: {
            auto* attr = expr.as<ast::AttributeExpr>();
            return this->type_check_attribute_access(*attr, true, is_mutable);
        }
        case ExprKind::Index: {
            auto* index = expr.as<ast::IndexExpr>();
            return this->type_check_index_access(*index, true, is_mutable);
        }
        default: {
            Type* type = TRY(this->type_check(expr));
            if (!type->is_reference()) {
                return err(expr.span(), "Expected a reference type but got '{}'", type->str());
            }

            if (is_mutable && !type->is_mutable()) {
                return err(expr.span(), "Cannot take a mutable reference to an immutable value");
            }

            return type;
        }
    }

    return {};
}

ErrorOr<Type*> TypeChecker::type_check_attribute_access(ast::AttributeExpr const& expr, bool as_reference, bool as_mutable) {
    Type* parent = TRY(this->resolve_reference(expr.parent(), as_mutable));
    bool is_mutable = parent->is_mutable();

    parent = parent->get_reference_type();
    if (parent->is_reference() || parent->is_pointer()) {
        parent = parent->underlying_type();
    }

    auto* structure = m_state.get_global_struct(parent);
    RefPtr<Scope> scope = nullptr;

    if (parent->is_trait()) {
        auto* trait = m_state.get_trait(parent);
        if (!trait) {
            return err(expr.parent().span(), "Cannot access attributes of type '{}'", parent->str());
        }

        auto scope = trait->scope();
        auto* function = scope->resolve<Function>(expr.attribute());

        if (!function) {
            return err(expr.span(), "Trait '{}' has no attribute named '{}'", trait->name(), expr.attribute());
        }

        auto& self = function->parameters().front();
        if (self.is_mutable() && !is_mutable) {
            return err(expr.parent().span(), "Method '{}' requires a mutable reference to self but self is immutable", function->name());
        }

        m_has_self = true;
        return function->underlying_type()->get_pointer_to();
    }

    if (!structure) {
        if (!m_state.has_impl(parent)) {
            for (auto& impl : m_state.generic_impls()) {
                scope = TRY(impl->make(m_state, parent));
                if (scope) {
                    break;
                }
            }

            if (!scope) {
                return err(expr.parent().span(), "Cannot access attributes of type '{}'", parent->str());
            }
        } else {
            auto& impl = *m_state.impls().at(parent);
            scope = impl.scope();
        }
    }

    String const& attr = expr.attribute();
    auto* method = scope->resolve<Function>(attr);

    if (method) {
        // FIXME: Handle the case where the function comes from an impl not a struct
        if (!method->is_public() && m_state.structure() != structure && method->module() != m_state.module()) {
            return err(expr.span(), "Cannot access private method '{}' of struct '{}'", method->name(), structure->qualified_name());
        }

        auto& self = method->parameters().front();
        if (self.is_mutable() && !is_mutable) {
            return err(expr.parent().span(), "Method '{}' requires a mutable reference to self but self is immutable", method->name());
        }

        m_has_self = true;
        return method->underlying_type()->get_pointer_to();
    }

    if (!structure) {
        return err(expr.span(), "Type '{}' has no attribute named '{}'", parent->str(), attr);
    }

    auto* field = structure->find(attr);
    if (!field) {
        return err(expr.span(), "Unknown attribute '{}' for struct '{}'", attr, structure->name());
    } else if (!field->is_public() && m_state.structure() != structure && structure->module() != m_state.module()) {
        return err(expr.span(), "Cannot access private field '{}'", field->name);
    }

    Type* type = field->type;
    if (as_reference) {
        return type->get_reference_to(as_mutable);
    } else {
        return type;
    }
}

ErrorOr<Type*> TypeChecker::type_check_index_access(
    ast::IndexExpr const& expr, bool as_reference, bool as_mutable
) {
    auto result = this->resolve_reference(expr.value(), as_mutable);
    Type* type = nullptr;

    if (result.is_err()) {
        type = TRY(this->type_check(expr.value()));
        if (!type->is_array() || type->is_pointer()) {
            return err(expr.value().span(), "Cannot index into type '{}'", type->str());
        }
    } else {
        type = result.value()->get_reference_type();
    }

    Type* inner = nullptr;
    if (type->is_array()) {
        inner = type->get_array_element_type();
    } else {
        inner = type->get_pointee_type();
    }

    Type* index_type = TRY(this->type_check(expr.index()));
    if (!index_type->is_int()) {
        return err(expr.index().span(), "Expected an integer");
    }

    if (as_reference) {
        return inner->get_reference_to(as_mutable);
    } else {
        return inner;
    }
}

ErrorOr<Type*> TypeChecker::type_check(ast::Expr const& expr) {
    switch (expr.kind()) {
    // NOLINTNEXTLINE
    #define Op(x) case ast::ExprKind::x: return type_check(static_cast<ast::x##Expr const&>(expr));
        ENUMERATE_EXPR_KINDS(Op)
    #undef Op
    }

    ASSERT(false, "Unreachable");
    return nullptr;
}

ErrorOr<Type*> TypeChecker::type_check(ast::BlockExpr const& expr) {
    for (auto& e : expr.block()) {
        TRY(this->type_check(*e));
    }

    return {};
}

ErrorOr<Type*> TypeChecker::type_check(ast::ExternBlockExpr const& expr) {
    for (auto& e : expr.block()) {
        TRY(this->type_check(*e));
    }

    return {};
}


ErrorOr<Type*> TypeChecker::type_check(ast::IntegerExpr const& expr) {
    IntType* type = nullptr;
    Type* context = m_state.type_context();

    if (context && context->is_int()) {
        type = context->as<IntType>();
    } else if (expr.suffix().type != ast::BuiltinType::None) {
        // We are 100% sure we get an int type from get_type_from_builtin so casting here is ok
        type = m_state.get_type_from_builtin(expr.suffix().type)->as<IntType>();
    } else {
        type = m_state.context().i32();
    }

    return type;
}

ErrorOr<Type*> TypeChecker::type_check(ast::StringExpr const&) {
    return m_state.context().cstr();
}

ErrorOr<Type*> TypeChecker::type_check(ast::BoolExpr const& expr) {
    if (expr.value() == ast::BoolExpr::Null) {
        Type* type = m_state.type_context();
        if (!type) {
            type = m_state.context().void_type()->get_pointer_to();
        }

        return type;
    }

    return m_state.context().i1();
}

ErrorOr<Type*> TypeChecker::type_check(ast::ArrayExpr const& expr) {
    Type* element_type = nullptr;
    for (auto& element : expr.elements()) {
        auto type = TRY(this->type_check(*element));
        if (!element_type) {
            element_type = type;
            continue;
        }

        if (!type->can_safely_cast_to(element_type)) {
            return err(element->span(), "Array elements must have the same type");
        }
    }

    return m_state.context().create_array_type(element_type, expr.elements().size());
}

ErrorOr<Type*> TypeChecker::type_check(ast::IdentifierExpr const& expr) {
    auto* symbol = m_state.scope()->resolve(expr.name());
    if (!symbol) {
        return err(expr.span(), "Unknown identifier '{}'", expr.name());
    }

    switch (symbol->type()) {
        case Symbol::Variable: {
            auto* variable = symbol->as<Variable>();
            return variable->value_type();
        }
        case Symbol::Function: {
            auto* function = symbol->as<Function>();
            return function->underlying_type()->get_pointer_to();
        }
        default:
            return err(expr.span(), "'{}' does not refer to a value", expr.name());
    }
}

ErrorOr<Type*> TypeChecker::type_check(ast::FloatExpr const& expr) {
    auto& ctx = m_state.context();
    return expr.is_double() ? ctx.f64() : ctx.f32();
}

ErrorOr<Type*> TypeChecker::type_check(ast::AssignmentExpr const& expr) {
    auto& identifier = expr.identifier();
    u8 flags = Variable::None;

    if (identifier.is_mutable) {
        flags |= Variable::Mutable;
    }

    if (expr.is_public()) {
        flags |= Variable::Public;
    }

    if (!expr.value()) {
        Type* type = TRY(expr.type()->evaluate(m_state));
        auto variable = Variable::create(identifier.value, 0, type, flags);

        variable->set_module(m_state.module());
        m_state.scope()->add_symbol(variable);

        return {};
    }

    Type* value_type = TRY(this->type_check(*expr.value()));
    Type* type = value_type;

    if (expr.type()) {
        type = TRY(expr.type()->evaluate(m_state));
    }

    if (!value_type->can_safely_cast_to(type)) {
        return err(
            expr.span(),
            "Cannot assign value of type '{}' to variable of type '{}'", 
            value_type->str(), 
            type->str()
        );
    }

    auto variable = Variable::create(identifier.value, 0, type, flags);
    
    variable->set_module(m_state.module());
    m_state.scope()->add_symbol(variable);

    return {};
}

ErrorOr<Type*> TypeChecker::type_check(ast::TupleAssignmentExpr const&) {
    return {};
}

ErrorOr<Type*> TypeChecker::type_check(ast::ConstExpr const& expr) {
    Type* value = TRY(this->type_check(expr.value()));
    if (expr.type()) {
        Type* type = TRY(expr.type()->evaluate(m_state));
        if (!value->can_safely_cast_to(type)) {
            return err(
                expr.span(),
                "Cannot assign value of type '{}' to constant of type '{}'", 
                value->str(), 
                type->str()
            );
        }

        value = type;
    }

    u8 flags = Variable::Constant;
    if (expr.is_public()) {
        flags |= Variable::Public;
    }

    auto constant = Variable::create(expr.name(), 0, value, flags);

    constant->set_module(m_state.module());
    m_state.scope()->add_symbol(constant);

    return {};
}

ErrorOr<Type*> TypeChecker::type_check(ast::UnaryOpExpr const& expr) {
    Type* value_type = TRY(this->type_check(expr.value()));
    switch (expr.op()) {
        case UnaryOp::Not: {
            return m_state.context().i1();
        }
        case UnaryOp::DeRef: {
            if (!value_type->is_pointer() && !value_type->is_reference()) {
                return err(expr.span(), "Cannot dereference value of type '{}'", value_type->str());
            }

            return value_type->underlying_type();
        }
        default:
            ASSERT(false, "Unimplemented unary operator");
    }

    return {};
}

ErrorOr<Type*> TypeChecker::type_check(ast::BinaryOpExpr const& expr) {
    if (expr.op() == BinaryOp::Assign) {
        if (!expr.lhs().is<ast::UnaryOpExpr>()) {
            Type* lhs = TRY(this->resolve_reference(expr.lhs()))->get_reference_type();
            Type* rhs = TRY(this->type_check(expr.rhs()));

            if (!rhs->can_safely_cast_to(lhs)) {
                return err(
                    expr.span(),
                    "Cannot assign value of type '{}' to variable of type '{}'", 
                    rhs->str(), 
                    lhs->str()
                );
            }

            return lhs;
        }

        auto* unary = expr.lhs().as<ast::UnaryOpExpr>();
        if (unary->op() != UnaryOp::DeRef) {
            return err(unary->span(), "Invalid left-hand side of assignment");
        }

        Type* lhs = TRY(this->type_check(unary->value()));
        Type* rhs = TRY(this->type_check(expr.rhs()));

        if (!lhs->is_pointer() && !lhs->is_reference()) {
            return err(unary->span(), "Cannot dereference a value of type '{}'", lhs->str());
        }

        if (!lhs->is_mutable()) {
            return err(unary->span(), "Cannot assign to a non-mutable reference");
        }

        lhs = lhs->underlying_type();
        if (!rhs->can_safely_cast_to(lhs)) {
            return err(
                expr.span(),
                "Cannot assign value of type '{}' to variable of type '{}'", 
                rhs->str(), 
                lhs->str()
            );
        }

        return lhs;
    }

    Type* lhs = TRY(this->type_check(expr.lhs()));
    Type* rhs = TRY(this->type_check(expr.rhs()));

    if (!rhs->can_safely_cast_to(lhs)) {
        return err(
            expr.span(),
            "Cannot perform binary operation on operands of type '{}' and '{}'", 
            lhs->str(), 
            rhs->str()
        );
    }

    if (is_comparison_operator(expr.op())) {
        return m_state.context().i1();
    }

    return lhs;
}

ErrorOr<Type*> TypeChecker::type_check(ast::InplaceBinaryOpExpr const& expr) {
    Type* lhs = TRY(this->resolve_reference(expr.lhs(), true))->get_reference_type();
    Type* rhs = TRY(this->type_check(expr.rhs()));

    if (!rhs->can_safely_cast_to(lhs)) {
        return err(
            expr.span(),
            "Cannot perform binary operation on operands of type '{}' and '{}'", 
            lhs->str(), 
            rhs->str()
        );
    }

    return lhs;
}

ErrorOr<Type*> TypeChecker::type_check(ast::ReferenceExpr const& expr) {
    return this->resolve_reference(expr.value(), expr.is_mutable());
}

ErrorOr<Type*> TypeChecker::type_check(ast::CallExpr const& expr) {
    Type* callee = TRY(this->type_check(expr.callee()));
    if (callee->is_pointer()) {
        callee = callee->get_pointee_type();
    }

    if (!callee->is_function()) {
        return err(expr.span(), "Cannot call a value of type '{}'", callee->str());
    }

    auto& arguments = expr.args();
    auto* function_type = callee->as<FunctionType>();

    size_t params = function_type->parameter_count();
    size_t index = 0;

    if (m_has_self) {
        params--;
        index++;
    }

    if (function_type->is_var_arg() && arguments.size() < params) {
        return err(expr.span(), "Expected at least {} arguments but got {}", params, arguments.size());
    } else if (!function_type->is_var_arg() && arguments.size() != params) {
        return err(expr.span(), "Expected {} arguments but got {}", params, arguments.size());
    }

    for (auto& argument : arguments) {
        Type* type = TRY(this->type_check(*argument));
        if (index >= function_type->parameter_count()) {
            continue;
        }

        Type* parameter = function_type->get_parameter_at(index);

        if (!type->can_safely_cast_to(parameter)) {
            return err(
                argument->span(),
                "Cannot pass value of type '{}' to parameter of type '{}'", 
                type->str(), 
                parameter->str()
            );
        }

        index++;
    }

    m_has_self = false;
    return function_type->return_type();
}

ErrorOr<Type*> TypeChecker::type_check(ast::ReturnExpr const& expr) {
    Function* current_function = m_state.function();
    Type* return_type = current_function->return_type();

    if (expr.value()) {
        if (return_type->is_void()) {
            return err(expr.value()->span(), "Cannot return a value from a function that expects void");
        }

        Type* type = TRY(this->type_check(*expr.value()));
        if (!type->can_safely_cast_to(return_type)) {
            return err(
                expr.value()->span(),
                "Cannot return a value of type '{}' from a function that expects '{}'",
                type->str(),
                return_type->str()
            );
        }

        return {};
    } else if (!return_type->is_void()) {
        return err(expr.span(), "Cannot return void from a function that expects '{}'", return_type->str());
    }

    return {};
}

ErrorOr<Type*> TypeChecker::type_check(ast::FunctionDeclExpr const& expr) {
    Vector<FunctionParameter> parameters;
    Vector<Type*> types;

    Type* self_type = m_state.self_type();

    u32 index = 0;
    for (auto& parameter : expr.parameters()) {
        Type* type = nullptr;
        u8 flags = parameter.flags;

        if (self_type && flags & FunctionParameter::Self) {
            type = self_type->get_pointer_to(flags & FunctionParameter::Mutable);
        } else {
            type = TRY(parameter.type->evaluate(m_state));
        }

        if (type->is_reference()) {
            bool is_mutable = flags & FunctionParameter::Mutable;
            if (type->is_mutable() && !is_mutable) {
                flags |= FunctionParameter::Mutable;
            } else if (is_mutable && !type->is_mutable()) {
                return err(parameter.span, "Cannot declare a mutable parameter that takes an immutable reference");
            }
        }

        parameters.push_back({ parameter.name, type, flags, static_cast<u32>(index), parameter.span });
        types.push_back(type);
        
        index++;
    }

    Type* return_type = m_state.context().void_type();
    if (expr.return_type()) {
        return_type = TRY(expr.return_type()->evaluate(m_state));
    }

    auto* underlying_type = FunctionType::get(m_state.context(), return_type, types, expr.is_c_variadic());
    auto scope = Scope::create(expr.name(), ScopeType::Function, m_state.scope());

    RefPtr<LinkInfo> link_info = nullptr;

    auto function = Function::create(
        expr.span(),
        expr.name(),
        move(parameters),
        underlying_type,
        scope,
        expr.linkage(),
        move(link_info),
        expr.is_public()
    );

    function->set_module(m_state.module());
    if (auto* original = m_state.get_global_function(function->qualified_name())) {
        auto error = err(expr.span(), "Function '{}' is already defined", function->qualified_name());
        error.add_note(original->span(), "Previous definition is here");

        return error;
    }

    m_state.scope()->add_symbol(function);

    return {};
}

ErrorOr<Type*> TypeChecker::type_check(ast::FunctionExpr const& expr) {
    TRY(this->type_check(expr.decl()));

    auto* function = m_state.scope()->resolve<Function>(expr.decl().name());
    
    auto* previous_function = m_state.function();
    auto previous_scope = m_state.scope();

    for (auto& parameter : function->parameters()) {
        u8 flags = Variable::None;
        if (parameter.is_mutable()) {
            flags |= Variable::Mutable;
        }

        auto variable = Variable::create(parameter.name, 0, parameter.type, flags);
        function->scope()->add_symbol(variable);
    }

    m_state.set_current_function(function);
    m_state.set_current_scope(function->scope());

    for (auto& statement : expr.body()) {
        TRY(this->type_check(*statement));
    }

    // TODO: Ensure all code paths return

    m_state.set_current_function(previous_function);
    m_state.set_current_scope(previous_scope);

    return {};
}

ErrorOr<Type*> TypeChecker::type_check(ast::DeferExpr const& expr) {
    TRY(this->type_check(expr.expr()));
    return {};
}

ErrorOr<Type*> TypeChecker::type_check(ast::IfExpr const& expr) {
    Type* condition = TRY(this->type_check(expr.condition()));
    if (!condition->can_safely_cast_to(m_state.context().i1())) {
        return err(expr.condition().span(), "If conditions must be booleans");
    }

    TRY(this->type_check(expr.body()));
    if (expr.else_body()) {
        TRY(this->type_check(*expr.else_body()));
    }

    return {};
}

ErrorOr<Type*> TypeChecker::type_check(ast::WhileExpr const& expr) {
    Type* condition = TRY(this->type_check(expr.condition()));
    if (!condition->can_safely_cast_to(m_state.context().i1())) {
        return err(expr.condition().span(), "While conditions must be booleans");
    }

    TRY(this->type_check(expr.body()));
    return {};
}

ErrorOr<Type*> TypeChecker::type_check(ast::BreakExpr const&) {
    return {};
}

ErrorOr<Type*> TypeChecker::type_check(ast::ContinueExpr const&) {
    return {};
}

ErrorOr<Type*> TypeChecker::type_check(ast::StructExpr const& expr) {
    if (expr.is_opaque()) {
        auto* type = StructType::get(
            m_state.context(), Symbol::parse_qualified_name(expr.name(), m_state.scope()), {}
        );

        auto structure = Struct::create(expr.name(), type, m_state.scope(), expr.is_public());
        structure->set_module(m_state.module());

        m_state.scope()->add_symbol(structure);
        return {};
    }

    auto* type = StructType::get(m_state.context(), Symbol::parse_qualified_name(expr.name(), m_state.scope()), {});
    auto scope = Scope::create(expr.name(), ScopeType::Struct, m_state.scope());

    auto structure = Struct::create(expr.name(), type, {}, scope, expr.is_public());
    structure->set_module(m_state.module());

    type->set_struct(structure.get());
    m_state.scope()->add_symbol(structure);

    HashMap<String, StructField> fields;
    Vector<Type*> types;

    for (auto& field : expr.fields()) {
        Type* type = TRY(field.type->evaluate(m_state));
        if (!type->is_sized_type()) {
            return err(field.type->span(), "Field '{}' has an unsized type", field.name);
        } else if (type == structure->underlying_type()) {
            return err(field.type->span(), "Field '{}' has the same type as the struct itself", field.name);
        }


        fields.insert_or_assign(field.name, StructField { field.name, type, field.flags, field.index });
        types.push_back(type);
    }

    type->set_fields(types);
    structure->set_fields(move(fields));

    auto previous_scope = m_state.scope();

    m_state.set_current_scope(scope);
    m_state.set_current_struct(structure.get());

    m_state.set_self_type(structure->underlying_type());

    for (auto& expr : expr.members()) {
        TRY(this->type_check(*expr));
    }

    m_state.set_current_scope(previous_scope);
    m_state.set_self_type(nullptr);

    return {};
}

ErrorOr<Type*> TypeChecker::type_check(ast::ConstructorExpr const& expr) {
    Struct* structure = TRY(m_state.resolve_struct(expr.parent()));
    auto& fields = structure->fields();

    for (auto& argument : expr.arguments()) {
        auto iterator = fields.find(argument.name);
        if (iterator == fields.end()) {
            return err(argument.span, "Unknown field '{}' for struct '{}'", argument.name, structure->name());
        }

        auto& field = iterator->second;
        Type* type = TRY(this->type_check(*argument.value));

        if (!type->can_safely_cast_to(field.type)) {
            return err(
                argument.span,
                "Cannot assign value of type '{}' to field '{}' of type '{}'",
                type->str(),
                field.name,
                field.type->str()
            );
        }
    }

    return structure->underlying_type();
}

ErrorOr<Type*> TypeChecker::type_check(ast::EmptyConstructorExpr const& expr) {
    Struct* structure = TRY(m_state.resolve_struct(expr.parent()));
    return structure->underlying_type();
}


ErrorOr<Type*> TypeChecker::type_check(ast::AttributeExpr const& expr) {
    return this->type_check_attribute_access(expr, false, false);
}

ErrorOr<Type*> TypeChecker::type_check(ast::IndexExpr const& expr) {
    return this->type_check_index_access(expr, false, false);
}

ErrorOr<Type*> TypeChecker::type_check(ast::CastExpr const& expr) {
    TRY(this->type_check(expr.value()));
    return TRY(expr.to().evaluate(m_state));
}

// TODO: Properly type check these two
ErrorOr<Type*> TypeChecker::type_check(ast::SizeofExpr const&) {
    return m_state.context().u32();
}

ErrorOr<Type*> TypeChecker::type_check(ast::OffsetofExpr const&) {
    return m_state.context().u32();
}

ErrorOr<Type*> TypeChecker::type_check(ast::PathExpr const& expr) {
    auto* symbol = TRY(m_state.access_symbol(expr.span(), expr.path()));
    switch (symbol->type()) {
        case Symbol::Variable: {
            auto* variable = symbol->as<Variable>();
            return variable->value_type();
        }
        case Symbol::Function: {
            auto* function = symbol->as<Function>();
            return function->underlying_type()->get_pointer_to();
        }
        default:
            return err(expr.span(), "'{}' does not refer to a value", expr.path().format());
    }
}

ErrorOr<Type*> TypeChecker::type_check(ast::TupleExpr const& expr) {
    Vector<Type*> types;
    for (auto& element : expr.elements()) {
        types.push_back(TRY(this->type_check(*element)));
    }

    return TupleType::get(m_state.context(), types);
}

ErrorOr<Type*> TypeChecker::type_check(ast::EnumExpr const&) {
    return {};
}

// TODO: Everything below
ErrorOr<Type*> TypeChecker::type_check(ast::ImportExpr const&) {
    return {};
}

ErrorOr<Type*> TypeChecker::type_check(ast::UsingExpr const&) {
    return {};
}

ErrorOr<Type*> TypeChecker::type_check(ast::ModuleExpr const&) {
    return {};
}

ErrorOr<Type*> TypeChecker::type_check(ast::TernaryExpr const&) {
    return {};
}

ErrorOr<Type*> TypeChecker::type_check(ast::ForExpr const&) {
    return {};
}

ErrorOr<Type*> TypeChecker::type_check(ast::RangeForExpr const&) {
    return {};
}

ErrorOr<Type*> TypeChecker::type_check(ast::ArrayFillExpr const&) {
    return {};
}

ErrorOr<Type*> TypeChecker::type_check(ast::TypeAliasExpr const&) {
    return {};
}

ErrorOr<Type*> TypeChecker::type_check(ast::StaticAssertExpr const&) {
    return {};
}

ErrorOr<Type*> TypeChecker::type_check(ast::MaybeExpr const&) {
    return {};
}

ErrorOr<Type*> TypeChecker::type_check(ast::MatchExpr const&) {
    return {};
}

ErrorOr<Type*> TypeChecker::type_check(ast::ImplExpr const&) {
    return {};
}

ErrorOr<Type*> TypeChecker::type_check(ast::TraitExpr const&) {
    return {};
}

ErrorOr<Type*> TypeChecker::type_check(ast::ImplTraitExpr const&) {
    return {};
}

ErrorOr<Type*> TypeChecker::type_check(ast::ConstEvalExpr const&) {
    return {};
}

}