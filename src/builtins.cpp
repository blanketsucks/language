#include <quart/builtins.h>
#include <quart/visitor.h>
#include <quart/filesystem.h>
#include <quart/logging.h>

#define ENTRY(n) { "__builtin_"#n, builtin_##n }

using namespace quart;

BUILTIN(include_str) {
    auto& expr = call->get(0);
    if (expr->kind() != ast::ExprKind::String) {
        ERROR(expr->span, "Expected a string literal");
    }

    auto str = expr->as<ast::StringExpr>();
    fs::Path path(str->value);
    
    if (!path.exists()) {
        ERROR(expr->span, "File '{0}' does not exist", str->value);
    } else if (path.isdir()) {
        ERROR(expr->span, "'{0}' is a directory", str->value);
    }

    auto stream = path.read();
    return visitor.to_str(stream.str());
}

void Builtins::init(Visitor& visitor) {
    visitor.builtins = {
        ENTRY(include_str)
    };
}