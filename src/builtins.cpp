#include "builtins.h"
#include "visitor.h"
#include "utils/filesystem.h"
#include "utils/log.h"

#define ENTRY(n) { "__builtin_"#n, builtin_##n }

BUILTIN(include_str) {
    auto& expr = call->get(0);
    if (expr->kind() != ast::ExprKind::String) {
        ERROR(expr->span, "Expected a string literal");
    }

    auto str = expr->as<ast::StringExpr>();
    utils::fs::Path path(str->value);
    
    if (!path.exists()) {
        ERROR(expr->span, "File '{0}' does not exist", str->value);
    }

    auto stream = path.read();
    return visitor.to_str(stream.str());
}



void Builtins::init(Visitor& visitor) {
    visitor.builtins = {
        ENTRY(include_str)
    };
}