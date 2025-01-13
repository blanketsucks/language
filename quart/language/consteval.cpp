#include <quart/language/consteval.h>
#include <quart/language/state.h>

// These are expressions that are always not constant no matter what
// NOTE: Call, Return, EmptyConstructor, Cast and Ternary could be implemented in the future.
#define ENUMERATE_NON_CONSTANT_EXPR(Op)         \
    Op(ExternBlock)                             \
    Op(Assignment)                              \
    Op(TupleAssignment)                         \
    Op(Const)                                   \
    Op(Reference)                               \
    Op(InplaceBinaryOp)                         \
    Op(Call)                                    \
    Op(Return)                                  \
    Op(FunctionDecl)                            \
    Op(Function)                                \
    Op(Defer)                                   \
    Op(Struct)                                  \
    Op(EmptyConstructor)                        \
    Op(Cast)                                    \
    Op(Using)                                   \
    Op(Enum)                                    \
    Op(Import)                                  \
    Op(Ternary)                                 \
    Op(ArrayFill)                               \
    Op(TypeAlias)                               \
    Op(StaticAssert)                            \
    Op(Maybe)                                   \
    Op(Module)                                  \
    Op(Impl)                                    \
    Op(Trait)                                   \
    Op(ImplTrait)                               \
    Op(Match)                                   \
    Op(RangeFor)

namespace quart {

bool ConstantEvaluator::is_constant_expression(ast::Expr const& expr) const {
    switch (expr.kind()) {
    // NOLINTNEXTLINE
    #define Op(x) case ast::ExprKind::x: return is_constant_expression(static_cast<ast::x##Expr const&>(expr));
        ENUMERATE_EXPR_KINDS(Op)
    #undef Op
    }

    ASSERT(false, "Unreachable");
    return false;
}

ErrorOr<Constant*> ConstantEvaluator::evaluate(ast::Expr const& expr) {
    switch (expr.kind()) {
    // NOLINTNEXTLINE
    #define Op(x) case ast::ExprKind::x: return evaluate(static_cast<ast::x##Expr const&>(expr));
        ENUMERATE_EXPR_KINDS(Op)
    #undef Op
    }

    ASSERT(false, "Unreachable");
    return nullptr;
}

// NOLINTNEXTLINE
#define Op(x) bool ConstantEvaluator::is_constant_expression(ast::x##Expr const&) const { return false; }
    ENUMERATE_NON_CONSTANT_EXPR(Op)
#undef Op

// NOLINTNEXTLINE
#define Op(x) ErrorOr<Constant*> ConstantEvaluator::evaluate(ast::x##Expr const& expr) { return err(expr.span(), "Expression is not constant"); }
    ENUMERATE_NON_CONSTANT_EXPR(Op)
#undef Op

bool ConstantEvaluator::is_constant_expression(ast::BlockExpr const& expr) const {
    for (auto const& e : expr.block()) {
        if (!this->is_constant_expression(*e)) {
            return false;
        }
    }

    return true;
}

ErrorOr<Constant*> ConstantEvaluator::evaluate(ast::BlockExpr const& expr) {
    for (auto const& e : expr.block()) {
        if (auto result = this->evaluate(*e); result.is_err()) {
            return result;
        }
    }

    return nullptr;
}

bool ConstantEvaluator::is_constant_expression(ast::IntegerExpr const&) const {
    return true;
}

ErrorOr<Constant*> ConstantEvaluator::evaluate(ast::IntegerExpr const& expr) {
    Type* context = m_state.type_context();

    IntType* type = nullptr;
    if (context && context->is_int()) {
        type = context->as<IntType>();
    } else if (expr.suffix().type != ast::BuiltinType::None) {
        type = m_state.get_type_from_builtin(expr.suffix().type)->as<IntType>();
    } else {
        type = m_state.context().i32();
    }

    return ConstantInt::get(m_state.context(), type, expr.value());
}

bool ConstantEvaluator::is_constant_expression(ast::FloatExpr const&) const {
    return true;
}

ErrorOr<Constant*> ConstantEvaluator::evaluate(ast::FloatExpr const& expr) {
    Type* type = expr.is_double() ? m_state.context().f64() : m_state.context().f32();
    return ConstantFloat::get(m_state.context(), type, expr.value());
}

bool ConstantEvaluator::is_constant_expression(ast::StringExpr const&) const {
    return true;
}

ErrorOr<Constant*> ConstantEvaluator::evaluate(ast::StringExpr const& expr) {
    Type* type = m_state.context().cstr();
    return ConstantString::get(m_state.context(), type, expr.value());
}

bool ConstantEvaluator::is_constant_expression(ast::IdentifierExpr const& expr) const {
    auto* variable = m_state.scope()->resolve<Variable>(expr.name());
    if (!variable) {
        return false;
    }

    return variable->is_constant();
}

ErrorOr<Constant*> ConstantEvaluator::evaluate(ast::IdentifierExpr const& expr) {
    // FIXME: Allow for functions too
    auto* variable = m_state.scope()->resolve<Variable>(expr.name());
    if (!variable) {
        return err(expr.span(), "Unknown identifier '{0}'", expr.name());
    }

    if (!variable->is_constant()) {
        return err(expr.span(), "Variable '{0}' is not constant", expr.name());
    }

    return variable->initializer();
}

bool ConstantEvaluator::is_constant_expression(ast::ArrayExpr const& expr) const {
    for (auto const& e : expr.elements()) {
        if (!this->is_constant_expression(*e)) {
            return false;
        }
    }

    return true;
}

ErrorOr<Constant*> ConstantEvaluator::evaluate(ast::ArrayExpr const& expr) {
    Vector<Constant*> elements;
    // FIXME: Type checking
    for (auto const& e : expr.elements()) {
        auto element = TRY(this->evaluate(*e));
        elements.push_back(element);
    }

    auto* type = ArrayType::get(m_state.context(), elements[0]->type(), elements.size());
    return ConstantArray::get(m_state.context(), type, elements);
}

bool ConstantEvaluator::is_constant_expression(ast::IndexExpr const& expr) const {
    return this->is_constant_expression(expr.value()) && this->is_constant_expression(expr.index());
}

ErrorOr<Constant*> ConstantEvaluator::evaluate(ast::IndexExpr const& expr) {
    Constant* value = TRY(this->evaluate(expr.value()));
    if (!value->is<ConstantArray>()) {
        return err(expr.span(), "Cannot index a non-array value");
    }

    Constant* idx = TRY(this->evaluate(expr.index()));
    if (!idx->is<ConstantInt>()) {
        return err(expr.span(), "Index must be an integer not {0}", idx->type()->str());
    }

    auto* array = value->as<ConstantArray>();
    u64 index = idx->as<ConstantInt>()->value();

    if (index >= array->size()) {
        return err(expr.span(), "Index out of bounds. Array has {0} elements", array->size());
    }

    return array->at(index);
}

bool ConstantEvaluator::is_constant_expression(ast::ConstructorExpr const& expr) const {
    for (auto const& argument : expr.arguments()) {
        if (!this->is_constant_expression(*argument.value)) {
            return false;
        }
    }

    return true;
}

ErrorOr<Constant*> ConstantEvaluator::evaluate(ast::ConstructorExpr const& expr) {
    Struct* structure = TRY(m_state.resolve_struct(expr.parent()));
    auto& fields = structure->fields();

    Vector<Constant*> arguments;
    arguments.resize(fields.size());

    for (auto const& argument : expr.arguments()) {
        Constant* value = TRY(this->evaluate(*argument.value));
        auto iterator = fields.find(argument.name);

        if (iterator == fields.end()) {
            return err(argument.value->span(), "Unknown field '{0}'", argument.name);
        }

        auto& field = iterator->second;
        arguments[field.index] = value;
    }

    return ConstantStruct::get(m_state.context(), structure->underlying_type(), arguments);
}

bool ConstantEvaluator::is_constant_expression(ast::AttributeExpr const& expr) const {
    return this->is_constant_expression(expr.parent());
}

ErrorOr<Constant*> ConstantEvaluator::evaluate(ast::AttributeExpr const& expr) {
    Constant* parent = TRY(this->evaluate(expr.parent()));
    if (!parent->is<ConstantStruct>()) {
        return err(expr.span(), "Cannot access attribute of a non-struct value");
    }

    auto* type = parent->type()->as<StructType>();
    auto* structure = type->get_struct();

    auto& fields = structure->fields();

    auto iterator = fields.find(expr.attribute());
    if (iterator == fields.end()) {
        return err(expr.span(), "Unknown attribute '{0}'", expr.attribute());
    }

    auto& field = iterator->second;
    auto* value = parent->as<ConstantStruct>();

    return value->at(field.index);
}

bool ConstantEvaluator::is_constant_expression(ast::UnaryOpExpr const& expr) const {
    if (expr.op() == UnaryOp::DeRef || expr.op() == UnaryOp::Ref) {
        return false;
    }

    return this->is_constant_expression(expr.value());
}

ErrorOr<Constant*> ConstantEvaluator::evaluate(ast::UnaryOpExpr const&) {
    ASSERT(false, "Not implemented");
    return nullptr;
}

bool ConstantEvaluator::is_constant_expression(ast::BinaryOpExpr const& expr) const {
    return this->is_constant_expression(expr.lhs()) && this->is_constant_expression(expr.rhs());
}

ErrorOr<Constant*> ConstantEvaluator::evaluate(ast::BinaryOpExpr const&) {
    ASSERT(false, "Not implemented");
    return nullptr;
}

bool ConstantEvaluator::is_constant_expression(ast::IfExpr const& expr) const {
    bool is_condition_constant = this->is_constant_expression(expr.condition());
    bool is_body_constant = this->is_constant_expression(expr.body());

    if (!is_condition_constant || !is_body_constant) {
        return false;
    }

    if (expr.else_body()) {
        return this->is_constant_expression(*expr.else_body());
    }

    return true;
}

ErrorOr<Constant*> ConstantEvaluator::evaluate(ast::IfExpr const&) {
    ASSERT(false, "Not implemented");
    return nullptr;
}

bool ConstantEvaluator::is_constant_expression(ast::WhileExpr const& expr) const {
    return this->is_constant_expression(expr.condition()) && this->is_constant_expression(expr.body());
}

ErrorOr<Constant*> ConstantEvaluator::evaluate(ast::WhileExpr const&) {
    ASSERT(false, "Not implemented");
    return nullptr;
}

bool ConstantEvaluator::is_constant_expression(ast::ForExpr const& expr) const {
    return this->is_constant_expression(expr.iterable()) && this->is_constant_expression(expr.body());
}

ErrorOr<Constant*> ConstantEvaluator::evaluate(ast::ForExpr const&) {
    ASSERT(false, "Not implemented");
    return nullptr;
}

bool ConstantEvaluator::is_constant_expression(ast::BreakExpr const&) const {
    // TODO: Maybe if we are inside a constantly evaluated loop, this should be true?
    return false;
}

ErrorOr<Constant*> ConstantEvaluator::evaluate(ast::BreakExpr const&) {
    ASSERT(false, "Not implemented");
    return nullptr;
}

bool ConstantEvaluator::is_constant_expression(ast::ContinueExpr const&) const {
    // TODO: Maybe if we are inside a constantly evaluated loop, this should be true?
    return false;
}

ErrorOr<Constant*> ConstantEvaluator::evaluate(ast::ContinueExpr const&) {
    ASSERT(false, "Not implemented");
    return nullptr;
}

bool ConstantEvaluator::is_constant_expression(ast::SizeofExpr const&) const {
    return true;
}

ErrorOr<Constant*> ConstantEvaluator::evaluate(ast::SizeofExpr const&) {
    ASSERT(false, "Not implemented");
    return nullptr;
}

bool ConstantEvaluator::is_constant_expression(ast::OffsetofExpr const&) const {
    return true;
}

ErrorOr<Constant*> ConstantEvaluator::evaluate(ast::OffsetofExpr const&) {
    ASSERT(false, "Not implemented");
    return nullptr;
}

bool ConstantEvaluator::is_constant_expression(ast::PathExpr const& expr) const {
    auto& path = expr.path();

    auto result = m_state.resolve_scope_path(expr.span(), path);
    if (result.is_err()) {
        return false;
    }

    Scope* scope = result.value();
    auto* variable = scope->resolve<Variable>(path.name());

    return variable && variable->is_constant();
}
  
ErrorOr<Constant*> ConstantEvaluator::evaluate(ast::PathExpr const& expr) {
    auto& path = expr.path();

    Scope* scope = TRY(m_state.resolve_scope_path(expr.span(), path));
    auto* variable = scope->resolve<Variable>(path.name());

    if (!variable) {
        return err(expr.span(), "Unknown identifier '{0}'", path.format());
    }

    if (!variable->is_constant()) {
        return err(expr.span(), "Variable '{0}' is not constant", path.name());
    }

    return variable->initializer();
}

bool ConstantEvaluator::is_constant_expression(ast::TupleExpr const& expr) const {
    for (auto const& e : expr.elements()) {
        if (!this->is_constant_expression(*e)) {
            return false;
        }
    }

    return true;
}

ErrorOr<Constant*> ConstantEvaluator::evaluate(ast::TupleExpr const&) {
    ASSERT(false, "Not implemented");
    return nullptr;
}

bool ConstantEvaluator::is_constant_expression(ast::BoolExpr const&) const {
    return true;
}

ErrorOr<Constant*> ConstantEvaluator::evaluate(ast::BoolExpr const&) {
    ASSERT(false, "Not implemented");
    return nullptr;
}

}