#pragma once

#include <quart/language/symbol.h>

namespace quart {

class Trait : Symbol {
public:

    size_t current_tag() const { return m_current_tag; }

private:
    Trait(String name) : Symbol(move(name), Symbol::Trait, false) {}

    size_t m_current_tag = 0;
};

}