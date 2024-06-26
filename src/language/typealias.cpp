#include <quart/language/typealias.h>

namespace quart {

bool TypeAlias::all_parameters_have_default() const {
    return std::all_of(m_parameters.begin(), m_parameters.end(), [](const auto& parameter) {
        return parameter.is_optional();
    });
}

Type* TypeAlias::evaluate(State&) {
    // FIXME: Implement
    return nullptr;
}

Type* TypeAlias::evaluate(State&, const Vector<Type*>&) {
    // FIXME: Implement
    return nullptr;
}

}