#include <quart/parser/parser.h>
#include <quart/parser/attrs.h>
#include <quart/parser/ast.h>
#include <quart/utils/utils.h>
#include <quart/logging.h>

#include <llvm/ADT/StringRef.h>

#include <memory>
#include <string>
#include <cstring>
#include <vector>

using namespace quart;

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
    {"u8", ast::BuiltinType::u8},
    {"u16", ast::BuiltinType::u16},
    {"u32", ast::BuiltinType::u32},
    {"u64", ast::BuiltinType::u64},
    {"u128", ast::BuiltinType::u128},
    {"f32", ast::BuiltinType::f32},
    {"f64", ast::BuiltinType::f64}
};


Parser::Parser(std::vector<Token> tokens) : tokens(std::move(tokens)) {
    this->index = 0;
    this->current = this->tokens.front();

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
        this->current = this->tokens.back(); // EOF
    } else {
        this->current = this->tokens[this->index];
    }

    return this->current;
}

Token Parser::peek(uint32_t offset) {
    if (this->index >= this->tokens.size()) {
        return this->tokens.back(); // EOF
    }

    return this->tokens[this->index + offset];
}

Token Parser::expect(TokenKind type, const std::string& value) {
    if (this->current.type != type) {
        ERROR(this->current.span, "Expected {0}", value);
    }

    Token token = this->current;
    this->next();

    return token;
}

bool Parser::is_valid_attribute(const std::string& name) {
    return this->attributes.find(name) != this->attributes.end();
}

int Parser::get_token_precendence() {
    auto it = PRECEDENCES.find(this->current.type);
    if (it == PRECEDENCES.end()) {
        return -1;
    }

    return it->second;
}

AttributeHandler::Result Parser::handle_expr_attributes(const ast::Attributes& attrs) {
    for (auto& entry : attrs.values) {
        auto result = AttributeHandler::handle(*this, entry.second);
        if (result != AttributeHandler::Ok) {
            return result;
        }
    }

    return AttributeHandler::Ok;
}

std::unique_ptr<ast::TypeExpr> Parser::parse_type() {
    Span start = this->current.span;

    switch (this->current.type) {
        case TokenKind::BinaryAnd: {
            this->next();
            bool is_mutable = false;

            if (this->current == TokenKind::Mut) {
                is_mutable = true;
                this->next();
            }

            return std::make_unique<ast::ReferenceTypeExpr>(
                this->current.span, this->parse_type(), is_mutable
            );
        }
        case TokenKind::Mul: {
            this->next();
            bool is_mutable = false;

            if (this->current == TokenKind::Mut) {
                is_mutable = true;
                this->next();
            }

            auto type = this->parse_type();
            if (type->kind == ast::TypeKind::Reference) {
                ERROR(type->span, "Cannot have a pointer to a reference");
            }

            return std::make_unique<ast::PointerTypeExpr>(
                this->current.span, std::move(type), is_mutable
            );
        }
        case TokenKind::LParen: {
            this->next();
            if (this->current == TokenKind::RParen) {
                ERROR(this->current.span, "Tuple type literals must atleast have a single element");
            }

            std::vector<std::unique_ptr<ast::TypeExpr>> types;
            while (this->current != TokenKind::RParen) {
                types.push_back(this->parse_type());
                if (this->current != TokenKind::Comma) {
                    break;
                }

                this->next();
            }

            Span end = this->expect(TokenKind::RParen, ")").span;
            return std::make_unique<ast::TupleTypeExpr>(Span::merge(start, end), std::move(types));
        }
        case TokenKind::LBracket: {
            this->next();
            auto type = this->parse_type();

            this->expect(TokenKind::SemiColon, ";");
            auto size = this->expr(false);

            Span end = this->expect(TokenKind::RBracket, "]").span;
            return std::make_unique<ast::ArrayTypeExpr>(Span::merge(start, end), std::move(type), std::move(size));
        }
        case TokenKind::Identifier: {
            std::string name = this->current.value;
            this->next();

            if (name == "int") {
                this->expect(TokenKind::LParen, "(");
                auto size = this->expr(false);

                Span end = this->expect(TokenKind::RParen, ")").span;
                return std::make_unique<ast::IntegerTypeExpr>(
                    Span::merge(start, end), std::move(size)
                );
            }

            if (Parser::TYPES.find(name) != Parser::TYPES.end()) {
                return std::make_unique<ast::BuiltinTypeExpr>(
                    Span::merge(start, this->current.span), Parser::TYPES.at(name)
                );
            }

            Path p = this->parse_path(name);
            return std::make_unique<ast::NamedTypeExpr>(
                Span::merge(start, this->current.span), p.name, p.segments
            );
        }
        case TokenKind::Func: {
            this->next(); this->expect(TokenKind::LParen, "(");

            std::vector<std::unique_ptr<ast::TypeExpr>> args;
            while (this->current != TokenKind::RParen) {
                args.push_back(this->parse_type());
                if (this->current != TokenKind::Comma) {
                    break;
                }
                
                this->next();
            }

            Span end = this->expect(TokenKind::RParen, ")").span;
            std::unique_ptr<ast::TypeExpr> ret = nullptr;

            if (this->current == TokenKind::Arrow) {
                end = this->next().span; ret = this->parse_type();
            }

            return std::make_unique<ast::FunctionTypeExpr>(
                Span::merge(start, end), std::move(args), std::move(ret)
            );
        }
        default: ERROR(this->current.span, "Expected type");
    }
}

std::unique_ptr<ast::BlockExpr> Parser::parse_block() {
    std::vector<std::unique_ptr<ast::Expr>> body;
    Span start = this->current.span;

    while (this->current != TokenKind::RBrace) {
        ast::Attributes attrs = this->parse_attributes();
        auto expr = this->statement();

        if (expr) {
            if (this->handle_expr_attributes(attrs) != AttributeHandler::Ok) {
                continue;
            }

            expr->attributes.update(attrs);
            body.push_back(std::move(expr));
        }
    }

    Span end = this->expect(TokenKind::RBrace, "}").span;
    return std::make_unique<ast::BlockExpr>(Span::merge(start, end), std::move(body));
}

std::unique_ptr<ast::TypeAliasExpr> Parser::parse_type_alias() {
    Span start = this->current.span;

    std::string name = this->expect(TokenKind::Identifier, "identifier").value;
    this->expect(TokenKind::Assign, "=");

    auto type = this->parse_type();
    Span end = this->expect(TokenKind::SemiColon, ";").span;

    return std::make_unique<ast::TypeAliasExpr>(
        Span::merge(start, end), name, std::move(type)
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

        bool is_mutable = false;
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
            is_mutable = true;

            if (this->current != TokenKind::Identifier) {
                ERROR(this->current.span, "Expected identifier");
            }

            argument = this->current.value;
            span = Span::merge(span, this->current.span);
        }

        std::unique_ptr<ast::TypeExpr> type = nullptr;
        bool is_self = false;

        if (argument == "self" && (this->is_inside_struct || this->self_usage_allowed) && !has_kwargs) {
            is_self = true; this->next();
        } else {
            this->next(); this->expect(TokenKind::Colon, ":");
            type = this->parse_type();
        }

        std::unique_ptr<ast::Expr> default_value = nullptr;
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
            is_mutable,
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

std::unique_ptr<ast::PrototypeExpr> Parser::parse_prototype(
    ast::ExternLinkageSpecifier linkage, bool with_name, bool is_operator
) {
    Span span = this->current.span;
    std::string name;

    if (with_name) {
        name = this->expect(TokenKind::Identifier, "function name").value;
        this->expect(TokenKind::LParen, "(");
    }

    auto pair = this->parse_arguments();

    Span end = this->current.span;
    std::unique_ptr<ast::TypeExpr> ret = nullptr;

    if (this->current == TokenKind::Arrow) {
        end = this->next().span; ret = this->parse_type();
    }

    return std::make_unique<ast::PrototypeExpr>(
        span, name, std::move(pair.first), std::move(ret), pair.second, is_operator, linkage
    );
}

std::unique_ptr<ast::Expr> Parser::parse_function_definition(
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

    std::vector<std::unique_ptr<ast::Expr>> body;
    while (!this->current.match({TokenKind::RBrace, TokenKind::EOS})) {
        ast::Attributes attrs = this->parse_attributes();
        auto expr = this->statement();

        if (expr) {
            if (this->handle_expr_attributes(attrs) != AttributeHandler::Ok) {
                continue;
            }

            expr->attributes.update(attrs);
            body.push_back(std::move(expr));
        }
    }

    auto end = this->expect(TokenKind::RBrace, "}").span;
    this->is_inside_function = false;

    return std::make_unique<ast::FunctionExpr>(
        Span::merge(start, end), std::move(prototype), std::move(body)
    );
}

std::unique_ptr<ast::FunctionExpr> Parser::parse_function() {
    Span start = this->current.span;
    auto prototype = this->parse_prototype(ast::ExternLinkageSpecifier::Unspecified, true, false);
    
    this->expect(TokenKind::LBrace, "{");
    this->is_inside_function = true;

    std::vector<std::unique_ptr<ast::Expr>> body;
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

    return std::make_unique<ast::FunctionExpr>(
        Span::merge(start, end), std::move(prototype), std::move(body)
    );
}

std::unique_ptr<ast::IfExpr> Parser::parse_if_statement() {
    Span start = this->current.span;
    auto condition = this->expr(false);

    std::unique_ptr<ast::Expr> body;
    if (this->current != TokenKind::LBrace) {
        body = this->statement();
    } else {
        this->next();
        body = this->parse_block();
    }

    Span end = body->span;
    std::unique_ptr<ast::Expr> else_body;
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

    return std::make_unique<ast::IfExpr>(
        Span::merge(start, end), std::move(condition), std::move(body), std::move(else_body)
    );
}

std::unique_ptr<ast::StructExpr> Parser::parse_struct() {
    Span start = this->current.span;
    Token token = this->expect(TokenKind::Identifier, "struct name");
    
    std::string name = token.value;
    Span end = token.span;

    std::vector<ast::StructField> fields;
    std::vector<std::unique_ptr<ast::Expr>> methods;
    std::vector<std::unique_ptr<ast::Expr>> parents;

    if (this->current == TokenKind::SemiColon) {
        this->next();
        return std::make_unique<ast::StructExpr>(
            Span::merge(start, this->current.span), name, true, std::move(parents), std::move(fields), std::move(methods)
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

    return std::make_unique<ast::StructExpr>(
        Span::merge(start, end), name, false, std::move(parents), std::move(fields), std::move(methods)
    );    
}

std::unique_ptr<ast::Expr> Parser::parse_variable_definition(bool is_const) {
    Span span = this->current.span;

    std::unique_ptr<ast::TypeExpr> type = nullptr;
    std::vector<ast::Ident> names;

    bool is_multiple_variables = false;
    bool has_consume_rest = false;
    bool is_mutable = false;

    if (is_const && this->current == TokenKind::Mut) {
        ERROR(this->current.span, "Cannot use 'mut' with 'const'");
    } else if (!is_const && this->current == TokenKind::Mut) {
        is_mutable = true;
        this->next();
    }

    std::string consume_rest;
    if (this->current == TokenKind::LParen) {
        this->next();

        bool mut_prefixed = is_mutable;
        while (this->current != TokenKind::RParen) {
            if (mut_prefixed && this->current == TokenKind::Mut) {
                NOTE(this->current.span, "Redundant 'mut'");
            }

            if (mut_prefixed) {
                is_mutable = true;
            }
            
            if (this->current == TokenKind::Mut) {
                is_mutable = true;
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
            names.push_back({token.value, is_mutable, token.span});

            if (this->current != TokenKind::Comma) {
                break;
            }

            this->next();
            is_mutable = false;
        }

        this->expect(TokenKind::RParen, ")");
        if (names.empty()) {
            ERROR(span, "Expected atleast a single variable name");
        }

        is_multiple_variables = true;
    } else {
        Token token = this->expect(TokenKind::Identifier, "variable name");
        names.push_back({token.value, is_mutable, Span::merge(span, token.span)});
    }

    if (this->current == TokenKind::Colon) {
        this->next(); type = this->parse_type();
    }
    
    std::unique_ptr<ast::Expr> expr = nullptr;
    Span end;

    if (this->current != TokenKind::Assign) {
        end = this->expect(TokenKind::SemiColon, ";").span;

        if (!type) {
            ERROR(end, "Un-initialized variables must have an inferred type");
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

        return std::make_unique<ast::ConstExpr>(span, names[0].value, std::move(type), std::move(expr));
    } else {
        return std::make_unique<ast::VariableAssignmentExpr>(
            span, names, std::move(type), std::move(expr), consume_rest, false, is_multiple_variables
        );
    }
}

std::unique_ptr<ast::Expr> Parser::parse_extern(ast::ExternLinkageSpecifier linkage) {
    std::unique_ptr<ast::Expr> definition;
    Span start = this->current.span;
    
    ast::Attributes attrs = this->parse_attributes();
    if (this->current == TokenKind::Identifier) {
        std::string name = this->current.value;
        Span span = this->current.span;

        std::string consume_rest;

        this->next();

        this->expect(TokenKind::Colon, ":");
        auto type = this->parse_type();

        std::unique_ptr<ast::Expr> expr;
        if (this->current == TokenKind::Assign) {
            expr = this->expr(false);
        }

        Span end = this->expect(TokenKind::SemiColon, ";").span;

        std::vector<ast::Ident> names = {{name, true, span},};
        definition = std::make_unique<ast::VariableAssignmentExpr>(
            Span::merge(start, end), names, std::move(type), std::move(expr), consume_rest, true, false
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

std::unique_ptr<ast::Expr> Parser::parse_extern_block() {
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
        std::vector<std::unique_ptr<ast::Expr>> definitions;
        this->next();

        while (this->current != TokenKind::RBrace) {
            auto def = this->parse_extern(linkage);
            if (this->handle_expr_attributes(def->attributes) != AttributeHandler::Ok) {
                continue;
            }

            definitions.push_back(std::move(def));
        }

        Span end = this->expect(TokenKind::RBrace, "}").span;
        return std::make_unique<ast::ExternBlockExpr>(Span::merge(start, end), std::move(definitions));
    }

    return this->parse_extern(linkage);
}

std::unique_ptr<ast::EnumExpr> Parser::parse_enum() {
    Span start = this->current.span;
    std::string name = this->expect(TokenKind::Identifier, "enum name").value;

    std::unique_ptr<ast::TypeExpr> type;
    if (this->current == TokenKind::Colon) {
        this->next(); type = this->parse_type();
    }

    this->expect(TokenKind::LBrace, "{");

    std::vector<ast::EnumField> fields;
    while (this->current != TokenKind::RBrace) {
        std::string field = this->expect(TokenKind::Identifier, "enum field name").value;
        std::unique_ptr<ast::Expr> value = nullptr;

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
    return std::make_unique<ast::EnumExpr>(Span::merge(start, end), name, std::move(type), std::move(fields));

}

std::unique_ptr<ast::Expr> Parser::parse_anonymous_function() {
    auto pair = this->parse_arguments();

    std::unique_ptr<ast::TypeExpr> ret = nullptr;
    if (this->current == TokenKind::Colon) {
        this->next();
        ret = this->parse_type();
    }

    this->expect(TokenKind::DoubleArrow, "=>");

    std::vector<std::unique_ptr<ast::Expr>> body;
    body.push_back(this->expr(false));
    
    auto prototype = std::make_unique<ast::PrototypeExpr>(
        this->current.span,
        "",
        std::move(pair.first), 
        std::move(ret), 
        std::move(pair.second),
        false,
        ast::ExternLinkageSpecifier::None
    );
    
    return std::make_unique<ast::FunctionExpr>(
        Span::merge(prototype->span, this->current.span), std::move(prototype), std::move(body)
    );
}

std::unique_ptr<ast::MatchExpr> Parser::parse_match_expr() {
    auto value = this->expr(false);
    this->expect(TokenKind::LBrace, "{");

    std::vector<ast::MatchArm> arms;
    bool has_wildcard = false;

    size_t i = 0;
    while (this->current != TokenKind::RBrace) {
        ast::MatchPattern pattern;
        pattern.is_wildcard = false;

        if (this->current == TokenKind::Mul) {
            if (has_wildcard) {
                ERROR(this->current.span, "Cannot have multiple wildcard match arms");
            }

            pattern.is_wildcard = true;
            pattern.span = this->current.span;

            has_wildcard = true;

            this->next();
        } else if (this->current == TokenKind::If) {
            this->next();

            auto expr = this->expr(false);
            Span span = expr->span;

            pattern.values.push_back(std::move(expr));
            pattern.is_conditional = true;

            pattern.span = span;
        } else {
            auto expr = this->expr(false);
            Span span = expr->span;

            pattern.values.push_back(std::move(expr));
            while (this->current != TokenKind::DoubleArrow) {
                expr = this->expr(false);
                span = Span::merge(span, expr->span);

                pattern.values.push_back(std::move(expr));
                if (this->current != TokenKind::BinaryOr) {
                    break;
                }

                this->next();
            }

            pattern.span = span;
        }

        this->expect(TokenKind::DoubleArrow, "=>");
        auto body = this->expr(false);

        arms.push_back({std::move(pattern), std::move(body), i});
        if (this->current != TokenKind::Comma) {
            break;
        }

        i++;
        this->next();
    }

    this->expect(TokenKind::RBrace, "}");
    return std::make_unique<ast::MatchExpr>(value->span, std::move(value), std::move(arms));
}

Path Parser::parse_path(llvm::Optional<std::string> name) {
    Path p = {
        .name = name ? *name : this->expect(TokenKind::Identifier, "identifier").value,
        .segments = {},
    };

    while (this->current == TokenKind::DoubleColon) {
        this->next();
        p.segments.push_back(this->expect(TokenKind::Identifier, "identifier").value);
    }

    if (!p.segments.empty()) {
        std::string back = p.segments.back();
        p.segments.pop_back();

        p.segments.push_back(p.name);
        p.name = back;
    }

    return p;
}

std::vector<std::unique_ptr<ast::Expr>> Parser::parse() {
    return this->statements();
}

std::vector<std::unique_ptr<ast::Expr>> Parser::statements() {
    std::vector<std::unique_ptr<ast::Expr>> statements;

    while (this->current != TokenKind::EOS) {
        ast::Attributes attrs = this->parse_attributes();
        auto expr = this->statement();

        if (expr) {
            if (this->handle_expr_attributes(attrs) != AttributeHandler::Ok) {
                continue;
            }

            expr->attributes.update(attrs);
            statements.push_back(std::move(expr));
        }
    }

    return statements;
}

std::unique_ptr<ast::Expr> Parser::statement() {
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
                return std::make_unique<ast::ReturnExpr>(Span::merge(start, end), nullptr);
            }

            auto expr = this->expr(false);

            Span end = this->expect(TokenKind::SemiColon, ";").span;
            return std::make_unique<ast::ReturnExpr>(Span::merge(start, end), std::move(expr));
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
            return std::make_unique<ast::WhileExpr>(Span::merge(start, body->span), std::move(condition), std::move(body));
        }
        case TokenKind::Break: {
            if (!this->is_inside_loop) {
                ERROR(this->current.span, "Break statement outside of loop");
            }

            Span start = this->current.span;
            this->next();

            Span end = this->expect(TokenKind::SemiColon, ";").span;
            return std::make_unique<ast::BreakExpr>(Span::merge(start, end));
        } 
        case TokenKind::Continue: {
            if (!this->is_inside_loop) {
                ERROR(this->current.span, "Continue statement outside of loop");
            }

            Span start = this->current.span;
            this->next();

            Span end = this->expect(TokenKind::SemiColon, ";").span;
            return std::make_unique<ast::ContinueExpr>(Span::merge(start, end));
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

            return std::make_unique<ast::UsingExpr>(Span::merge(start, this->current.span), members, std::move(parent));
        } 
        case TokenKind::Defer: {
            if (!this->is_inside_function) {
                ERROR(this->current.span, "Defer statement outside of function");
            }

            Span start = this->current.span;
            this->next();

            auto expr = this->expr();
            return std::make_unique<ast::DeferExpr>(Span::merge(start, this->current.span), std::move(expr));
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
            return std::make_unique<ast::ImportExpr>(Span::merge(start, end), name, is_wildcard, is_relative);
        }
        case TokenKind::Module: {
            this->next();
            std::string name = this->expect(TokenKind::Identifier, "module name").value;

            Span start = this->expect(TokenKind::LBrace, "{").span;
            std::vector<std::unique_ptr<ast::Expr>> body;

            while (this->current != TokenKind::RBrace) {
                body.push_back(this->statement());
            }

            Span end = this->expect(TokenKind::RBrace, "}").span;
            return std::make_unique<ast::ModuleExpr>(
                Span::merge(start, end), name, std::move(body)
            );
        }
        case TokenKind::For: {
            Span start = this->current.span;
            this->next();

            bool is_mutable = false;
            if (this->current == TokenKind::Mut) {
                is_mutable = true;
                this->next();
            }

            Token token = this->expect(TokenKind::Identifier, "identifier");
            ast::Ident ident = { token.value, is_mutable, token.span };

            this->expect(TokenKind::In, "in");

            bool outer = this->is_inside_loop;
            this->is_inside_loop = true;

            auto expr = this->expr(false);

            if (this->current == TokenKind::DoubleDot) {
                this->next();

                std::unique_ptr<ast::Expr> end = nullptr;
                if (this->current != TokenKind::LBrace) {
                    end = this->expr(false);
                }
                
                this->expect(TokenKind::LBrace, "{");

                auto body = this->parse_block();
                this->is_inside_loop = outer;

                return std::make_unique<ast::RangeForExpr>(
                    Span::merge(start, body->span), ident, std::move(expr), std::move(end), std::move(body)
                );
            }

            this->expect(TokenKind::LBrace, "{");

            auto body = this->parse_block();
            this->is_inside_loop = outer;

            return std::make_unique<ast::ForExpr>(
                Span::merge(start, body->span), ident, std::move(expr), std::move(body)
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
                
            return std::make_unique<ast::StaticAssertExpr>(Span::merge(start, end), std::move(expr), message);
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
            return std::make_unique<ast::ImplExpr>(
                Span::merge(start, end), std::move(type), std::move(body)
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
    if (this->current == TokenKind::Not && this->peek() == TokenKind::LBracket) {
        this->next(); this->next();
        
        while (this->current != TokenKind::RBracket) {
            std::string name = this->expect(TokenKind::Identifier, "attribute name").value;
            if (!this->is_valid_attribute(name)) {
                ERROR(this->current.span, "Unknown attribute '{0}'", name);
            }

            auto& handler = this->attributes.at(name);
            attrs.add(handler(*this));

            if (this->current != TokenKind::Comma) {
                break;
            }

            this->next();
        }

        this->expect(TokenKind::RBracket, "]");
    }

    return attrs;
}

std::unique_ptr<ast::Expr> Parser::expr(bool semicolon) {
    auto left = this->unary();
    auto expr = this->binary(0, std::move(left));
    
    if (semicolon) {
        this->end();
    }

    return expr;
}

std::unique_ptr<ast::Expr> Parser::binary(int prec, std::unique_ptr<ast::Expr> left) {
    while (true) {
        int precedence = this->get_token_precendence();
        if (precedence < prec) {
            return left;
        }

        TokenKind kind = this->current.type;
        BinaryOp op = BINARY_OPS[kind];

        this->next();
        auto right = this->unary();

        int next = this->get_token_precendence();
        if (precedence < next) {
            right = this->binary(precedence + 1, std::move(right));
        }
       
        if (INPLACE_OPERATORS.find(kind) != INPLACE_OPERATORS.end()) {
            left = std::make_unique<ast::InplaceBinaryOpExpr>(
                Span::merge(left->span, right->span), op, std::move(left), std::move(right)
            );
        } else {
            left = std::make_unique<ast::BinaryOpExpr>(
                Span::merge(left->span, right->span), op, std::move(left), std::move(right)
            );
        }
    }
}

std::unique_ptr<ast::Expr> Parser::unary() {
    auto iterator = UNARY_OPS.find(this->current.type);
    std::unique_ptr<ast::Expr> expr;

    if (iterator == UNARY_OPS.end()) {
        expr = this->call();
    } else {
        Span start = this->current.span;

        UnaryOp op = iterator->second;
        this->next();

        auto value = this->call();
        expr = std::make_unique<ast::UnaryOpExpr>(Span::merge(start, value->span), op, std::move(value));
    }

    switch (this->current.type) {
        case TokenKind::As: {
            this->next();
            auto type = this->parse_type();

            Span span = Span::merge(expr->span, type->span);
            return std::make_unique<ast::CastExpr>(span, std::move(expr), std::move(type));
        }
        case TokenKind::If: {
            this->next();
            auto condition = this->expr(false);

            if (this->current != TokenKind::Else) {
                ERROR(this->current.span, "Expected 'else' after 'if' in ternary expression");
            }

            this->next();
            auto else_expr = this->expr(false);

            return std::make_unique<ast::TernaryExpr>(
                Span::merge(expr->span, else_expr->span), std::move(condition), std::move(expr), std::move(else_expr)
            );
        }
        default: return expr;
    }
}

std::unique_ptr<ast::Expr> Parser::call() {
    Span start = this->current.span;

    auto expr = this->factor();
    while (this->current == TokenKind::LParen) {
        this->next();

        std::vector<std::unique_ptr<ast::Expr>> args;
        std::map<std::string, std::unique_ptr<ast::Expr>> kwargs;

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
        expr = std::make_unique<ast::CallExpr>(
            Span::merge(start, this->current.span), std::move(expr), std::move(args), std::move(kwargs)
        );
    }

    if (this->current == TokenKind::Inc || this->current == TokenKind::Dec) {
        UnaryOp op = this->current.type == TokenKind::Inc ? UnaryOp::Inc : UnaryOp::Dec;
        expr = std::make_unique<ast::UnaryOpExpr>(Span::merge(start, this->current.span), op, std::move(expr));

        this->next();
    } else if ( // Basically this condition checks for Foo{bar: ...} or Foo{}
        this->current == TokenKind::LBrace && 
        ((this->peek() == TokenKind::Identifier && this->peek(2) == TokenKind::Colon) || this->peek() == TokenKind::RBrace) &&
        (expr->kind() == ast::ExprKind::Variable || expr->kind() == ast::ExprKind::Path)
    ) {
        this->next();
        std::vector<ast::ConstructorField> fields;

        if (this->current == TokenKind::RBrace) {
            this->next();
            return std::make_unique<ast::EmptyConstructorExpr>(
                Span::merge(start, this->current.span), std::move(expr)
            );
        }

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
        expr = std::make_unique<ast::ConstructorExpr>(Span::merge(start, this->current.span), std::move(expr), std::move(fields));
    }

    if (this->current == TokenKind::Dot) {
        expr = this->attr(start, std::move(expr));
    } else if (this->current == TokenKind::LBracket) {
        expr = this->element(start, std::move(expr));
    }

    return expr;
}

std::unique_ptr<ast::Expr> Parser::attr(Span start, std::unique_ptr<ast::Expr> expr) {
    while (this->current == TokenKind::Dot) {
        this->next();
        
        std::string value = this->expect(TokenKind::Identifier, "attribute name").value;
        expr = std::make_unique<ast::AttributeExpr>(
            Span::merge(start, this->current.span), value, std::move(expr)
        );
    }

    if (this->current == TokenKind::LBracket) {
        return this->element(start, std::move(expr));
    }
    
    return expr;
}

std::unique_ptr<ast::Expr> Parser::element(Span start, std::unique_ptr<ast::Expr> expr) {
    while (this->current == TokenKind::LBracket) {
        this->next();
        auto index = this->expr(false);

        this->expect(TokenKind::RBracket, "]");
        expr = std::make_unique<ast::IndexExpr>(
            Span::merge(start, this->current.span), std::move(expr), std::move(index)
        );
    }

    if (this->current == TokenKind::Dot) {
        return this->attr(start, std::move(expr));
    }

    return expr;
}

std::unique_ptr<ast::Expr> Parser::factor() {
    std::unique_ptr<ast::Expr> expr;

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
            return std::make_unique<ast::IntegerExpr>(Span::merge(start, this->current.span), value, bits, is_float);
        }
        case TokenKind::Char: {
            std::string value = this->current.value; this->next();
            return std::make_unique<ast::CharExpr>(Span::merge(start, this->current.span), value[0]);
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

            return std::make_unique<ast::FloatExpr>(Span::merge(start, this->current.span), result, is_double);
        }
        case TokenKind::String: {
            std::string value = this->current.value;
            this->next();

            expr = std::make_unique<ast::StringExpr>(Span::merge(start, this->current.span), value);
            break;
        }
        case TokenKind::Identifier: {
            std::string name = this->current.value;
            this->next();
            if (name == "true") {
                return std::make_unique<ast::IntegerExpr>(Span::merge(start, this->current.span), "1", 1);
            } else if (name == "false") {
                return std::make_unique<ast::IntegerExpr>(Span::merge(start, this->current.span), "0", 1);
            }

            expr = std::make_unique<ast::VariableExpr>(Span::merge(start, this->current.span), name);
            break;
        }
        case TokenKind::Sizeof: {
            this->next();
            
            this->expect(TokenKind::LParen, "(");
            std::unique_ptr<ast::Expr> expr = this->expr(false);

            Span end = this->expect(TokenKind::RParen, ")").span;

            // TODO: sizeof for types
            return std::make_unique<ast::SizeofExpr>(Span::merge(start, end), std::move(expr));
        }
        case TokenKind::Offsetof: {
            Span start = this->current.span;
            this->next();

            this->expect(TokenKind::LParen, "(");
            auto value = this->expr(false);

            this->expect(TokenKind::Comma, ",");
            std::string field = this->expect(TokenKind::Identifier, "identifier").value;

            Span end = this->expect(TokenKind::RParen, ")").span;
            return std::make_unique<ast::OffsetofExpr>(Span::merge(start, end), std::move(value), field);
        }
        case TokenKind::Match: {
            this->next();
            return this->parse_match_expr();
        }
        case TokenKind::LParen: {
            Span start = this->current.span;
            this->next();

            if (this->current == TokenKind::Identifier && this->peek() == TokenKind::Colon) {
                return this->parse_anonymous_function();
            }

            expr = this->expr(false);
            if (this->current == TokenKind::Comma) {
                std::vector<std::unique_ptr<ast::Expr>> elements;
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
                expr = std::make_unique<ast::TupleExpr>(Span::merge(start, end), std::move(elements));
            } else {
                this->expect(TokenKind::RParen, ")");
            }
            break;
        }
        case TokenKind::LBracket: {
            this->next();
            std::vector<std::unique_ptr<ast::Expr>> elements;

            if (this->current != TokenKind::RBracket) {
                auto element = this->expr(false);
                if (this->current == TokenKind::SemiColon) {
                    this->next();
                    auto size = this->expr(false);

                    this->expect(TokenKind::RBracket, "]");
                    return std::make_unique<ast::ArrayFillExpr>(Span::merge(start, this->current.span), std::move(element), std::move(size));
                }

                elements.push_back(std::move(element));
                this->expect(TokenKind::Comma, ",");
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
            expr = std::make_unique<ast::ArrayExpr>(Span::merge(start, end), std::move(elements));
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
        expr = std::make_unique<ast::PathExpr>(Span::merge(start, this->current.span), value, std::move(expr));
    }

    if (this->current == TokenKind::Dot) {
        expr = this->attr(start, std::move(expr));
    } else if (this->current == TokenKind::LBracket) {
        expr = this->element(start, std::move(expr));
    }

    return expr;
}