#include <quart/language/functions.h>
#include <quart/language/scopes.h>

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
    bool is_public
) {
    return RefPtr<Function>(new Function(span, move(name), move(parameters), underlying_type, move(scope), linkage_specifier, move(link_info), is_public));
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