#include <quart/parser/attrs.h>
#include <quart/parser/parser.h>
#include <quart/utils/utils.h>
#include <quart/utils/log.h>

#define ENTRY(n) { #n, handle_##n##_attribute }

SIMPLE_ATTR(noreturn, Attribute::Noreturn)
SIMPLE_ATTR(packed, Attribute::Packed)

ATTR(llvm_intrinsic) {
    parser.expect(TokenKind::LParen, "(");
    std::string name = parser.expect(TokenKind::Identifier, "identifier").value;
    parser.expect(TokenKind::RParen, ")");

    return Attribute(Attribute::LLVMIntrinsic, name);
}

ATTR(link) {
    static std::vector<std::string> VALID_LINK_KEYS = {"name", "export", "arch", "section"};

    parser.expect(TokenKind::LParen, "(");
    std::map<std::string, std::string> args;

    Token token = parser.expect(TokenKind::Identifier, "identifier");
    std::string key = token.value;

    Span span = token.span;
    while (utils::contains(VALID_LINK_KEYS, key)) {
        parser.expect(TokenKind::Assign, "=");

        auto value = parser.expect(TokenKind::String, "string").value;
        args[key] = value;

        if (parser.current == TokenKind::Comma) {
            parser.next();
            token = parser.expect(TokenKind::Identifier, "identifier");

            key = token.value;
            span = token.span;
        } else {
            break;
        }
    }

    if (!utils::contains(VALID_LINK_KEYS, key)) {
        ERROR(span, "Invalid 'link' attribute key '{0}'", key);
    }

    parser.expect(TokenKind::RParen, ")");
    return Attribute(Attribute::Link, args);
}

void Attributes::init(Parser& parser) {
    parser.attributes = {
        ENTRY(noreturn),
        ENTRY(packed),
        ENTRY(llvm_intrinsic),
        ENTRY(link)
    };
}