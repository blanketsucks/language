#include "objects/typealias.h"

TypeAlias TypeAlias::from_enum(utils::Shared<Enum> enumeration) {
    return { enumeration->name, enumeration->type, enumeration, enumeration->start, enumeration->end };
}

TypeAlias TypeAlias::empty() {
    return { "", nullptr, nullptr, Location(), Location() };
}

bool TypeAlias::is_empty() {
    return this->name.empty() && !this->type && !this->enumeration;
}