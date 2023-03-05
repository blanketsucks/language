#include "parser/attrs.h"
#include "parser/parser.h"

#define ENTRY(n) { #n, handle_##n##_attribute }

SIMPLE_ATTR(noreturn, Attribute::Noreturn)
SIMPLE_ATTR(packed, Attribute::Packed)

ATTR(llvm_intrisinc) {
    parser.expect(TokenKind::LParen, "(");
    std::string name = parser.expect(TokenKind::Identifier, "identifier").value;
    parser.expect(TokenKind::RParen, ")");

    return Attribute(Attribute::LLVMIntrinsic, name);
}

ATTR(impl) {
    parser.expect(TokenKind::LParen, "(");
    auto type = parser.parse_type();
    parser.expect(TokenKind::RParen, ")");

    return Attribute(Attribute::Impl, std::move(type));
}

void Attributes::init(Parser& parser) {
    parser.attributes = {
        ENTRY(noreturn),
        ENTRY(packed),
        ENTRY(llvm_intrisinc),
        ENTRY(impl)
    };
}