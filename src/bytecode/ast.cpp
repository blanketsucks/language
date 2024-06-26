#include <quart/language/state.h>
#include <quart/parser/ast.h>

namespace quart::ast {

static inline bytecode::Register select_dst(State& state, Optional<bytecode::Register> dst) {
    return dst.value_or(state.allocate_register());
}

BytecodeResult BlockExpr::generate(State& state, Optional<bytecode::Register>) const {
    for (auto& expr : m_block) {
        TRY(expr->generate(state, {}));
    }

    return {};
}

BytecodeResult ExternBlockExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

BytecodeResult IntegerExpr::generate(State& state, Optional<bytecode::Register> dst) const {
    auto* type = state.types().create_int_type(m_width, true);
    auto op = bytecode::Operand(m_value, type);
    if (!dst.has_value()) {
        return op;
    }

    state.emit<bytecode::Move>(*dst, op);
    state.set_register_type(*dst, type);

    return bytecode::Operand(*dst);
}

BytecodeResult StringExpr::generate(State& state, Optional<bytecode::Register> dst) const {
    auto reg = select_dst(state, dst);
    state.emit<bytecode::NewString>(reg, m_value);

    state.set_register_type(reg, state.types().cstr());
    return bytecode::Operand(reg);
}

BytecodeResult ArrayExpr::generate(State& state, Optional<bytecode::Register> dst) const {
    if (m_elements.empty()) {
        return err(span(), "Empty array expressions are not allowed");
    }

    auto& registry = state.types();

    auto reg = select_dst(state, dst);
    Vector<bytecode::Operand> ops;

    quart::Type* array_element_type = nullptr;
    for (auto& expr : m_elements) {
        auto option = TRY(expr->generate(state, {}));
        if (!option.has_value()) {
            return err(expr->span(), "Expected an expression");
        }

        bytecode::Operand operand = option.value();
        if (ops.empty()) {
            ops.push_back(operand);
            array_element_type = state.type(operand);

            continue;
        }
    
        auto* type = state.type(operand);
        if (!type->can_safely_cast_to(array_element_type)) {
            return err(expr->span(), "Array elements must have the same type");
        } else if (type != array_element_type) {
            auto reg = state.allocate_register();

            state.emit<bytecode::Cast>(reg, operand, array_element_type);
            state.set_register_type(reg, array_element_type);

            ops.emplace_back(reg);
        } else {
            ops.push_back(operand);
        }
    }

    state.emit<bytecode::NewArray>(reg, ops);
    auto* type = registry.create_array_type(array_element_type, ops.size());

    state.set_register_type(reg, type);
    return bytecode::Operand(reg);
}

BytecodeResult IdentifierExpr::generate(State& state, Optional<bytecode::Register> dst) const {
    auto* symbol = state.scope()->resolve(m_name);
    if (!symbol) {
        return err(span(), "Unknown identifier '{0}'", m_name);
    }

    switch (symbol->type()) {
        case Symbol::Variable: {
            auto* variable = symbol->as<Variable>();
            auto reg = select_dst(state, dst);

            state.emit<bytecode::GetLocal>(reg, variable->local_index());
            state.set_register_type(reg, variable->value_type());

            return bytecode::Operand(reg);
        }
        case Symbol::Function: {
            auto* function = symbol->as<Function>();
            auto reg = select_dst(state, dst);

            // FIXME: Maybe it would be better to address functions by an index rather than name since both can be unique.
            state.emit<bytecode::GetFunction>(reg, function->qualified_name());
            state.set_register_type(reg, function->underlying_type()->get_pointer_to());

            return bytecode::Operand(reg);
        }
        default:
            return err(span(), "'{0}' does not refer to a value", m_name);
    }
}

BytecodeResult FloatExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

BytecodeResult AssignmentExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

BytecodeResult TupleAssignmentExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

BytecodeResult ConstExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

BytecodeResult UnaryOpExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

BytecodeResult BinaryOpExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

BytecodeResult InplaceBinaryOpExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

BytecodeResult ReferenceExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

BytecodeResult CallExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

BytecodeResult ReturnExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

BytecodeResult FunctionDeclExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

BytecodeResult FunctionExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

BytecodeResult DeferExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

BytecodeResult IfExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

BytecodeResult WhileExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

BytecodeResult BreakExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

BytecodeResult ContinueExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

BytecodeResult StructExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

BytecodeResult ConstructorExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

BytecodeResult EmptyConstructorExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

BytecodeResult AttributeExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

BytecodeResult IndexExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

BytecodeResult CastExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

BytecodeResult SizeofExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

BytecodeResult OffsetofExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

BytecodeResult PathExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

BytecodeResult TupleExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

BytecodeResult EnumExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

BytecodeResult ImportExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

BytecodeResult UsingExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

BytecodeResult ModuleExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

BytecodeResult TernaryExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

BytecodeResult ForExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

BytecodeResult RangeForExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

BytecodeResult ArrayFillExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

BytecodeResult TypeAliasExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

BytecodeResult StaticAssertExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

BytecodeResult MaybeExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

BytecodeResult MatchExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

BytecodeResult ImplExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

}