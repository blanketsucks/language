#include <quart/language/functions.h>
#include <quart/language/scopes.h>
#include <quart/language/state.h>

#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/StringExtras.h>

namespace quart {

RefPtr<Function> Function::create(
    Span span,
    String name,
    Vector<FunctionParameter> parameters,
    FunctionType* underlying_type, 
    RefPtr<Scope> scope,
    LinkageSpecifier linkage_specifier,
    RefPtr<LinkInfo> link_info,
    bool is_public,
    bool is_async
) {
    return RefPtr<Function>(new Function(span, move(name), move(parameters), underlying_type, move(scope), linkage_specifier, move(link_info), is_public, is_async));
}

void Function::set_qualified_name() {
    if (m_link_info && !m_link_info->name.empty()) {
        m_qualified_name = m_link_info->name;
    } else if (m_linkage_specifier == LinkageSpecifier::C) {
        m_qualified_name = name();
    } else {
        m_qualified_name = Symbol::parse_qualified_name(name(), m_scope->parent());
    }
}

void Function::set_local_parameters() {
    for (auto& param : parameters()) {
        size_t index = this->allocate_local();
        this->set_local_type(index, param.type);

        u8 flags = Variable::None;
        if (param.is_mutable()) {
            flags |= Variable::Mutable;
        }

        Type* type = param.type;
        if (param.is_byval()) {
            type = type->get_pointee_type();
        }

        auto variable = Variable::create(param.name, index, type, flags);
        m_scope->add_symbol(variable);
    }
}

ErrorOr<void> Function::finalize_body(State& state) {
    for (auto& block : m_basic_blocks) {
        if (!block->is_terminated() && !this->return_type()->is_void()) {
            return err(span(), "Function '{}' does not return from all paths", this->name());
        } else if (!block->is_terminated()) {
            state.switch_to(block);
            state.emit<bytecode::Return>();
        }
    }

    return {};
}

ErrorOr<RefPtr<Function>> Function::specialize(State& state, Vector<FunctionParameter> const& parameters) {
    SpecializedFunctionKey key;

    key.parameters.reserve(parameters.size());
    for (auto& param : parameters) {
        key.parameters.push_back(param.type);
    }

    auto it = m_specializations.find(key);
    if (it != m_specializations.end()) {
        return it->second;
    }

    String name = format(
        "{}<{}>",
        this->name(),
        format_range(parameters, [](auto& param) { return param.type->str(); })
    );

    auto scope = Scope::create(name, ScopeType::Function, m_scope->parent());
    auto underlying_type = FunctionType::get(
        state.context(),
        this->return_type(),
        key.parameters,
        this->underlying_type()->is_function_var_arg()
    );

    auto function = RefPtr<Function>(new Function(
        m_span,
        name,
        parameters,
        underlying_type,
        scope,
        m_linkage_specifier,
        m_link_info,
        is_public(),
        m_is_async
    ));

    function->set_local_parameters();

    m_specializations.insert_or_assign(key, function);
    
    auto previous_scope = state.scope();
    auto previous_function = state.function();
    auto previous_block = state.current_block();
    
    state.switch_to(nullptr);
    state.emit<bytecode::NewFunction>(function.get());

    auto entry_block = state.create_block();
    function->set_entry_block(entry_block);

    state.switch_to(entry_block);
    state.set_current_scope(function->scope());
    state.set_current_function(function.get());

    state.emit<bytecode::NewLocalScope>(function.get());

    TRY(m_body->generate(state));
    TRY(function->finalize_body(state));

    state.set_current_scope(previous_scope);
    state.set_current_function(previous_function);
    state.switch_to(previous_block);

    state.add_global_function(function);

    return function;
}

void Function::dump() const {
    auto range = format_range(m_parameters, [](auto& param) { return param.type->str(); });
    out("function {}({}) -> {}", m_qualified_name, range, return_type()->str());

    if (is_decl()) {
        outln(";");
        return;
    } else {
        outln(":");
    }

    for (auto& block : m_basic_blocks) {
        block->dump();
        outln();
    }
}

}