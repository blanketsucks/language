#include <quart/language/state.h>

namespace quart {

bytecode::Register State::allocate_register() {
    bytecode::Register reg = m_generator.allocate_register();
    m_registers.push_back(nullptr);

    return reg;
}

void State::set_register_type(bytecode::Register reg, quart::Type* type) {
    m_registers[reg.index()] = type;
}

quart::Type* State::type(bytecode::Operand operand) const {
    if (operand.is_immediate()) {
        return operand.value_type();
    } else {
        return m_registers[operand.value()];
    }
}

ErrorOr<Scope*> State::resolve_scope_path(Span span, const Path& path) {
    Scope* scope = m_current_scope;
    for (auto& segment : path.segments) {
        Span segment_span = { span.start(), span.start() + segment.size() };
        auto* symbol = scope->resolve(segment);
        
        if (!symbol) {
            return err(segment_span, "namespace '{0}' not found", segment);
        }

        if (!symbol->is(Symbol::Module, Symbol::Struct)) {
            return err(segment_span, "'{0}' is not a valid namespace", segment);
        }

        if (symbol->is<Module>()) {
            scope = symbol->as<Module>()->scope();
        } else {
            scope = symbol->as<Struct>()->scope();
        }

        span.set_start(segment_span.end() + 2);
    }

    return scope;
}

}
