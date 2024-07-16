#include <quart/parser/attrs.h>
#include <quart/parser/parser.h>
#include <quart/common.h>

#include <llvm/ADT/STLExtras.h>

#define ENTRY(n) { #n, parse_##n##_attribute }

namespace quart {

SIMPLE_ATTRIBUTE(noreturn, Attribute::Noreturn)
SIMPLE_ATTRIBUTE(packed, Attribute::Packed)

ATTRIBUTE(llvm_intrinsic) {
    TRY(parser.expect(TokenKind::LParen));

    String name = TRY(parser.expect(TokenKind::String)).value();
    TRY(parser.expect(TokenKind::RParen));

    return Attribute { Attribute::LLVMIntrinsic, name };
}

ATTRIBUTE(link) {
    static const std::vector<std::string> VALID_LINK_KEYS = {
        "name", "export", "arch", "section", "platform"
    };
    
    TRY(parser.expect(TokenKind::LParen));
    HashMap<String, String> args;

    while (true) {
        Token token = TRY(parser.expect(TokenKind::Identifier));
        String key = token.value();

        Span span = token.span();
        if (!llvm::is_contained(VALID_LINK_KEYS, key)) {
            return err(span, "Invalid 'link' attribute key '{0}'", key);
        }

        TRY(parser.expect(TokenKind::Assign));

        String value = TRY(parser.expect(TokenKind::String)).value();
        args[key] = value;

        auto option = parser.try_expect(TokenKind::Comma);
        if (option.has_value()) {
            token = TRY(parser.expect(TokenKind::Identifier));

            key = token.value();
            span = token.span();
        } else {
            break;
        }
    }

    TRY(parser.expect(TokenKind::RParen));
    return Attribute { Attribute::Link, args };
}

void Attributes::init(Parser& parser) {
    parser.set_attributes({
        ENTRY(noreturn),
        ENTRY(packed),
        ENTRY(llvm_intrinsic),
        ENTRY(link)
    });
}

ATTRIBUTE_HANDLER(link) {
    auto args = attr.as<HashMap<String, String>>();
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

}