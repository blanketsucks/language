#include <quart/parser/parser.h>
#include <quart/parser/attrs.h>
#include <quart/parser/ast.h>
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
static std::map<llvm::StringRef, std::pair<u32, bool>> NUM_TYPES_BIT_MAPPING = {
    {"i8", {8, false}},
    {"i16", {16, false}},
    {"i32", {32, false}},
    {"i64", {64, false}},
    {"i128", {128, false}},
    {"f32", {32, true}},
    {"f64", {64, true}}
};

static std::map<llvm::StringRef, ast::BuiltinType> STR_TO_TYPE = {
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

Token const& Parser::peek(u32 offset) const {
    if (this->index >= this->tokens.size()) {
        return this->tokens.back(); // EOF
    }

    return this->tokens[this->index + offset];
}

Token& Parser::peek(u32 offset) {
    if (this->index >= this->tokens.size()) {
        return this->tokens.back(); // EOF
    }

    return this->tokens[this->index + offset];
}

Token Parser::expect(TokenKind type, llvm::StringRef value) {
    if (this->current.type != type) {
        ERROR(this->current.span, "Expected {0}", value);
    }

    Token token = this->current;
    this->next();

    return token;
}

llvm::Optional<Token> Parser::try_expect(TokenKind type) {
    if (this->current != type) return llvm::None;

    Token token = this->current;
    this->next();

    return token;
}

bool Parser::is_valid_attribute(llvm::StringRef name) {
    return this->attributes.find(name) != this->attributes.end();
}

int Parser::get_token_precendence() {
    auto it = PRECEDENCES.find(this->current.type);
    if (it == PRECEDENCES.end()) {
        return -1;
    }

    return it->second;
}

bool Parser::is_upcoming_constructor(ast::Expr const& previous) const {
    if (this->current != TokenKind::LBrace) return false;

    auto& peek = this->peek();
    if (peek == TokenKind::RBrace) return true;
    
    if (!peek.is(TokenKind::Identifier) || !this->peek(2).is(TokenKind::Colon)) {
        return false;
    }

    return previous.is(ast::ExprKind::Variable, ast::ExprKind::Path);
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

OwnPtr<ast::TypeExpr> Parser::parse_type() {
    Span start = this->current.span;

    switch (this->current.type) {
        case TokenKind::BinaryAnd: {
            this->next();
            bool is_mutable = this->try_expect(TokenKind::Mut).hasValue();

            return make_own<ast::ReferenceTypeExpr>(
                this->current.span, this->parse_type(), is_mutable
            );
        }
        case TokenKind::Mul: {
            this->next();
            bool is_mutable = this->try_expect(TokenKind::Mut).hasValue();

            auto type = this->parse_type();
            if (type->kind == ast::TypeKind::Reference) {
                ERROR(type->span, "Cannot have a pointer to a reference");
            }

            return make_own<ast::PointerTypeExpr>(
                this->current.span, std::move(type), is_mutable
            );
        }
        case TokenKind::LParen: {
            this->next();
            if (this->current == TokenKind::RParen) {
                ERROR(this->current.span, "Tuple type literals must atleast have a single element");
            }

            std::vector<OwnPtr<ast::TypeExpr>> types;
            while (this->current != TokenKind::RParen) {
                types.push_back(this->parse_type());
                if (this->current != TokenKind::Comma) {
                    break;
                }

                this->next();
            }

            Span end = this->expect(TokenKind::RParen, ")").span;
            return make_own<ast::TupleTypeExpr>(Span::merge(start, end), std::move(types));
        }
        case TokenKind::LBracket: {
            this->next();
            auto type = this->parse_type();

            this->expect(TokenKind::SemiColon, ";");
            auto size = this->expr(false);

            Span end = this->expect(TokenKind::RBracket, "]").span;
            return make_own<ast::ArrayTypeExpr>(Span::merge(start, end), std::move(type), std::move(size));
        }
        case TokenKind::Identifier: {
            std::string name = this->current.value;
            this->next();

            if (name == "int") {
                this->expect(TokenKind::LParen, "(");
                auto size = this->expr(false);

                Span end = this->expect(TokenKind::RParen, ")").span;
                return make_own<ast::IntegerTypeExpr>(
                    Span::merge(start, end), std::move(size)
                );
            }

            if (STR_TO_TYPE.find(name) != STR_TO_TYPE.end()) {
                return make_own<ast::BuiltinTypeExpr>(
                    Span::merge(start, this->current.span), STR_TO_TYPE[name]
                );
            }

            Path p = this->parse_path(name);
            auto type = make_own<ast::NamedTypeExpr>(
                Span::merge(start, this->current.span),
                std::move(p.name), 
                std::move(p.segments)
            );

            if (this->current == TokenKind::Lt) {
                return make_own<ast::GenericTypeExpr>(
                    Span::merge(start, this->current.span), std::move(type), this->parse_generic_arguments()
                );
            }

            return type;
        }
        case TokenKind::Func: {
            this->next(); this->expect(TokenKind::LParen, "(");

            std::vector<OwnPtr<ast::TypeExpr>> args;
            while (this->current != TokenKind::RParen) {
                args.push_back(this->parse_type());
                if (this->current != TokenKind::Comma) {
                    break;
                }
                
                this->next();
            }

            Span end = this->expect(TokenKind::RParen, ")").span;
            OwnPtr<ast::TypeExpr> ret = nullptr;

            if (this->current == TokenKind::Arrow) {
                end = this->next().span; ret = this->parse_type();
            }

            return make_own<ast::FunctionTypeExpr>(
                Span::merge(start, end), std::move(args), std::move(ret)
            );
        }
        default: ERROR(this->current.span, "Expected type");
    }
}

OwnPtr<ast::BlockExpr> Parser::parse_block() {
    std::vector<OwnPtr<ast::Expr>> body;
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
    return make_own<ast::BlockExpr>(Span::merge(start, end), std::move(body));
}

std::vector<ast::GenericParameter> Parser::parse_generic_parameters() {
    std::vector<ast::GenericParameter> parameters;

    this->expect(TokenKind::Lt, "<");
    while (this->current != TokenKind::Gt) {
        Token token = this->expect(TokenKind::Identifier, "identifier");

        std::string name = token.value;
        Span span = token.span;

        ast::ExprList<ast::TypeExpr> constraints;
        OwnPtr<ast::TypeExpr> default_type = nullptr;

        if (this->current == TokenKind::Colon) {
            this->next();

            while (this->current != TokenKind::Comma && this->current != TokenKind::Gt) {
                constraints.push_back(this->parse_type());
                if (this->current != TokenKind::Add) {
                    break;
                }

                this->next();
            }
        }

        if (this->current == TokenKind::Assign) {
            this->next();
            default_type = this->parse_type();
        }

        parameters.push_back({
            std::move(name),
            std::move(constraints), 
            std::move(default_type), 
            span
        });
        if (this->current != TokenKind::Comma) {
            break;
        }

        this->next();
    }

    this->expect(TokenKind::Gt, ">");
    return parameters;
}

ast::ExprList<ast::TypeExpr> Parser::parse_generic_arguments() {
    ast::ExprList<ast::TypeExpr> arguments;

    this->expect(TokenKind::Lt, "<");
    while (this->current != TokenKind::Gt) {
        arguments.push_back(this->parse_type());
        if (this->current != TokenKind::Comma) {
            break;
        }

        this->next();
    }

    this->expect(TokenKind::Gt, ">");
    return arguments;
}

OwnPtr<ast::TypeAliasExpr> Parser::parse_type_alias() {
    Span start = this->current.span;
    std::vector<ast::GenericParameter> parameters;

    std::string name = this->expect(TokenKind::Identifier, "identifier").value;
    if (this->current == TokenKind::Lt) {
        parameters = this->parse_generic_parameters();
    }

    this->expect(TokenKind::Assign, "=");

    auto type = this->parse_type();
    Span end = this->expect(TokenKind::SemiColon, ";").span;

    return make_own<ast::TypeAliasExpr>(
        Span::merge(start, end), std::move(name), std::move(type), std::move(parameters)
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
        if (!this->current.is(
            TokenKind::Mut, TokenKind::Identifier, TokenKind::Mul, TokenKind::Ellipsis
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

        OwnPtr<ast::TypeExpr> type = nullptr;
        bool is_self = false;

        bool allow_self = this->is_inside_struct || this->self_usage_allowed;
        if (argument == "self" && allow_self && !has_kwargs) {
            is_self = true; this->next();
        } else {
            this->next(); this->expect(TokenKind::Colon, ":");
            type = this->parse_type();
        }

        OwnPtr<ast::Expr> default_value = nullptr;
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
            std::move(argument), 
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

OwnPtr<ast::PrototypeExpr> Parser::parse_prototype(
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
    OwnPtr<ast::TypeExpr> ret = nullptr;

    if (this->current == TokenKind::Arrow) {
        end = this->next().span; ret = this->parse_type();
    }

    return make_own<ast::PrototypeExpr>(
        span, std::move(name), std::move(pair.first), std::move(ret), 
        pair.second, is_operator, linkage
    );
}

OwnPtr<ast::Expr> Parser::parse_function_definition(
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

    std::vector<OwnPtr<ast::Expr>> body;
    while (!this->current.is(TokenKind::RBrace, TokenKind::EOS)) {
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

    return make_own<ast::FunctionExpr>(
        Span::merge(start, end), std::move(prototype), std::move(body)
    );
}

OwnPtr<ast::FunctionExpr> Parser::parse_function() {
    Span start = this->current.span;
    auto prototype = this->parse_prototype(ast::ExternLinkageSpecifier::Unspecified, true, false);
    
    this->expect(TokenKind::LBrace, "{");
    this->is_inside_function = true;

    std::vector<OwnPtr<ast::Expr>> body;
    while (!this->current.is(TokenKind::RBrace, TokenKind::EOS)) {
        ast::Attributes attrs = this->parse_attributes();
        auto expr = this->statement();

        if (expr) {
            expr->attributes.update(attrs);
            body.push_back(std::move(expr));
        }
    }

    auto end = this->expect(TokenKind::RBrace, "}").span;
    this->is_inside_function = false;

    return make_own<ast::FunctionExpr>(
        Span::merge(start, end), std::move(prototype), std::move(body)
    );
}

OwnPtr<ast::IfExpr> Parser::parse_if_statement() {
    Span start = this->current.span;
    auto condition = this->expr(false);

    OwnPtr<ast::Expr> body;
    if (this->current != TokenKind::LBrace) {
        body = this->statement();
    } else {
        this->next();
        body = this->parse_block();
    }

    Span end = body->span;
    OwnPtr<ast::Expr> else_body;
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

    return make_own<ast::IfExpr>(
        Span::merge(start, end), std::move(condition), std::move(body), std::move(else_body)
    );
}

OwnPtr<ast::StructExpr> Parser::parse_struct() {
    Span start = this->current.span;
    Token token = this->expect(TokenKind::Identifier, "struct name");
    
    std::string name = token.value;
    Span end = token.span;

    std::vector<ast::StructField> fields;
    std::vector<OwnPtr<ast::Expr>> methods;

    if (this->current == TokenKind::SemiColon) {
        this->next();
        return make_own<ast::StructExpr>(
            Span::merge(start, this->current.span), std::move(name), true, std::move(fields), std::move(methods)
        );
    }

    this->is_inside_struct = true;

    this->expect(TokenKind::LBrace, "{");
    u32 index = 0;

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


        if (this->current != TokenKind::Identifier && !this->current.is(STRUCT_ALLOWED_KEYWORDS)) {
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
                    std::move(name), 
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

    return make_own<ast::StructExpr>(
        Span::merge(start, end), std::move(name), false, std::move(fields), std::move(methods)
    );    
}

OwnPtr<ast::Expr> Parser::parse_variable_definition(bool is_const) {
    Span span = this->current.span;

    OwnPtr<ast::TypeExpr> type = nullptr;
    std::vector<ast::Ident> names;

    bool has_consume_rest = false;
    bool is_mutable = false;

    if (is_const && this->current == TokenKind::Mut) {
        ERROR(this->current.span, "Cannot use 'mut' with 'const'");
    } else if (!is_const && this->current == TokenKind::Mut) {
        is_mutable = true;
        this->next();
    }

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
    } else {
        Token token = this->expect(TokenKind::Identifier, "variable name");
        names.push_back({std::move(token.value), is_mutable, Span::merge(span, token.span)});
    }

    if (this->current == TokenKind::Colon) {
        this->next(); type = this->parse_type();
    }
    
    OwnPtr<ast::Expr> expr = nullptr;
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

        return make_own<ast::ConstExpr>(
            span, std::move(names[0].value), std::move(type), std::move(expr)
        );
    } else {
        return make_own<ast::VariableAssignmentExpr>(
            span, std::move(names), std::move(type), std::move(expr), false
        );
    }
}

OwnPtr<ast::Expr> Parser::parse_extern(ast::ExternLinkageSpecifier linkage) {
    OwnPtr<ast::Expr> definition;
    Span start = this->current.span;
    
    ast::Attributes attrs = this->parse_attributes();
    if (this->current == TokenKind::Identifier) {
        std::string name = this->current.value;
        Span span = this->current.span;

        this->next();

        this->expect(TokenKind::Colon, ":");
        auto type = this->parse_type();

        OwnPtr<ast::Expr> expr;
        if (this->current == TokenKind::Assign) {
            expr = this->expr(false);
        }

        Span end = this->expect(TokenKind::SemiColon, ";").span;

        std::vector<ast::Ident> names = {{std::move(name), true, span},};
        definition = make_own<ast::VariableAssignmentExpr>(
            Span::merge(start, end), std::move(names), std::move(type), std::move(expr), true
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

OwnPtr<ast::Expr> Parser::parse_extern_block() {
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
        std::vector<OwnPtr<ast::Expr>> definitions;
        this->next();

        while (this->current != TokenKind::RBrace) {
            auto def = this->parse_extern(linkage);
            if (this->handle_expr_attributes(def->attributes) != AttributeHandler::Ok) {
                continue;
            }

            definitions.push_back(std::move(def));
        }

        Span end = this->expect(TokenKind::RBrace, "}").span;
        return make_own<ast::ExternBlockExpr>(Span::merge(start, end), std::move(definitions));
    }

    return this->parse_extern(linkage);
}

OwnPtr<ast::EnumExpr> Parser::parse_enum() {
    Span start = this->current.span;
    std::string name = this->expect(TokenKind::Identifier, "enum name").value;

    OwnPtr<ast::TypeExpr> type;
    if (this->current == TokenKind::Colon) {
        this->next(); type = this->parse_type();
    }

    this->expect(TokenKind::LBrace, "{");

    std::vector<ast::EnumField> fields;
    while (this->current != TokenKind::RBrace) {
        std::string field = this->expect(TokenKind::Identifier, "enum field name").value;
        OwnPtr<ast::Expr> value = nullptr;

        if (this->current == TokenKind::Assign) {
            this->next();
            value = this->expr(false);
        }

        fields.push_back({std::move(field), std::move(value)});
        if (this->current != TokenKind::Comma) {
            break;
        }

        this->next();
    }

    Span end = this->expect(TokenKind::RBrace, "}").span;
    return make_own<ast::EnumExpr>(
        Span::merge(start, end), std::move(name), std::move(type), std::move(fields)
    );
}

OwnPtr<ast::Expr> Parser::parse_anonymous_function() {
    auto pair = this->parse_arguments();

    OwnPtr<ast::TypeExpr> ret = nullptr;
    if (this->current == TokenKind::Colon) {
        this->next();
        ret = this->parse_type();
    }

    this->expect(TokenKind::DoubleArrow, "=>");

    std::vector<OwnPtr<ast::Expr>> body;
    body.push_back(this->expr(false));
    
    auto prototype = make_own<ast::PrototypeExpr>(
        this->current.span,
        "",
        std::move(pair.first), 
        std::move(ret), 
        std::move(pair.second),
        false,
        ast::ExternLinkageSpecifier::None
    );
    
    return make_own<ast::FunctionExpr>(
        Span::merge(prototype->span, this->current.span), std::move(prototype), std::move(body)
    );
}

OwnPtr<ast::CallExpr> Parser::parse_call(
    Span start, OwnPtr<ast::Expr> callee
) {
    std::vector<OwnPtr<ast::Expr>> args;
    std::map<std::string, OwnPtr<ast::Expr>> kwargs;

    bool has_kwargs = false;
    while (this->current != TokenKind::RParen) {
        if (this->current == TokenKind::Identifier && this->peek() == TokenKind::Colon) {
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

    Span end = this->expect(TokenKind::RParen, ")").span;
    return make_own<ast::CallExpr>(
        Span::merge(start, end), std::move(callee), std::move(args), std::move(kwargs)
    );
}

OwnPtr<ast::MatchExpr> Parser::parse_match_expr() {
    auto value = this->expr(false);
    this->expect(TokenKind::LBrace, "{");

    std::vector<ast::MatchArm> arms;
    bool has_wildcard = false;

    size_t i = 0;
    while (this->current != TokenKind::RBrace) {
        ast::MatchPattern pattern;
        pattern.is_wildcard = false;

        if (this->current == TokenKind::Else) {
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
    return make_own<ast::MatchExpr>(value->span, std::move(value), std::move(arms));
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

std::vector<OwnPtr<ast::Expr>> Parser::parse() {
    return this->statements();
}

std::vector<OwnPtr<ast::Expr>> Parser::statements() {
    std::vector<OwnPtr<ast::Expr>> statements;

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

OwnPtr<ast::Expr> Parser::statement() {
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
                return make_own<ast::ReturnExpr>(Span::merge(start, end), nullptr);
            }

            auto expr = this->expr(false);

            Span end = this->expect(TokenKind::SemiColon, ";").span;
            return make_own<ast::ReturnExpr>(Span::merge(start, end), std::move(expr));
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
            return make_own<ast::WhileExpr>(Span::merge(start, body->span), std::move(condition), std::move(body));
        }
        case TokenKind::Break: {
            if (!this->is_inside_loop) {
                ERROR(this->current.span, "Break statement outside of loop");
            }

            Span start = this->current.span;
            this->next();

            Span end = this->expect(TokenKind::SemiColon, ";").span;
            return make_own<ast::BreakExpr>(Span::merge(start, end));
        } 
        case TokenKind::Continue: {
            if (!this->is_inside_loop) {
                ERROR(this->current.span, "Continue statement outside of loop");
            }

            Span start = this->current.span;
            this->next();

            Span end = this->expect(TokenKind::SemiColon, ";").span;
            return make_own<ast::ContinueExpr>(Span::merge(start, end));
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

            return make_own<ast::UsingExpr>(
                Span::merge(start, this->current.span), std::move(members), std::move(parent)
            );
        } 
        case TokenKind::Defer: {
            if (!this->is_inside_function) {
                ERROR(this->current.span, "Defer statement outside of function");
            }

            Span start = this->current.span;
            this->next();

            auto expr = this->expr();
            return make_own<ast::DeferExpr>(Span::merge(start, this->current.span), std::move(expr));
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
            return make_own<ast::ImportExpr>(Span::merge(start, end), std::move(name), is_wildcard, is_relative);
        }
        case TokenKind::Module: {
            this->next();
            std::string name = this->expect(TokenKind::Identifier, "module name").value;

            Span start = this->expect(TokenKind::LBrace, "{").span;
            std::vector<OwnPtr<ast::Expr>> body;

            while (this->current != TokenKind::RBrace) {
                body.push_back(this->statement());
            }

            Span end = this->expect(TokenKind::RBrace, "}").span;
            return make_own<ast::ModuleExpr>(
                Span::merge(start, end), std::move(name), std::move(body)
            );
        }
        case TokenKind::For: {
            Span start = this->current.span;
            this->next();

            bool is_mutable = this->try_expect(TokenKind::Mut).hasValue();

            Token token = this->expect(TokenKind::Identifier, "identifier");
            ast::Ident ident = { std::move(token.value), is_mutable, token.span };

            this->expect(TokenKind::In, "in");

            bool outer = this->is_inside_loop;
            this->is_inside_loop = true;

            auto expr = this->expr(false);

            if (this->current == TokenKind::DoubleDot) {
                this->next();

                OwnPtr<ast::Expr> end = nullptr;
                if (this->current != TokenKind::LBrace) {
                    end = this->expr(false);
                }
                
                this->expect(TokenKind::LBrace, "{");

                auto body = this->parse_block();
                this->is_inside_loop = outer;

                return make_own<ast::RangeForExpr>(
                    Span::merge(start, body->span), std::move(ident), std::move(expr), std::move(end), std::move(body)
                );
            }

            this->expect(TokenKind::LBrace, "{");

            auto body = this->parse_block();
            this->is_inside_loop = outer;

            return make_own<ast::ForExpr>(
                Span::merge(start, body->span), std::move(ident), std::move(expr), std::move(body)
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
                
            return make_own<ast::StaticAssertExpr>(Span::merge(start, end), std::move(expr), std::move(message));
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
            return make_own<ast::ImplExpr>(
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

OwnPtr<ast::Expr> Parser::expr(bool semicolon) {
    auto expr = this->binary(0, this->unary());
    if (semicolon) this->end();

    return expr;
}

OwnPtr<ast::Expr> Parser::binary(int prec, OwnPtr<ast::Expr> left) {
    while (true) {
        int precedence = this->get_token_precendence();
        if (precedence < prec) {
            return left;
        }

        TokenKind kind = this->current.type;
        if (kind == TokenKind::Gt && this->peek() == TokenKind::Gt) {
            this->next();
            kind = TokenKind::Rsh;
        }

        BinaryOp op = BINARY_OPS[kind];

        this->next();
        auto right = this->unary();

        int next = this->get_token_precendence();
        if (precedence < next) {
            right = this->binary(precedence + 1, std::move(right));
        }
       
        if (INPLACE_OPERATORS.find(kind) != INPLACE_OPERATORS.end()) {
            left = make_own<ast::InplaceBinaryOpExpr>(
                Span::merge(left->span, right->span), op, std::move(left), std::move(right)
            );
        } else {
            left = make_own<ast::BinaryOpExpr>(
                Span::merge(left->span, right->span), op, std::move(left), std::move(right)
            );
        }
    }
}

OwnPtr<ast::Expr> Parser::unary() {
    auto iterator = UNARY_OPS.find(this->current.type);
    OwnPtr<ast::Expr> expr;

    if (iterator == UNARY_OPS.end()) {
        expr = this->call();
    } else if (this->current.is(TokenKind::BinaryAnd)) {
        Span start = this->current.span;
        this->next();

        bool is_mutable = this->try_expect(TokenKind::Mut).hasValue();

        auto value = this->expr(false);
        expr = make_own<ast::ReferenceExpr>(
            Span::merge(start, value->span), std::move(value), is_mutable
        );
    } else {
        Span start = this->current.span;

        UnaryOp op = iterator->second;
        this->next();

        auto value = this->call();
        expr = make_own<ast::UnaryOpExpr>(Span::merge(start, value->span), std::move(value), op);
    }

    switch (this->current.type) {
        case TokenKind::As: {
            this->next();
            auto type = this->parse_type();

            Span span = Span::merge(expr->span, type->span);
            return make_own<ast::CastExpr>(span, std::move(expr), std::move(type));
        }
        case TokenKind::If: {
            this->next();
            auto condition = this->expr(false);

            if (this->current != TokenKind::Else) {
                ERROR(this->current.span, "Expected 'else' after 'if' in ternary expression");
            }

            this->next();
            auto else_expr = this->expr(false);

            return make_own<ast::TernaryExpr>(
                Span::merge(expr->span, else_expr->span), std::move(condition), std::move(expr), std::move(else_expr)
            );
        }
        default: return expr;
    }
}

OwnPtr<ast::Expr> Parser::call() {
    Span start = this->current.span;

    auto expr = this->primary();
    while (this->current == TokenKind::LParen) {
        this->next();
        expr = this->parse_call(start, std::move(expr));
    }

    if (this->current.is(TokenKind::Inc, TokenKind::Dec)) {
        UnaryOp op = this->current.type == TokenKind::Inc ? UnaryOp::Inc : UnaryOp::Dec;
        expr = make_own<ast::UnaryOpExpr>(Span::merge(start, this->current.span), std::move(expr), op);

        this->next();
    } else if (this->is_upcoming_constructor(*expr)) {
        Span end = this->current.span;
        this->next();

        std::vector<ast::ConstructorField> fields;
        if (this->current == TokenKind::RBrace) {
            this->next();
            return make_own<ast::EmptyConstructorExpr>(
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
        expr = make_own<ast::ConstructorExpr>(Span::merge(start, end), std::move(expr), std::move(fields));
    }

    if (this->current == TokenKind::Dot) {
        expr = this->attr(start, std::move(expr));
    } else if (this->current == TokenKind::LBracket) {
        expr = this->element(start, std::move(expr));
    }

    return expr;
}

OwnPtr<ast::Expr> Parser::attr(Span start, OwnPtr<ast::Expr> expr) {
    while (this->current == TokenKind::Dot) {
        this->next();
        
        std::string value = this->expect(TokenKind::Identifier, "attribute name").value;
        expr = make_own<ast::AttributeExpr>(
            Span::merge(start, this->current.span), std::move(expr), value
        );
    }

    if (this->current == TokenKind::LBracket) {
        return this->element(start, std::move(expr));
    }
    
    return expr;
}

OwnPtr<ast::Expr> Parser::element(Span start, OwnPtr<ast::Expr> expr) {
    while (this->current == TokenKind::LBracket) {
        this->next();
        auto index = this->expr(false);

        this->expect(TokenKind::RBracket, "]");
        expr = make_own<ast::IndexExpr>(
            Span::merge(start, this->current.span), std::move(expr), std::move(index)
        );
    }

    if (this->current == TokenKind::Dot) {
        return this->attr(start, std::move(expr));
    }

    return expr;
}

OwnPtr<ast::Expr> Parser::primary() {
    OwnPtr<ast::Expr> expr;

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
            return make_own<ast::IntegerExpr>(Span::merge(start, this->current.span), value, bits, is_float);
        }
        case TokenKind::Char: {
            std::string value = this->current.value; this->next();
            return make_own<ast::CharExpr>(Span::merge(start, this->current.span), value[0]);
        }
        case TokenKind::Float: {
            double result = 0.0;
            llvm::StringRef(this->current.value).getAsDouble(result);

            this->next();
            bool is_double = false;

            if (this->current.is(TokenKind::Identifier) && this->current.value == "f64") {
                this->next();
                is_double = true;
            }

            return make_own<ast::FloatExpr>(Span::merge(start, this->current.span), result, is_double);
        }
        case TokenKind::String: {
            std::string value = this->current.value;
            this->next();

            expr = make_own<ast::StringExpr>(Span::merge(start, this->current.span), value);
            break;
        }
        case TokenKind::Identifier: {
            std::string name = this->current.value;
            this->next();
            if (name == "true") {
                return make_own<ast::IntegerExpr>(Span::merge(start, this->current.span), "1", 1);
            } else if (name == "false") {
                return make_own<ast::IntegerExpr>(Span::merge(start, this->current.span), "0", 1);
            }

            expr = make_own<ast::VariableExpr>(Span::merge(start, this->current.span), name);
            break;
        }
        case TokenKind::Sizeof: {
            this->next();
            
            this->expect(TokenKind::LParen, "(");
            OwnPtr<ast::Expr> expr = this->expr(false);

            Span end = this->expect(TokenKind::RParen, ")").span;

            // TODO: sizeof for types
            return make_own<ast::SizeofExpr>(Span::merge(start, end), std::move(expr));
        }
        case TokenKind::Offsetof: {
            Span start = this->current.span;
            this->next();

            this->expect(TokenKind::LParen, "(");
            auto value = this->expr(false);

            this->expect(TokenKind::Comma, ",");
            std::string field = this->expect(TokenKind::Identifier, "identifier").value;

            Span end = this->expect(TokenKind::RParen, ")").span;
            return make_own<ast::OffsetofExpr>(Span::merge(start, end), std::move(value), field);
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
                std::vector<OwnPtr<ast::Expr>> elements;
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
                expr = make_own<ast::TupleExpr>(Span::merge(start, end), std::move(elements));
            } else {
                this->expect(TokenKind::RParen, ")");
            }
            break;
        }
        case TokenKind::LBracket: {
            this->next();
            std::vector<OwnPtr<ast::Expr>> elements;

            if (this->current != TokenKind::RBracket) {
                auto element = this->expr(false);
                if (this->current == TokenKind::SemiColon) {
                    this->next();
                    auto size = this->expr(false);

                    this->expect(TokenKind::RBracket, "]");
                    return make_own<ast::ArrayFillExpr>(Span::merge(start, this->current.span), std::move(element), std::move(size));
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
            expr = make_own<ast::ArrayExpr>(Span::merge(start, end), std::move(elements));
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
        expr = make_own<ast::PathExpr>(Span::merge(start, this->current.span), std::move(expr), value);
    }

    if (this->current == TokenKind::Dot) {
        expr = this->attr(start, std::move(expr));
    } else if (this->current == TokenKind::LBracket) {
        expr = this->element(start, std::move(expr));
    }

    return expr;
}