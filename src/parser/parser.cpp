#include "parser/parser.h"
#include "parser/ast.h"
#include "utils/utils.h"
#include "utils/log.h"

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

Parser::Parser(std::vector<Token> tokens) : tokens(tokens) {
    this->index = 0;
    this->current = this->tokens.front();

    // Populate the hash map with already defined precedences.
    for (auto pair : PRECEDENCES) {
        this->precedences[pair.first] = pair.second;
    }

    this->types = {
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

Token Parser::peek() {
    if (this->index >= this->tokens.size()) {
        return this->tokens.back();
    }

    return this->tokens[this->index + 1];
}

Token Parser::expect(TokenKind type, std::string value) {
    if (this->current.type != type) {
        ERROR(this->current.span, "Expected {0}", value);
    }

    Token token = this->current;
    this->next();

    return token;
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
    if (this->current == TokenKind::BinaryAnd) {
        is_reference = true;
        this->next();
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
        if (this->types.find(name) != this->types.end()) {
            type = utils::make_scope<ast::BuiltinTypeExpr>(
                Span::from_span(start, this->current.span), this->types[name]
            );
        } else {
            std::deque<std::string> parents;
            while (this->current == TokenKind::DoubleColon) {
                this->next();
                parents.push_back(this->expect(TokenKind::Identifier, "identifier").value);
            }

            if (!parents.empty()) {
                std::string back = parents.back();
                parents.pop_back();

                parents.push_front(name);
                name = back;
            }

            type = utils::make_scope<ast::NamedTypeExpr>(Span::from_span(start, this->current.span), name, parents);
        }
    }

    while (this->current == TokenKind::Mul) {
        Span end = this->next().span;
        type = utils::make_scope<ast::PointerTypeExpr>(Span::from_span(start, end), std::move(type));
    }

    if (is_reference) {
        type = utils::make_scope<ast::ReferenceTypeExpr>(Span::from_span(start, this->current.span), std::move(type));
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

    return utils::make_scope<ast::TypeAliasExpr>(Span::from_span(start, end), name, std::move(type));
}

std::pair<std::vector<ast::Argument>, bool> Parser::parse_arguments() {
    std::vector<ast::Argument> args;

    bool has_kwargs = false;
    bool has_default_values = false;
    bool is_variadic = false;

    while (this->current != TokenKind::RParen) {
        std::string argument = this->current.value;
        if (!this->current.match(
            {TokenKind::Immutable, TokenKind::Identifier, TokenKind::Mul, TokenKind::Ellipsis}
        )) {
            ERROR(this->current.span, "Expected argument name, '*' or '...'");
        }

        bool is_immutable = false;
        if (this->current == TokenKind::Ellipsis) {
            if (is_variadic) {
                ERROR(this->current.span, "Cannot have multiple variadic arguments");
            }

            is_variadic = true;
            this->next();

            break;
        } else if (this->current == TokenKind::Mul) {
            if (has_kwargs) {
                ERROR(this->current.span, "Only one '*' seperator is allowed in a function prototype");
            }

            has_kwargs = true;
            this->next();

            this->expect(TokenKind::Comma, ",");
            continue;
        } else if (this->current == TokenKind::Immutable) {
            this->next();
            is_immutable = true;

            if (this->current != TokenKind::Identifier) {
                ERROR(this->current.span, "Expected identifier");
            }

            argument = this->current.value;
        }

        utils::Scope<ast::TypeExpr> type = nullptr;
        bool is_self = false;

        if (argument == "self" && this->is_inside_struct && !has_kwargs) {
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
        
        args.push_back({
            argument, 
            std::move(type),
            std::move(default_value),
            is_self, 
            has_kwargs, 
            is_immutable
        });

        if (this->current != TokenKind::Comma) {
            break;
        }

        this->next();
    }

    this->expect(TokenKind::RParen, ")");
    return {std::move(args), is_variadic};
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

utils::Scope<ast::IfExpr> Parser::parse_if_statement() {
    Span start = this->current.span;
    this->expect(TokenKind::LParen, "(");

    auto condition = this->expr(false);
    this->expect(TokenKind::RParen, ")");

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
    std::string name = this->expect(TokenKind::Identifier, "struct name").value;

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

                if (is_private) {
                    definition->attributes.add("private");
                }

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

    Span end = this->expect(TokenKind::RBrace, "}").span;
    this->is_inside_struct = false;

    return utils::make_scope<ast::StructExpr>(
        Span::from_span(start, end), name, false, std::move(parents), std::move(fields), std::move(methods)
    );    
}

utils::Scope<ast::Expr> Parser::parse_variable_definition(bool is_const) {
    Span start = this->current.span;
    utils::Scope<ast::TypeExpr> type = nullptr;

    std::vector<std::string> names;
    bool is_multiple_variables = false;
    bool has_consume_rest = false;
    bool is_immutable = false;

    if (!is_const && this->current == TokenKind::Immutable) {
        is_immutable = true;
        this->next();
    }

    std::string consume_rest;
    if (this->current == TokenKind::LParen) {
        this->next();

        while (this->current != TokenKind::RParen) {
            if (this->current == TokenKind::Mul) {
                if (has_consume_rest) {
                    ERROR(this->current.span, "Cannot consume rest of the arguments more than once");
                }

                this->next();
                has_consume_rest = true;

                consume_rest = this->expect(TokenKind::Identifier, "consume rest name").value;
                names.push_back(consume_rest);
            } else {
                names.push_back(this->expect(TokenKind::Identifier, "variable name").value);
            }

            if (this->current != TokenKind::Comma) {
                break;
            }

            this->next();
        }

        this->expect(TokenKind::RParen, ")");
        if (names.empty()) {
            ERROR(start, "Expected atleast a single variable name");
        }

        is_multiple_variables = true;
    } else {
        std::string name = this->expect(TokenKind::Identifier, "variable name").value;
        names.push_back(name);
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

        return utils::make_scope<ast::ConstExpr>(Span::from_span(start, end), names[0], std::move(type), std::move(expr));
    } else {
        return utils::make_scope<ast::VariableAssignmentExpr>(
            Span::from_span(start, end), names, std::move(type), std::move(expr), consume_rest, false, is_multiple_variables, is_immutable
        );
    }
}

utils::Scope<ast::NamespaceExpr> Parser::parse_namespace() {
    Span start = this->current.span;

    std::string name = this->expect(TokenKind::Identifier, "namespace name").value;
    std::deque<std::string> parents;

    while (this->current == TokenKind::DoubleColon) {
        this->next();
        std::string parent = this->expect(TokenKind::Identifier, "namespace name").value;

        parents.push_back(parent);
    }

    if (!parents.empty()) {
        std::string back = parents.back();
        parents.pop_back();

        parents.push_back(name);
        name = back;
    }

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


        if (this->current.value == "func") {
            this->next();
            member = this->parse_function_definition();
        } else if (this->current.value == "extern") {
            this->next();
            member = this->parse_extern_block();
        } else if (this->current.value == "struct") {
            this->next();
            member = this->parse_struct();
        } else if (this->current.value == "namespace") {
            this->next();
            member = this->parse_namespace();
        } else if (this->current.value == "type") {
            this->next();
            member = this->parse_type_alias();
        }

        member->attributes.update(attrs);
        members.push_back(std::move(member));
    }

    Span end = this->expect(TokenKind::RBrace, "}").span;
    return utils::make_scope<ast::NamespaceExpr>(Span::from_span(start, end), name, parents, std::move(members));
}

utils::Scope<ast::Expr> Parser::parse_extern(ast::ExternLinkageSpecifier linkage) {
    utils::Scope<ast::Expr> definition;
    Span start = this->current.span;
    
    ast::Attributes attrs = this->parse_attributes();
    if (this->current == TokenKind::Identifier) {
        std::string name = this->current.value;
        std::string consume_rest;

        this->next();

        this->expect(TokenKind::Colon, ":");
        auto type = this->parse_type();

        utils::Scope<ast::Expr> expr;
        if (this->current == TokenKind::Assign) {
            expr = this->expr(false);
        }

        Span end = this->expect(TokenKind::SemiColon, ";").span;

        std::vector<std::string> names = {name,};
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

            this->next();
            Span start = this->current.span;
            if (this->current == TokenKind::SemiColon) {
                this->next();
                return utils::make_scope<ast::ReturnExpr>(Span::from_span(start, this->current.span), nullptr);
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
            this->expect(TokenKind::LParen, "(");
            
            auto condition = this->expr(false);
            this->expect(TokenKind::RParen, ")");

            this->expect(TokenKind::LBrace, "{");

            this->is_inside_loop = true;
            auto body = this->parse_block();

            this->is_inside_loop = has_outer_loop;
            return utils::make_scope<ast::WhileExpr>(Span::from_span(start, body->span), std::move(condition), std::move(body));
        } 
        case TokenKind::For: {
            bool has_outer_loop = this->is_inside_loop;
            Span start_ = this->current.span;

            this->next();
            this->expect(TokenKind::LParen, "(");

            auto start = this->parse_variable_definition(false);

            auto end = this->expr(false);
            this->expect(TokenKind::SemiColon, ";");

            auto step = this->expr(false);
            this->expect(TokenKind::RParen, ")");

            this->expect(TokenKind::LBrace, "{");

            this->is_inside_loop = true;
            auto body = this->parse_block();

            this->is_inside_loop = has_outer_loop;
            return utils::make_scope<ast::ForExpr>(
                Span::from_span(start_, body->span), std::move(start), std::move(end), std::move(step), std::move(body)
            );
        } 
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
        case TokenKind::Foreach: {
            Span start = this->current.span;
            this->next(); this->expect(TokenKind::LParen, "(");

            // TODO: foreach ((x, y) in foo)
            auto variable = this->expect(TokenKind::Identifier, "identifier").value;
            this->expect(TokenKind::In, "in");

            bool outer = this->is_inside_loop;
            this->is_inside_loop = true;

            auto expr = this->expr(false);

            this->expect(TokenKind::RParen, ")");
            this->expect(TokenKind::LBrace, "{");

            auto body = this->parse_block();
            this->is_inside_loop = outer;

            return utils::make_scope<ast::ForeachExpr>(
                Span::from_span(start, body->span), variable, std::move(expr), std::move(body)
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

                if (this->current == TokenKind::LParen) {
                    this->next();

                    std::string value;
                    ast::AttributeValueType type = ast::AttributeValueType::String;

                    switch (this->current.type) {
                        case TokenKind::String:
                            value = this->current.value; break;
                        case TokenKind::Integer:
                            value = this->current.value;
                            type = ast::AttributeValueType::Integer;
                            break;
                        default:
                            ERROR(this->current.span, "Expected a string or integer.");
                    }

                    this->next();
                    attrs.add(name, type, value);

                    this->expect(TokenKind::RParen, ")");
                } else {
                    attrs.add(name);
                }

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
        Span start = this->current.span;
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
                Span::from_span(start, this->current.span), INPLACE_OPERATORS[op], std::move(left), std::move(right)
            );
        } else {
            left = utils::make_scope<ast::BinaryOpExpr>(
                Span::from_span(start, this->current.span), op, std::move(left), std::move(right)
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
    return utils::make_scope<ast::UnaryOpExpr>(Span::from_span(start, this->current.span), op, std::move(value));
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
    } else if (this->current == TokenKind::LBrace) {
        this->next();
        std::vector<ast::ConstructorField> fields;

        bool previous_is_named_field = true;
        while (this->current != TokenKind::RBrace) {
            std::string name;
            if (this->current == TokenKind::Identifier && this->peek() == TokenKind::Colon) {
                if (!previous_is_named_field) {
                    ERROR(this->current.span, "Expected a field name.");
                }

                previous_is_named_field = true;
                name = this->current.value;

                this->next(); this->next();
            } else {
                previous_is_named_field = false;
            }

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