#pragma once

#include <quart/common.h>
#include <quart/errors.h>

namespace quart {

class Parser;

class AttributeParser {
public:

    static ErrorOr<HashMap<String, String>> parse_call_like_attribute(Parser&, const String& name, const Set<String>& allowed_parameters);
};

}