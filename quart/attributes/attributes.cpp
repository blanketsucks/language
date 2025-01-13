#include <quart/attributes/attributes.h>
#include <quart/parser/parser.h>
#include <quart/attributes/parser.h>
#include <quart/common.h>
#include <quart/target.h>

#include <llvm/ADT/STLExtras.h>

#define ATTRIBUTE(n) ErrorOr<Attribute> parse_##n##_attribute(Parser& parser)
#define ATTRIBUTE_HANDLER(n) AttributeHandler::Result handle_##n##_attribute(Parser& parser, const Attribute& attr)

#define SIMPLE_ATTRIBUTE(n, t) ATTRIBUTE(n) { (void)parser; return Attribute { t }; }

#define ENTRY(n) { #n, parse_##n##_attribute }

namespace quart {

SIMPLE_ATTRIBUTE(noreturn, Attribute::Noreturn)
SIMPLE_ATTRIBUTE(packed, Attribute::Packed)

ATTRIBUTE(link) {
    static const Set<String> ALLOWED_LINK_PARAMETERS = { "name", "arch", "section", "platform" };
    HashMap<String, String> args = TRY(AttributeParser::parse_call_like_attribute(parser, "link", ALLOWED_LINK_PARAMETERS));
    
    auto info = make_ref<LinkInfo>(args);
    return Attribute { Attribute::Link, info };
}

void Attributes::init(Parser& parser) {
    parser.set_attributes({
        ENTRY(noreturn),
        ENTRY(packed),
        ENTRY(link)
    });
}

ATTRIBUTE_HANDLER(link) {
    (void)parser;

    auto info = attr.value<RefPtr<LinkInfo>>();
    auto& target = Target::build();
    
    if (info->has_arch()) {
        auto& arch = info->arch;
        return static_cast<AttributeHandler::Result>(target.arch() != Target::normalize(arch));
    }

    if (info->has_platform()) {
        auto& platform = info->platform;

        StringView os = target.os();
        if (os.empty()) {
            os = PLATFORM_NAME;
        }

        return static_cast<AttributeHandler::Result>(os != platform);
    }

    return AttributeHandler::Ok;
}

AttributeHandler::Result AttributeHandler::handle(Parser& parser, const Attribute& attr) {
    switch (attr.type()) {
        case Attribute::Link:
            return handle_link_attribute(parser, attr);
        default:
            return AttributeHandler::Ok;
    }
}

}