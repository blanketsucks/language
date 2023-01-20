#include "objects/typealias.h"

TypeAlias TypeAlias::from_enum(utils::Ref<Enum> enumeration) {
    return { enumeration->name, enumeration->type, enumeration, enumeration->span };
}

TypeAlias TypeAlias::null() {
    return { "", nullptr, nullptr, Span() };
}

bool TypeAlias::is_null() {
    return this->name.empty() && !this->type && !this->enumeration;
}