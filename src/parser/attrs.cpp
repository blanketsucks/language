#include <quart/parser/attrs.h>
#include <quart/parser/parser.h>
#include <quart/logging.h>
#include <quart/common.h>

#include <llvm/ADT/STLExtras.h>

#define ENTRY(n) { #n, parse_##n##_attribute }

using namespace quart;

SIMPLE_ATTR(noreturn, Attribute::Noreturn)
SIMPLE_ATTR(packed, Attribute::Packed)

ATTR(llvm_intrinsic) {
    parser.expect(TokenKind::LParen, "(");
    std::string name = parser.expect(TokenKind::String, "string").value;
    parser.expect(TokenKind::RParen, ")");

    return Attribute(Attribute::LLVMIntrinsic, name);
}

ATTR(link) {
    static std::vector<std::string> VALID_LINK_KEYS = {
        "name", "export", "arch", "section", "platform"
    };
    
    parser.expect(TokenKind::LParen, "(");
    std::map<std::string, std::string> args;

    Token token = parser.expect(TokenKind::Identifier, "identifier");
    std::string key = token.value;

    Span span = token.span;

    while (llvm::is_contained(VALID_LINK_KEYS, key)) {
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

    if (!llvm::is_contained(VALID_LINK_KEYS, key)) {
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

ATTR_HANDLER(link) {
    auto args = attr.as<std::map<std::string, std::string>>();
    if (args.find("platform") != args.end()) {
        if (args["platform"] != PLATFORM_NAME) {
            return AttributeHandler::Skip;
        }
    }

    return AttributeHandler::Ok;
}

AttributeHandler::Result AttributeHandler::handle(Parser& parser, const Attribute& attr) {
    switch (attr.type) {
        case Attribute::Link: return handle_link_attribute(parser, attr);
        default: return AttributeHandler::Ok;
    }
}