#include <quart/attributes/parser.h>
#include <quart/parser/parser.h>

namespace quart {

ErrorOr<HashMap<String, String>> AttributeParser::parse_call_like_attribute(
    Parser& parser, const String& name, const Set<String>& allowed_parameters
) {
    HashMap<String, String> args;
    TRY(parser.expect(TokenKind::LParen));

    while (true) {
        Token token = TRY(parser.expect(TokenKind::Identifier));
        String key = token.value();

        Span span = token.span();
        if (!allowed_parameters.contains(key)) {
            return err(span, "Invalid '{0}' attribute key '{1}'", name, key);
        } else if (args.contains(key)) {
            return err(span, "A value for '{0}' has already been provided", key);
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
    return args;
}

}