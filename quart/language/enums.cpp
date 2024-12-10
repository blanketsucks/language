#include <quart/language/enums.h>

namespace quart {

RefPtr<Enum> Enum::create(String name, quart::Type* underlying_type, Scope* scope) {
    return RefPtr<Enum>(new Enum(move(name), underlying_type, scope));
}
    
}
