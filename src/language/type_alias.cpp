#include <quart/language/type_alias.h>

namespace quart {

bool TypeAlias::all_parameters_have_default() const {
    return llvm::all_of(m_parameters, [](const auto& param) { return param.is_optional(); });
}

Type* TypeAlias::evaluate(State& state) {
    auto range = llvm::map_range(m_parameters, [](const auto& param) { return param.default_type; });
    return this->evaluate(state, Vector<Type*>(range.begin(), range.end()));
}

Type* TypeAlias::evaluate(State&, const Vector<Type*>&) {
    // FIXME: Implement
    return nullptr;
}

}