#include <quart/parser/parser.h>
#include <quart/parser/attrs.h>
#include <quart/parser/ast.h>
#include <quart/utils/utils.h>
#include <quart/utils/log.h>

#include "llvm/ADT/StringRef.h"

#include <memory>
#include <string>
#include <cstring>
#include <vector>

static std::vector<ast::ExprKind> NUMERIC_KINDS = {
    ast::ExprKind::String,
    ast::ExprKind::Integer,
    ast::ExprKind::Float
};

static std::vector<TokenKind> NAMESPACE_ALLOWED_KEYWORDS = {
    TokenKind::Struct,
    TokenKind::Func,
    TokenKind::Type,
    TokenKind::Const,
    TokenKind::Enum,
    TokenKind::Namespace,
    TokenKind::Extern
};

static std::vector<TokenKind> STRUCT_ALLOWED_KEYWORDS = {
    TokenKind::Func,
    TokenKind::Type,
    TokenKind::Const
};

// {name: (bits, is_float)}
static std::map<std::string, std::pair<uint32_t, bool>> NUM_TYPES_BIT_MAPPING = {
    {"i8", {8, false}},
    {"i16", {16, false}},
    {"i32", {32, false}},
    {"i64", {64, false}},
    {"i128", {128, false}},
    {"f32", {32, true}},
    {"f64", {64, true}}
};

std::map<std::string, ast::BuiltinType> Parser::TYPES = {
    {"void", ast::BuiltinType::Void},
    {"bool", ast::BuiltinType::Bool},
    {"i8", ast::BuiltinType::i8},
    {"i16", ast::BuiltinType::i16},
    {"i32", ast::BuiltinType::i32},
    {"i64", ast::BuiltinType::i64},
    {"i128", ast::BuiltinType::i128},
    {"f32", ast::BuiltinType::f32},
    {"f64", ast::BuiltinType::f64}
};


Parser::Parser(std::vector<Token> tokens) : tokens(tokens) {
    this->index = 0;
    this->current = this->tokens.front();

    // Populate the hash map with already defined precedences.
    for (auto pair : PRECEDENCES) {
        this->precedences[pair.first] = pair.second;
    }

    Attributes::init(*this);
}

void Parser::end() {
    if (this->current != TokenKind::SemiColon) {
        Token last = this->tokens[this->index - 1];
        ERROR(last.span, "Expected ';'");
    }

    this->next();
}

Token Parser::next() {
    this->index++;

    if (this->index >= this->tokens.size()) {
        this->current = this->tokens.back();
    } else {
        this->current = this->tokens[this->index];
    }

    return this->current;
}

Token Parser::peek(uint32_t offset) {
    if (this->index >= this->tokens.size()) {
        return this->tokens.back();
    }

    return this->tokens[this->index + offset];
}

Token Parser::expect(TokenKind type, std::string value) {
    if (this->current.type != type) {
        ERROR(this->current.span, "Expected {0}", value);
    }

    Token token = this->current;
    this->next();

    return token;
}

bool Parser::is_valid_attribute(std::string name) {
    return this->attributes.find(name) != this->attributes.end();
}

int Parser::get_token_precendence() {
    int precedence = this->precedences[this->current.type];
    if (precedence <= 0) {
        return -1;
    }

    return precedence;
}

utils::Scope<ast::TypeExpr> Parser::parse_type() {
    utils::Scope<ast::TypeExpr> type = nullptr;

    Span start = this->current.span;

    bool is_reference = false;
    bool is_immutable = true;

    bool has_mut = false;
    if (this->current == TokenKind::Mut) {
        is_immutable = false; has_mut = true;
        this->next();
    }

    if (this->current == TokenKind::BinaryAnd) {
        is_reference = true;
        this->next();
    }

    if (has_mut && !is_reference) {
        ERROR(start, "Cannot use 'mut' without '&'");
    }

    if (this->current.match(TokenKind::Identifier, "int")) {
        this->next();
        this->expect(TokenKind::LParen, "(");

        auto size = this->expr(false);
        Span end = this->expect(TokenKind::RParen, ")").span;

        type = utils::make_scope<ast::IntegerTypeExpr>(Span::from_span(start, end), std::move(size));
    } else if (this->current == TokenKind::Func) {
        // func(int, int) -> int 
        this->next(); this->expect(TokenKind::LParen, "(");

        std::vector<utils::Scope<ast::TypeExpr>> args;
        while (this->current != TokenKind::RParen) {
            args.push_back(this->parse_type());
            if (this->current != TokenKind::Comma) {
                break;
            }
            
            this->next();
        }

        Span end = this->expect(TokenKind::RParen, ")").span;
        utils::Scope<ast::TypeExpr> ret = nullptr;

        if (this->current == TokenKind::Arrow) {
            end = this->next().span; ret = this->parse_type();
        }

        type = utils::make_scope<ast::FunctionTypeExpr>(Span::from_span(start, end), std::move(args), std::move(ret));
    } else if (this->current == TokenKind::LParen) {
        this->next();
        if (this->current == TokenKind::RParen) {
            ERROR(this->current.span, "Tuple type literals must atleast have a single element");
        }

        std::vector<utils::Scope<ast::TypeExpr>> types;
        while (this->current != TokenKind::RParen) {
            types.push_back(this->parse_type());
            if (this->current != TokenKind::Comma) {
                break;
            }

            this->next();
        }

        Span end = this->expect(TokenKind::RParen, ")").span;
        type = utils::make_scope<ast::TupleTypeExpr>(Span::from_span(start, end), std::move(types));
    } else if (this->current == TokenKind::LBracket) {
        this->next();
        auto element = this->parse_type();

        this->expect(TokenKind::SemiColon, ";");
        auto size = this->expr(false);

        Span end = this->expect(TokenKind::RBracket, "]").span;
        type = utils::make_scope<ast::ArrayTypeExpr>(
            Span::from_span(start, end), std::move(element), std::move(size)
        );
    } else {
        std::string name = this->expect(TokenKind::Identifier, "identifier").value;
        if (Parser::TYPES.find(name) != Parser::TYPES.end()) {
            type = utils::make_scope<ast::BuiltinTypeExpr>(
                Span::from_span(start, this->current.span), Parser::TYPES[name]
            );
        } else {
            Path p = this->parse_path(name);
            type = utils::make_scope<ast::NamedTypeExpr>(
                Span::from_span(start, this->current.span), p.name, p.path
            );
        }
    }

    while (this->current == TokenKind::Mul) {
        Span end = this->next().span;
        type = utils::make_scope<ast::PointerTypeExpr>(Span::from_span(start, end), std::move(type));
    }

    if (is_reference) {
        type = utils::make_scope<ast::ReferenceTypeExpr>(
            Span::from_span(start, this->current.span), std::move(type), is_immutable
        );
    }

    return type;
} 

utils::Scope<ast::BlockExpr> Parser::parse_block() {
    std::vector<utils::Scope<ast::Expr>> body;
    Span start = this->current.span;

    while (this->current != TokenKind::RBrace) {
        ast::Attributes attrs = this->parse_attributes();
        auto expr = this->statement();

        if (expr) {
            expr->attributes.update(attrs);
            body.push_back(std::move(expr));
        }
    }

    Span end = this->expect(TokenKind::RBrace, "}").span;
    return utils::make_scope<ast::BlockExpr>(Span::from_span(start, end), std::move(body));
}

utils::Scope<ast::TypeAliasExpr> Parser::parse_type_alias() {
    Span start = this->current.span;

    std::string name = this->expect(TokenKind::Identifier, "identifier").value;
    this->expect(TokenKind::Assign, "=");

    auto type = this->parse_type();
    Span end = this->expect(TokenKind::SemiColon, ";").span;

    return utils::make_scope<ast::TypeAliasExpr>(
        Span::from_span(start, end), name, std::move(type)
    );
}

std::pair<std::vector<ast::Argument>, bool> Parser::parse_arguments() {
    std::vector<ast::Argument> args;

    bool has_kwargs = false;
    bool has_default_values = false;
    bool is_c_variadic = false;
    bool is_variadic = false;

    while (this->current != TokenKind::RParen) {
        std::string argument = this->current.value;
        if (!this->current.match(
            {TokenKind::Mut, TokenKind::Identifier, TokenKind::Mul, TokenKind::Ellipsis}
        )) {
            ERROR(this->current.span, "Expected argument name, '*' or '...'");
        }

        bool is_immutable = true;
        Span span = this->current.span;

        if (this->current == TokenKind::Ellipsis) {
            if (is_c_variadic) {
                ERROR(this->current.span, "Cannot have multiple variadic arguments");
            }

            is_c_variadic = true;
            this->next();

            break;
        } else if (this->current == TokenKind::Mul && this->peek() == TokenKind::Identifier) {
            if (is_variadic) {
                ERROR(this->current.span, "Cannot have multiple variadic arguments");
            } else if (is_c_variadic) {
                ERROR(this->current.span, "Cannot mix `...` and `*` variadic arguments");
            } else if (has_kwargs) {
                ERROR(this->current.span, "Cannot have a variadic argument after `*`");
            }
            
            is_variadic = true;

            this->next();

            argument = this->current.value;
        } else if (this->current == TokenKind::Mul) {
            if (has_kwargs) {
                ERROR(this->current.span, "Only one '*' seperator is allowed in a function prototype");
            }

            has_kwargs = true;
            this->next();

            this->expect(TokenKind::Comma, ",");
            continue;
        } else if (this->current == TokenKind::Mut) {
            this->next();
            is_immutable = false;

            if (this->current != TokenKind::Identifier) {
                ERROR(this->current.span, "Expected identifier");
            }

            argument = this->current.value;
            span = Span::from_span(span, this->current.span);
        }

        utils::Scope<ast::TypeExpr> type = nullptr;
        bool is_self = false;

        if (argument == "self" && (this->is_inside_struct || this->self_usage_allowed) && !has_kwargs) {
            is_self = true; this->next();
        } else {
            this->next(); this->expect(TokenKind::Colon, ":");
            type = this->parse_type();
        }

        utils::Scope<ast::Expr> default_value = nullptr;
        if (this->current == TokenKind::Assign) {
            has_default_values = true;
            this->next();

            default_value = this->expr(false);
        } else {
            if (has_default_values) {
                ERROR(this->current.span, "Cannot have non-default arguments follow default arguments");
            }
        }

        if (default_value && is_variadic) {
            ERROR(this->current.span, "Cannot have a default value for a variadic argument");
        }
        
        args.push_back({
            argument, 
            std::move(type),
            std::move(default_value),
            is_self, 
            has_kwargs, 
            is_immutable,
            is_variadic,
            span
        });

        // Everything after `*args` is a keyword argument since it cannot be passed in positionally
        if (is_variadic) { has_kwargs = true; is_variadic = false; }

        if (this->current != TokenKind::Comma) {
            break;
        }

        this->next();
    }

    this->expect(TokenKind::RParen, ")");
    return {std::move(args), is_c_variadic};
}

utils::Scope<ast::PrototypeExpr> Parser::parse_prototype(
    ast::ExternLinkageSpecifier linkage, bool with_name, bool is_operator
) {
    Span start = this->current.span;
    std::string name;

    if (with_name) {
        name = this->expect(TokenKind::Identifier, "function name").value;
        this->expect(TokenKind::LParen, "(");
    }

    auto pair = this->parse_arguments();

    Span end = this->current.span;
    utils::Scope<ast::TypeExpr> ret = nullptr;

    if (this->current == TokenKind::Arrow) {
        end = this->next().span; ret = this->parse_type();
    }

    return utils::make_scope<ast::PrototypeExpr>(
        Span::from_span(start, end), name, std::move(pair.first), std::move(ret), pair.second, is_operator, linkage
    );
}

utils::Scope<ast::Expr> Parser::parse_function_definition(
    ast::ExternLinkageSpecifier linkage, bool is_operator
) {
    Span start = this->current.span;
    auto prototype = this->parse_prototype(linkage, true, is_operator);

    if (this->current == TokenKind::SemiColon) {
        this->next();
        return prototype;
    }
    
    this->expect(TokenKind::LBrace, "{");
    this->is_inside_function = true;

    std::vector<utils::Scope<ast::Expr>> body;
    while (!this->current.match({TokenKind::RBrace, TokenKind::EOS})) {
        ast::Attributes attrs = this->parse_attributes();
        auto expr = this->statement();

        if (expr) {
            expr->attributes.update(attrs);
            body.push_back(std::move(expr));
        }
    }

    auto end = this->expect(TokenKind::RBrace, "}").span;
    this->is_inside_function = false;

    return utils::make_scope<ast::FunctionExpr>(
        Span::from_span(start, end), std::move(prototype), std::move(body)
    );
}

utils::Scope<ast::FunctionExpr> Parser::parse_function() {
    Span start = this->current.span;
    auto prototype = this->parse_prototype(ast::ExternLinkageSpecifier::Unspecified, true, false);
    
    this->expect(TokenKind::LBrace, "{");
    this->is_inside_function = true;

    std::vector<utils::Scope<ast::Expr>> body;
    while (!this->current.match({TokenKind::RBrace, TokenKind::EOS})) {
        ast::Attributes attrs = this->parse_attributes();
        auto expr = this->statement();

        if (expr) {
            expr->attributes.update(attrs);
            body.push_back(std::move(expr));
        }
    }

    auto end = this->expect(TokenKind::RBrace, "}").span;
    this->is_inside_function = false;

    return utils::make_scope<ast::FunctionExpr>(
        Span::from_span(start, end), std::move(prototype), std::move(body)
    );
}

utils::Scope<ast::IfExpr> Parser::parse_if_statement() {
    Span start = this->current.span;
    auto condition = this->expr(false);

    utils::Scope<ast::Expr> body;
    if (this->current != TokenKind::LBrace) {
        body = this->statement();
    } else {
        this->next();
        body = this->parse_block();
    }

    Span end = body->span;
    utils::Scope<ast::Expr> else_body;
    if (this->current == TokenKind::Else) {
        this->next();
        if (this->current != TokenKind::LBrace) {
            else_body = this->statement();
        } else {
            this->next();
            else_body = this->parse_block();
        }

        end = else_body->span;
    }

    return utils::make_scope<ast::IfExpr>(
        Span::from_span(start, end), std::move(condition), std::move(body), std::move(else_body)
    );
}

utils::Scope<ast::StructExpr> Parser::parse_struct() {
    Span start = this->current.span;
    Token token = this->expect(TokenKind::Identifier, "struct name");
    
    std::string name = token.value;
    Span end = token.span;

    std::vector<ast::StructField> fields;
    std::vector<utils::Scope<ast::Expr>> methods;
    std::vector<utils::Scope<ast::Expr>> parents;

    if (this->current == TokenKind::SemiColon) {
        this->next();
        return utils::make_scope<ast::StructExpr>(
            Span::from_span(start, this->current.span), name, true, std::move(parents), std::move(fields), std::move(methods)
        );
    }

    this->is_inside_struct = true;
    if (this->current == TokenKind::LParen) {
        this->next();
        while (this->current != TokenKind::RParen) {
            parents.push_back(this->expr(false));
            if (this->current != TokenKind::Comma) {
                break;
            }

            this->next();
        }

        this->expect(TokenKind::RParen, ")");
    }

    this->expect(TokenKind::LBrace, "{");
    uint32_t index = 0;

    while (this->current != TokenKind::RBrace) {
        ast::Attributes attributes = this->parse_attributes();

        bool is_private = false;
        bool is_readonly = false;
        bool is_operator = false;

        switch (this->current.type) {
            case TokenKind::Private: is_private = true; this->next(); break;
            case TokenKind::Readonly: is_readonly = true; this->next(); break;
            case TokenKind::Operator: is_operator = true; this->next(); break;
            default: break;
        }


        if (this->current != TokenKind::Identifier && !this->current.match(STRUCT_ALLOWED_KEYWORDS)) {
            ERROR(this->current.span, "Expected field name or function definition");
        }

        switch (this->current.type) {
            case TokenKind::Func: {
                this->next();
                auto definition = this->parse_function_definition(
                    ast::ExternLinkageSpecifier::None, is_operator
                );

                // TODO: fix private 

                definition->attributes.update(attributes);
                methods.push_back(std::move(definition));
            } break;
            case TokenKind::Const: {
                this->next();
                methods.push_back(this->parse_variable_definition(true));
            } break;
            case TokenKind::Type: {
                this->next();
                methods.push_back(this->parse_type_alias());
            } break;
            default: {
                if (index == UINT32_MAX) {
                    ERROR(this->current.span, "Cannot define more than {0} fields inside a struct", UINT32_MAX);
                }

                std::string name = this->current.value;
                this->next();

                this->expect(TokenKind::Colon, ":");
                fields.push_back({
                    name, 
                    this->parse_type(), 
                    index,
                    is_private, 
                    is_readonly
                });

                this->expect(TokenKind::SemiColon, ";");
                index++;
            }
        }
    }

    this->expect(TokenKind::RBrace, "}");
    this->is_inside_struct = false;

    return utils::make_scope<ast::StructExpr>(
        Span::from_span(start, end), name, false, std::move(parents), std::move(fields), std::move(methods)
    );    
}

utils::Scope<ast::Expr> Parser::parse_variable_definition(bool is_const) {
    Span span = this->current.span;

    utils::Scope<ast::TypeExpr> type = nullptr;
    std::vector<ast::Ident> names;

    bool is_multiple_variables = false;
    bool has_consume_rest = false;
    bool is_immutable = true;

    if (is_const && this->current == TokenKind::Mut) {
        ERROR(this->current.span, "Cannot use 'mut' with 'const'");
    } else if (!is_const && this->current == TokenKind::Mut) {
        is_immutable = false;
        this->next();
    }

    std::string consume_rest;
    if (this->current == TokenKind::LParen) {
        this->next();

        bool mut_prefixed = !is_immutable;
        while (this->current != TokenKind::RParen) {
            if (mut_prefixed && this->current == TokenKind::Mut) {
                NOTE(this->current.span, "Redundant 'mut'");
            }

            if (mut_prefixed) {
                is_immutable = false;
            }
            
            if (this->current == TokenKind::Mut) {
                is_immutable = false;
                this->next();
            }

            if (this->current == TokenKind::Mul) {
                if (has_consume_rest) {
                    ERROR(this->current.span, "Cannot consume rest of the arguments more than once");
                }

                this->next();
                has_consume_rest = true;
            }

            Token token = this->expect(TokenKind::Identifier, "variable name");
            names.push_back({token.value, is_immutable, token.span});

            if (this->current != TokenKind::Comma) {
                break;
            }

            this->next();
            is_immutable = true;
        }

        this->expect(TokenKind::RParen, ")");
        if (names.empty()) {
            ERROR(span, "Expected atleast a single variable name");
        }

        is_multiple_variables = true;
    } else {
        Token token = this->expect(TokenKind::Identifier, "variable name");
        names.push_back({token.value, is_immutable, Span::from_span(span, token.span)});
    }

    if (this->current == TokenKind::Colon) {
        this->next(); type = this->parse_type();
    }
    
    utils::Scope<ast::Expr> expr = nullptr;
    Span end;

    if (this->current != TokenKind::Assign) {
        end = this->expect(TokenKind::SemiColon, ";").span;

        if (!type) {
            ERROR(this->current.span, "Un-initialized variables must have an inferred type");
        }
    } else {
        this->next();
        expr = this->expr(false);
        
        this->expect(TokenKind::SemiColon, ";");
        end = expr->span;
    };

    if (names.size() > 1 && !expr) {
        ERROR(expr->span, "Expected an expression when using multiple named assignments");
    }
    
    if (is_const) {
        if (!expr) {
            ERROR(this->current.span, "Constants must have an initializer");
        }

        return utils::make_scope<ast::ConstExpr>(span, names[0].value, std::move(type), std::move(expr));
    } else {
        return utils::make_scope<ast::VariableAssignmentExpr>(
            span, names, std::move(type), std::move(expr), consume_rest, false, is_multiple_variables
        );
    }
}

utils::Scope<ast::NamespaceExpr> Parser::parse_namespace() {
    Span start = this->current.span;
    Path p = this->parse_path();

    this->expect(TokenKind::LBrace, "{");
    std::vector<utils::Scope<ast::Expr>> members;

    while (this->current != TokenKind::RBrace) {
        ast::Attributes attrs = this->parse_attributes();
        if (!this->current.match(NAMESPACE_ALLOWED_KEYWORDS)) {
            ERROR(this->current.span, "Expected function, extern, struct, const, type, or namespace definition");
        }

        utils::Scope<ast::Expr> member;
        TokenKind kind = this->current.type;

        this->next();
        switch (kind) {
            case TokenKind::Func: member = this->parse_function_definition(); break;
            case TokenKind::Extern: member = this->parse_extern_block(); break;
            case TokenKind::Struct: member = this->parse_struct(); break;
            case TokenKind::Namespace: member = this->parse_namespace(); break;
            case TokenKind::Type: member = this->parse_type_alias(); break;
            case TokenKind::Const: member = this->parse_variable_definition(true); break;
            case TokenKind::Enum: member = this->parse_enum(); break;
            default: __UNREACHABLE
        }

        member->attributes.update(attrs);
        members.push_back(std::move(member));
    }

    Span end = this->expect(TokenKind::RBrace, "}").span;
    return utils::make_scope<ast::NamespaceExpr>(
        Span::from_span(start, end), p.name, p.path, std::move(members)
    );
}

utils::Scope<ast::Expr> Parser::parse_extern(ast::ExternLinkageSpecifier linkage) {
    utils::Scope<ast::Expr> definition;
    Span start = this->current.span;
    
    ast::Attributes attrs = this->parse_attributes();
    if (this->current == TokenKind::Identifier) {
        std::string name = this->current.value;
        Span span = this->current.span;

        std::string consume_rest;

        this->next();

        this->expect(TokenKind::Colon, ":");
        auto type = this->parse_type();

        utils::Scope<ast::Expr> expr;
        if (this->current == TokenKind::Assign) {
            expr = this->expr(false);
        }

        Span end = this->expect(TokenKind::SemiColon, ";").span;

        std::vector<ast::Ident> names = {{name, true, span},};
        definition = utils::make_scope<ast::VariableAssignmentExpr>(
            Span::from_span(start, end), names, std::move(type), std::move(expr), consume_rest, true, false
        );
    } else {
        if (this->current != TokenKind::Func) {
            ERROR(this->current.span, "Expected function");
        }

        this->next();
        definition = this->parse_function_definition(linkage);
    }

    definition->attributes.update(attrs);
    return definition;
}

utils::Scope<ast::Expr> Parser::parse_extern_block() {
    Span start = this->current.span;
    ast::ExternLinkageSpecifier linkage = ast::ExternLinkageSpecifier::Unspecified;

    if (this->current == TokenKind::String) {
        if (this->current.value != "C") {
            ERROR(this->current.span, "Unknown extern linkage specifier");
        }

        this->next();
        linkage = ast::ExternLinkageSpecifier::C;
    }

    // TODO: Allow const/let defitnitions
    if (this->current == TokenKind::LBrace) {
        std::vector<utils::Scope<ast::Expr>> definitions;
        this->next();

        while (this->current != TokenKind::RBrace) {
            auto definition = this->parse_extern(linkage);
            definitions.push_back(std::move(definition));
        }

        Span end = this->expect(TokenKind::RBrace, "}").span;
        return utils::make_scope<ast::BlockExpr>(Span::from_span(start, end), std::move(definitions));
    }

    return this->parse_extern(linkage);
}

utils::Scope<ast::EnumExpr> Parser::parse_enum() {
    Span start = this->current.span;
    std::string name = this->expect(TokenKind::Identifier, "enum name").value;

    utils::Scope<ast::TypeExpr> type;
    if (this->current == TokenKind::Colon) {
        this->next(); type = this->parse_type();
    }

    this->expect(TokenKind::LBrace, "{");

    std::vector<ast::EnumField> fields;
    while (this->current != TokenKind::RBrace) {
        std::string field = this->expect(TokenKind::Identifier, "enum field name").value;
        utils::Scope<ast::Expr> value = nullptr;

        if (this->current == TokenKind::Assign) {
            this->next();
            value = this->expr(false);
        }

        fields.push_back({field, std::move(value)});
        if (this->current != TokenKind::Comma) {
            break;
        }

        this->next();
    }

    Span end = this->expect(TokenKind::RBrace, "}").span;
    return utils::make_scope<ast::EnumExpr>(Span::from_span(start, end), name, std::move(type), std::move(fields));

}

utils::Scope<ast::Expr> Parser::parse_anonymous_function() {
    auto pair = this->parse_arguments();

    utils::Scope<ast::TypeExpr> ret = nullptr;
    if (this->current == TokenKind::Colon) {
        this->next();
        ret = this->parse_type();
    }

    this->expect(TokenKind::DoubleArrow, "=>");

    std::vector<utils::Scope<ast::Expr>> body;
    body.push_back(this->expr(false));
    
    auto prototype = utils::make_scope<ast::PrototypeExpr>(
        this->current.span,
        "",
        std::move(pair.first), 
        std::move(ret), 
        std::move(pair.second),
        false,
        ast::ExternLinkageSpecifier::None
    );
    
    return utils::make_scope<ast::FunctionExpr>(
        Span::from_span(prototype->span, this->current.span), std::move(prototype), std::move(body)
    );
}

Path Parser::parse_path(llvm::Optional<std::string> name) {
    Path p = {
        .name = name ? *name : this->expect(TokenKind::Identifier, "identifier").value,
        .path = {},
    };

    while (this->current == TokenKind::DoubleColon) {
        this->next();
        p.path.push_back(this->expect(TokenKind::Identifier, "identifier").value);
    }

    if (!p.path.empty()) {
        std::string back = p.path.back();
        p.path.pop_back();

        p.path.push_back(p.name);
        p.name = back;
    }

    return p;
}

std::vector<utils::Scope<ast::Expr>> Parser::parse() {
    return this->statements();
}

std::vector<utils::Scope<ast::Expr>> Parser::statements() {
    std::vector<utils::Scope<ast::Expr>> statements;

    while (this->current != TokenKind::EOS) {
        ast::Attributes attrs = this->parse_attributes();
        auto expr = this->statement();

        if (expr) {
            expr->attributes = attrs;
            statements.push_back(std::move(expr));
        }
    }

    return statements;
}

utils::Scope<ast::Expr> Parser::statement() {
    switch (this->current.type) {
        case TokenKind::Extern: {
            this->next();
            return this->parse_extern_block();
        } 
        case TokenKind::Func: {
            this->next();
            return this->parse_function_definition();
        }
        case TokenKind::Return: {
            if (!this->is_inside_function) {
                ERROR(this->current.span, "Return statement outside of function");
            }

            Span start = this->current.span;
            this->next();

            if (this->current == TokenKind::SemiColon) {
                Span end = this->current.span;

                this->next();
                return utils::make_scope<ast::ReturnExpr>(Span::from_span(start, end), nullptr);
            }

            auto expr = this->expr(false);

            Span end = this->expect(TokenKind::SemiColon, ";").span;
            return utils::make_scope<ast::ReturnExpr>(Span::from_span(start, end), std::move(expr));
        } 
        case TokenKind::If: {
            if (!this->is_inside_function) {
                ERROR(this->current.span, "If statement outside of function");
            }

            this->next();
            return this->parse_if_statement();
        } 
        case TokenKind::Let: {
            this->next();
            return this->parse_variable_definition(false);
        }
        case TokenKind::Const: {
            this->next();
            return this->parse_variable_definition(true);
        }
        case TokenKind::Struct: {
            this->next();
            return this->parse_struct();
        }
        case TokenKind::Namespace: {
            this->next();
            return this->parse_namespace();
        } 
        case TokenKind::Type: {
            this->next();
            return this->parse_type_alias();
        }
        case TokenKind::While: {
            bool has_outer_loop = this->is_inside_loop;
            Span start = this->current.span;

            this->next();
            auto condition = this->expr(false);

            this->expect(TokenKind::LBrace, "{");

            this->is_inside_loop = true;
            auto body = this->parse_block();

            this->is_inside_loop = has_outer_loop;
            return utils::make_scope<ast::WhileExpr>(Span::from_span(start, body->span), std::move(condition), std::move(body));
        } 
        // case TokenKind::For: {
        //     bool has_outer_loop = this->is_inside_loop;
        //     Span start_ = this->current.span;

        //     this->next();
        //     this->expect(TokenKind::LParen, "(");

        //     auto start = this->parse_variable_definition(false);

        //     auto end = this->expr(false);
        //     this->expect(TokenKind::SemiColon, ";");

        //     auto step = this->expr(false);
        //     this->expect(TokenKind::RParen, ")");

        //     this->expect(TokenKind::LBrace, "{");

        //     this->is_inside_loop = true;
        //     auto body = this->parse_block();

        //     this->is_inside_loop = has_outer_loop;
        //     return utils::make_scope<ast::ForExpr>(
        //         Span::from_span(start_, body->span), std::move(start), std::move(end), std::move(step), std::move(body)
        //     );
        // } 
        case TokenKind::Break: {
            if (!this->is_inside_loop) {
                ERROR(this->current.span, "Break statement outside of loop");
            }

            Span start = this->current.span;
            this->next();

            Span end = this->expect(TokenKind::SemiColon, ";").span;
            return utils::make_scope<ast::BreakExpr>(Span::from_span(start, end));
        } 
        case TokenKind::Continue: {
            if (!this->is_inside_loop) {
                ERROR(this->current.span, "Continue statement outside of loop");
            }

            Span start = this->current.span;
            this->next();

            Span end = this->expect(TokenKind::SemiColon, ";").span;
            return utils::make_scope<ast::ContinueExpr>(Span::from_span(start, end));
        } 
        case TokenKind::Using: {
            Span start = this->current.span;
            this->next();

            std::vector<std::string> members;
            if (this->current != TokenKind::LParen) {
                members.push_back(this->expect(TokenKind::Identifier, "identifier").value);
            } else {
                this->next();
                while (this->current != TokenKind::RParen) {
                    members.push_back(this->expect(TokenKind::Identifier, "identifier").value);
                    if (this->current != TokenKind::Comma) {
                        break;
                    }
                    this->next();
                }

                this->expect(TokenKind::RParen, ")");
            }

            if (this->current != TokenKind::From) {
                ERROR(this->current.span, "Expected 'from' keyword.");
            }

            this->next();
            auto parent = this->expr();

            return utils::make_scope<ast::UsingExpr>(Span::from_span(start, this->current.span), members, std::move(parent));
        } 
        case TokenKind::Defer: {
            if (!this->is_inside_function) {
                ERROR(this->current.span, "Defer statement outside of function");
            }

            Span start = this->current.span;
            this->next();

            auto expr = this->expr();
            return utils::make_scope<ast::DeferExpr>(Span::from_span(start, this->current.span), std::move(expr));
        } 
        case TokenKind::Enum: {
            this->next();
            return this->parse_enum();
        } 
        case TokenKind::Import: {
            Span start = this->current.span;
            this->next();

            bool is_relative = false;
            if (this->current == TokenKind::DoubleColon) {
                is_relative = true;
                this->next();
            }

            std::string name = this->expect(TokenKind::Identifier, "module name").value;
            bool is_wildcard = false;

            while (this->current == TokenKind::DoubleColon) {
                this->next();

                if (this->current == TokenKind::Mul) {
                    is_wildcard = true;
                    this->next();

                    break;
                }

                name += FORMAT("::{0}", this->expect(TokenKind::Identifier, "module name").value);
            }

            Span end = this->expect(TokenKind::SemiColon, ";").span;
            return utils::make_scope<ast::ImportExpr>(Span::from_span(start, end), name, is_wildcard, is_relative);
        }
        case TokenKind::Module: {
            this->next();
            std::string name = this->expect(TokenKind::Identifier, "module name").value;

            Span start = this->expect(TokenKind::LBrace, "{").span;
            std::vector<utils::Scope<ast::Expr>> body;

            while (this->current != TokenKind::RBrace) {
                body.push_back(this->statement());
            }

            Span end = this->expect(TokenKind::RBrace, "}").span;
            return utils::make_scope<ast::ModuleExpr>(
                Span::from_span(start, end), name, std::move(body)
            );
        }
        case TokenKind::For: {
            Span start = this->current.span;
            this->next();

            bool is_immutable = true;
            if (this->current == TokenKind::Mut) {
                is_immutable = false;
                this->next();
            }

            Token token = this->expect(TokenKind::Identifier, "identifier");
            ast::Ident ident = { token.value, is_immutable, token.span };

            this->expect(TokenKind::In, "in");

            bool outer = this->is_inside_loop;
            this->is_inside_loop = true;

            auto expr = this->expr(false);

            this->expect(TokenKind::LBrace, "{");

            auto body = this->parse_block();
            this->is_inside_loop = outer;

            return utils::make_scope<ast::ForeachExpr>(
                Span::from_span(start, body->span), ident, std::move(expr), std::move(body)
            );
        } 
        case TokenKind::StaticAssert: {
            Span start = this->current.span;
            this->next(); this->expect(TokenKind::LParen, "(");

            auto expr = this->expr(false);
            std::string message;
            if (this->current == TokenKind::Comma) {
                this->next(); message = this->expect(TokenKind::String, "string").value;
            }

            this->expect(TokenKind::RParen, ")"); 
            Span end = this->expect(TokenKind::SemiColon, ";").span;
                
            return utils::make_scope<ast::StaticAssertExpr>(Span::from_span(start, end), std::move(expr), message);
        }
        case TokenKind::Impl: {
            Span start = this->current.span;
            this->next();

            auto type = this->parse_type();
            this->expect(TokenKind::LBrace, "{");

            ast::ExprList<ast::FunctionExpr> body;
            this->self_usage_allowed = true;

            while (this->current != TokenKind::RBrace) {
                this->expect(TokenKind::Func, "func");
                body.push_back(this->parse_function());
            }

            Span end = this->expect(TokenKind::RBrace, "}").span;
            return utils::make_scope<ast::ImplExpr>(
                Span::from_span(start, end), std::move(type), std::move(body)
            );
        }
        case TokenKind::SemiColon:
            this->next();
            return nullptr;
        default:
            return this->expr();
    }

    ERROR(this->current.span, "Unreachable");
    return nullptr;
}

ast::Attributes Parser::parse_attributes() {
    ast::Attributes attrs;
    if (this->current == TokenKind::LBracket) {
        this->next();
        if (this->current == TokenKind::LBracket) {
            this->next();

            while (this->current != TokenKind::RBracket) {
                std::string name = this->expect(TokenKind::Identifier, "attribute name").value;
                if (!this->is_valid_attribute(name)) {
                    ERROR(this->current.span, "Unknown attribute '{0}'.", name);
                }

                auto& handler = this->attributes[name];
                attrs.add(handler(*this));

                if (this->current != TokenKind::Comma) {
                    break;
                }

                this->next();
            }

            this->expect(TokenKind::RBracket, "]");
            this->expect(TokenKind::RBracket, "]");
        }
    }

    return attrs;
}

utils::Scope<ast::Expr> Parser::expr(bool semicolon) {
    auto left = this->unary();
    auto expr = this->binary(0, std::move(left));
    
    if (semicolon) {
        this->end();
    }

    return expr;
}

utils::Scope<ast::Expr> Parser::binary(int prec, utils::Scope<ast::Expr> left) {
    while (true) {
        int precedence = this->get_token_precendence();
        if (precedence < prec) {
            return left;
        }

        TokenKind op = this->current.type;
        this->next();

        auto right = this->unary();

        int next = this->get_token_precendence();
        if (precedence < next) {
            right = this->binary(precedence + 1, std::move(right));
        }
       
        if (INPLACE_OPERATORS.find(op) != INPLACE_OPERATORS.end()) {
            left = utils::make_scope<ast::InplaceBinaryOpExpr>(
                Span::from_span(left->span, right->span), INPLACE_OPERATORS[op], std::move(left), std::move(right)
            );
        } else {
            left = utils::make_scope<ast::BinaryOpExpr>(
                Span::from_span(left->span, right->span), op, std::move(left), std::move(right)
            );
        }
    }
}

utils::Scope<ast::Expr> Parser::unary() {
    if (std::find(UNARY_OPERATORS.begin(), UNARY_OPERATORS.end(), this->current.type) == UNARY_OPERATORS.end()) {
        return this->call();
    }

    Span start = this->current.span;

    TokenKind op = this->current.type;
    this->next();

    auto value = this->call();
    return utils::make_scope<ast::UnaryOpExpr>(Span::from_span(start, value->span), op, std::move(value));
}

utils::Scope<ast::Expr> Parser::call() {
    Span start = this->current.span;

    auto expr = this->factor();
    while (this->current == TokenKind::LParen) {
        this->next();

        std::vector<utils::Scope<ast::Expr>> args;
        std::map<std::string, utils::Scope<ast::Expr>> kwargs;

        bool has_kwargs = false;
        while (this->current != TokenKind::RParen) {
            if (this->current == TokenKind::Identifier && this->peek() == TokenKind::Assign) {
                std::string name = this->current.value;
                this->next(); this->next();

                auto value = this->expr(false);
                kwargs[name] = std::move(value);

                has_kwargs = true;
            } else {
                if (has_kwargs) {
                    ERROR(this->current.span, "Positional arguments must come before keyword arguments");
                }

                auto value = this->expr(false);
                args.push_back(std::move(value));
            }

            if (this->current != TokenKind::Comma) {
                break;
            }

            this->next();
        }

        this->expect(TokenKind::RParen, ")");
        expr = utils::make_scope<ast::CallExpr>(
            Span::from_span(start, this->current.span), std::move(expr), std::move(args), std::move(kwargs)
        );
    }

    if (this->current == TokenKind::Inc || this->current == TokenKind::Dec) {
        TokenKind op = this->current.type;
        expr = utils::make_scope<ast::UnaryOpExpr>(Span::from_span(start, this->current.span), op, std::move(expr));

        this->next();
    } else if (
        this->current == TokenKind::LBrace && 
        this->peek() == TokenKind::Identifier && 
        this->peek(2) == TokenKind::Colon
    ) {
        this->next();
        std::vector<ast::ConstructorField> fields;

        while (this->current != TokenKind::RBrace) {
            std::string name = this->expect(TokenKind::Identifier, "field name").value;
            this->expect(TokenKind::Colon, ":");

            auto value = this->expr(false);
            fields.push_back({ name, std::move(value) });

            if (this->current != TokenKind::Comma) {
                break;
            }

            this->next();
        }

        this->expect(TokenKind::RBrace, "}");
        expr = utils::make_scope<ast::ConstructorExpr>(Span::from_span(start, this->current.span), std::move(expr), std::move(fields));
    } else if (this->current == TokenKind::Maybe) {
        this->next();
        expr = utils::make_scope<ast::MaybeExpr>(Span::from_span(start, this->current.span), std::move(expr));
    }

    if (this->current == TokenKind::Dot) {
        expr = this->attr(start, std::move(expr));
    } else if (this->current == TokenKind::LBracket) {
        expr = this->element(start, std::move(expr));
    }

    switch (this->current.type) {
        case TokenKind::As: {
            this->next();
            return utils::make_scope<ast::CastExpr>(Span::from_span(start, this->current.span), std::move(expr), this->parse_type());
        }
        case TokenKind::If: {
            this->next();
            auto condition = this->expr(false);

            if (this->current != TokenKind::Else) {
                ERROR(this->current.span, "Expected 'else' after 'if' in ternary expression");
            }

            this->next();
            auto else_expr = this->expr(false);

            return utils::make_scope<ast::TernaryExpr>(
                Span::from_span(start, this->current.span), std::move(condition), std::move(expr), std::move(else_expr)
            );
        }
        default: {
            return expr;
        }
    }
}

utils::Scope<ast::Expr> Parser::attr(Span start, utils::Scope<ast::Expr> expr) {
    while (this->current == TokenKind::Dot) {
        this->next();
        
        std::string value = this->expect(TokenKind::Identifier, "attribute name").value;
        expr = utils::make_scope<ast::AttributeExpr>(Span::from_span(start, this->current.span), value, std::move(expr));

        if (this->current == TokenKind::Maybe) {
            this->next();
            expr = utils::make_scope<ast::MaybeExpr>(Span::from_span(start, this->current.span), std::move(expr));
        }
    }

    if (this->current == TokenKind::LBracket) {
        return this->element(start, std::move(expr));
    }
    
    return expr;
}

utils::Scope<ast::Expr> Parser::element(Span start, utils::Scope<ast::Expr> expr) {
    while (this->current == TokenKind::LBracket) {
        this->next();
        auto index = this->expr(false);

        this->expect(TokenKind::RBracket, "]");
        expr = utils::make_scope<ast::ElementExpr>(Span::from_span(start, this->current.span), std::move(expr), std::move(index));

        if (this->current == TokenKind::Maybe) {
            this->next();
            expr = utils::make_scope<ast::MaybeExpr>(Span::from_span(start, this->current.span), std::move(expr));
        }
    }

    if (this->current == TokenKind::Dot) {
        return this->attr(start, std::move(expr));
    }

    return expr;
}

utils::Scope<ast::Expr> Parser::factor() {
    utils::Scope<ast::Expr> expr;

    Span start = this->current.span;
    switch (this->current.type) {
        case TokenKind::Integer: {
            std::string value = this->current.value;
            this->next();

            int bits = 32;
            bool is_float = false;

            if (NUM_TYPES_BIT_MAPPING.find(this->current.value) != NUM_TYPES_BIT_MAPPING.end()) {
                auto pair = NUM_TYPES_BIT_MAPPING[this->current.value];
                bits = pair.first; is_float = pair.second;

                this->next();
            }

            // Integers are not callable nor indexable so we can safely return from this function.
            return utils::make_scope<ast::IntegerExpr>(Span::from_span(start, this->current.span), value, bits, is_float);
        }
        case TokenKind::Char: {
            std::string value = this->current.value; this->next();
            return utils::make_scope<ast::CharExpr>(Span::from_span(start, this->current.span), value[0]);
        }
        case TokenKind::Float: {
            double result = 0.0;
            llvm::StringRef(this->current.value).getAsDouble(result);

            this->next();
            bool is_double = false;

            if (this->current.match(TokenKind::Identifier, "d")) {
                this->next();
                is_double = true;
            }

            return utils::make_scope<ast::FloatExpr>(Span::from_span(start, this->current.span), result, is_double);
        }
        case TokenKind::String: {
            std::string value = this->current.value;
            this->next();

            expr = utils::make_scope<ast::StringExpr>(Span::from_span(start, this->current.span), value);
            break;
        }
        case TokenKind::Identifier: {
            std::string name = this->current.value;
            this->next();
            if (name == "true") {
                return utils::make_scope<ast::IntegerExpr>(Span::from_span(start, this->current.span), "1", 1);
            } else if (name == "false") {
                return utils::make_scope<ast::IntegerExpr>(Span::from_span(start, this->current.span), "0", 1);
            }

            expr = utils::make_scope<ast::VariableExpr>(Span::from_span(start, this->current.span), name);
            break;
        }
        case TokenKind::Sizeof: {
            this->next();
            
            this->expect(TokenKind::LParen, "(");
            utils::Scope<ast::Expr> expr = this->expr(false);

            Span end = this->expect(TokenKind::RParen, ")").span;

            // TODO: sizeof for types
            return utils::make_scope<ast::SizeofExpr>(Span::from_span(start, end), std::move(expr));
        }
        case TokenKind::Offsetof: {
            Span start = this->current.span;
            this->next();

            this->expect(TokenKind::LParen, "(");
            auto value = this->expr(false);

            this->expect(TokenKind::Comma, ",");
            std::string field = this->expect(TokenKind::Identifier, "identifier").value;

            Span end = this->expect(TokenKind::RParen, ")").span;
            return utils::make_scope<ast::OffsetofExpr>(Span::from_span(start, end), std::move(value), field);
        }
        case TokenKind::LParen: {
            Span start = this->current.span;
            this->next();

            if (this->current == TokenKind::Identifier && this->peek() == TokenKind::Colon) {
                return this->parse_anonymous_function();
            }

            expr = this->expr(false);
            if (this->current == TokenKind::Comma) {
                std::vector<utils::Scope<ast::Expr>> elements;
                elements.push_back(std::move(expr));

                this->next();
                while (this->current != TokenKind::RParen) {
                    auto element = this->expr(false);
                    elements.push_back(std::move(element));
                    if (this->current != TokenKind::Comma) {
                        break;
                    }

                    this->next();
                }

                Span end = this->expect(TokenKind::RParen, ")").span;
                expr = utils::make_scope<ast::TupleExpr>(Span::from_span(start, end), std::move(elements));
            } else {
                this->expect(TokenKind::RParen, ")");
            }
            break;
        }
        case TokenKind::LBracket: {
            this->next();
            std::vector<utils::Scope<ast::Expr>> elements;

            if (this->current != TokenKind::RBracket) {
                auto element = this->expr(false);
                if (this->current == TokenKind::SemiColon) {
                    this->next();
                    auto size = this->expr(false);

                    this->expect(TokenKind::RBracket, "]");
                    return utils::make_scope<ast::ArrayFillExpr>(Span::from_span(start, this->current.span), std::move(element), std::move(size));
                }

                elements.push_back(std::move(element));
            }

            while (this->current != TokenKind::RBracket) {
                auto element = this->expr(false);
                elements.push_back(std::move(element));

                if (this->current != TokenKind::Comma) {
                    break;
                }

                this->next();
            }

            Span end = this->expect(TokenKind::RBracket, "]").span;
            expr = utils::make_scope<ast::ArrayExpr>(Span::from_span(start, end), std::move(elements));
            break;
        }
        case TokenKind::LBrace: {
            this->next();
            expr = this->parse_block();

            break;
        }
        default: {
            ERROR(this->current.span, "Expected an expression");
        }
    }

    while (this->current == TokenKind::DoubleColon) {
        this->next();

        std::string value = this->expect(TokenKind::Identifier, "identifier").value;
        expr = utils::make_scope<ast::NamespaceAttributeExpr>(Span::from_span(start, this->current.span), value, std::move(expr));
    }

    if (this->current == TokenKind::Dot) {
        expr = this->attr(start, std::move(expr));
    } else if (this->current == TokenKind::LBracket) {
        expr = this->element(start, std::move(expr));
    }

    return expr;
}