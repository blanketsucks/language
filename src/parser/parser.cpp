#include "parser/parser.h"
#include "parser/ast.h"
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

static std::vector<std::string> NAMESPACE_ALLOWED_KEYWORDS = {
    "struct",
    "namespace",
    "extern",
    "func",
    "type",
    "const"
};

static std::vector<std::string> STRUCT_ALLOWED_KEYWORDS = {
    "func",
    "type",
    "const"
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
        ERROR(last.end, "Expected ';'");
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
        ERROR(this->current.start, "Expected {0}", value);
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

utils::Ref<ast::TypeExpr> Parser::parse_type() {
    utils::Ref<ast::TypeExpr> type = nullptr;
    Location start = this->current.start;

    bool is_reference = false;
    if (this->current == TokenKind::BinaryAnd) {
        is_reference = true;
        this->next();
    }

    if (this->current.match(TokenKind::Identifier, "int")) {
        this->next();
        this->expect(TokenKind::LParen, "(");

        auto size = this->expr(false);
        Location end = this->expect(TokenKind::RParen, ")").end;

        type = utils::make_ref<ast::IntegerTypeExpr>(start, end, std::move(size));
    } else if (this->current.match(TokenKind::Keyword, "func")) {
        // func(int, int) -> int 
        this->next(); this->expect(TokenKind::LParen, "(");

        std::vector<utils::Ref<ast::TypeExpr>> args;
        while (this->current != TokenKind::RParen) {
            args.push_back(this->parse_type());
            if (this->current != TokenKind::Comma) {
                break;
            }
            
            this->next();
        }

        Location end = this->expect(TokenKind::RParen, ")").end;
        utils::Ref<ast::TypeExpr> ret = nullptr;

        if (this->current == TokenKind::Arrow) {
            end = this->next().end; ret = this->parse_type();
        }

        type = utils::make_ref<ast::FunctionTypeExpr>(start, end, std::move(args), std::move(ret));
    } else if (this->current == TokenKind::LParen) {
        this->next();
        if (this->current == TokenKind::RParen) {
            ERROR(this->current.start, "Tuple type literals must atleast have a single element");
        }

        std::vector<utils::Ref<ast::TypeExpr>> types;
        while (this->current != TokenKind::RParen) {
            types.push_back(this->parse_type());
            if (this->current != TokenKind::Comma) {
                break;
            }

            this->next();
        }

        Location end = this->expect(TokenKind::RParen, ")").end;
        type = utils::make_ref<ast::TupleTypeExpr>(start, end, std::move(types));
    } else if (this->current == TokenKind::LBracket) {
        this->next();
        auto element = this->parse_type();

        this->expect(TokenKind::SemiColon, ";");
        auto size = this->expr(false);

        Location end = this->expect(TokenKind::RBracket, "]").end;
        type = utils::make_ref<ast::ArrayTypeExpr>(start, end, std::move(element), std::move(size));
    } else {
        std::string name = this->expect(TokenKind::Identifier, "identifier").value;
        if (this->types.find(name) != this->types.end()) {
            type = utils::make_ref<ast::BuiltinTypeExpr>(start, this->current.end, this->types[name]);
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

            type = utils::make_ref<ast::NamedTypeExpr>(start, this->current.end, name, parents);
        }
    }

    while (this->current == TokenKind::Mul) {
        Location end = this->next().end;
        type = utils::make_ref<ast::PointerTypeExpr>(start, end, std::move(type));
    }

    if (is_reference) {
        type = utils::make_ref<ast::ReferenceTypeExpr>(start, this->current.end, std::move(type));
    }

    return type;
} 

utils::Ref<ast::BlockExpr> Parser::parse_block() {
    std::vector<utils::Ref<ast::Expr>> body;
    Location start = this->current.start;

    while (this->current != TokenKind::RBrace) {
        ast::Attributes attrs = this->parse_attributes();
        auto expr = this->statement();

        if (expr) {
            expr->attributes.update(attrs);
            body.push_back(std::move(expr));
        }
    }

    Location end = this->expect(TokenKind::RBrace, "}").end;
    return utils::make_ref<ast::BlockExpr>(start, end, std::move(body));
}

utils::Ref<ast::TypeAliasExpr> Parser::parse_type_alias() {
    Location start = this->current.start;

    std::string name = this->expect(TokenKind::Identifier, "identifier").value;
    this->expect(TokenKind::Assign, "=");

    auto type = this->parse_type();
    this->end();

    return utils::make_ref<ast::TypeAliasExpr>(start, this->current.end, name, std::move(type));
}

utils::Ref<ast::PrototypeExpr> Parser::parse_prototype(
    ast::ExternLinkageSpecifier linkage, bool with_name, bool is_operator
) {
    Location start = this->current.start;
    std::string name;

    if (with_name) {
        name = this->expect(TokenKind::Identifier, "function name").value;
    }

    this->expect(TokenKind::LParen, "(");

    std::vector<ast::Argument> args;

    bool has_kwargs = false;
    bool has_default_values = false;
    bool is_variadic = false;

    while (this->current != TokenKind::RParen) {
        std::string value = this->current.value;
        if (!this->current.match({TokenKind::Keyword, TokenKind::Identifier, TokenKind::Mul, TokenKind::Ellipsis})) {
            ERROR(this->current.start, "Expected argument name, '*' or '...'");
        }

        bool is_immutable = false;
        if (this->current == TokenKind::Ellipsis) {
            if (is_variadic) {
                ERROR(this->current.start, "Cannot have multiple variadic arguments");
            }

            is_variadic = true;
            this->next();

            break;
        } else if (this->current == TokenKind::Mul) {
            if (has_kwargs) {
                ERROR(this->current.start, "Only one '*' seperator is allowed in a function prototype");
            }

            has_kwargs = true;
            this->next();

            this->expect(TokenKind::Comma, ",");
            continue;
        } else if (this->current == TokenKind::Keyword) {
            if (this->current.value != "immutable") {
                ERROR(this->current.start, "Expected 'immutable' keyword");
            }

            this->next();
            is_immutable = true;

            if (this->current != TokenKind::Identifier) {
                ERROR(this->current.start, "Expected identifier");
            }

            value = this->current.value;
        }

        utils::Ref<ast::TypeExpr> type = nullptr;
        std::string argument;

        bool is_self = false;
        if (value == "self") {
            if (this->is_inside_struct) {
                argument = "self"; is_self = true;
                this->next();
            } else {
                this->next();
                this->expect(TokenKind::Colon, ":");

                type = this->parse_type();
                argument = value;
            }
        } else {
            this->next(); this->expect(TokenKind::Colon, ":");
            type = this->parse_type();

            argument = value;
        }

        utils::Ref<ast::Expr> default_value = nullptr;
        if (this->current == TokenKind::Assign) {
            has_default_values = true;
            this->next();

            default_value = this->expr(false);
        } else {
            if (has_default_values) {
                ERROR(this->current.start, "Cannot have non-default arguments follow default arguments");
            }
        }

        ast::Argument arg = {
            argument, 
            std::move(type),
            std::move(default_value),
            is_self, 
            has_kwargs, 
            is_immutable
        };

        args.push_back(std::move(arg));
        if (this->current != TokenKind::Comma) {
            break;
        }

        this->next();
    }

    Location end = this->expect(TokenKind::RParen, ")").end;
    utils::Ref<ast::TypeExpr> ret = nullptr;
    if (this->current == TokenKind::Arrow) {
        end = this->next().end; ret = this->parse_type();
    }

    return utils::make_ref<ast::PrototypeExpr>(
        start, end, name, std::move(args), std::move(ret), is_variadic, is_operator, linkage
    );
}

utils::Ref<ast::Expr> Parser::parse_function_definition(
    ast::ExternLinkageSpecifier linkage, bool is_operator
) {
    auto prototype = this->parse_prototype(linkage, true, is_operator);
    if (this->current == TokenKind::SemiColon) {
        this->next();
        return prototype;
    }
    
    this->expect(TokenKind::LBrace, "{");
    this->is_inside_function = true;

    auto body = this->parse_block();
    this->is_inside_function = false;

    return utils::make_ref<ast::FunctionExpr>(prototype->start, body->end, std::move(prototype), std::move(body));
}

utils::Ref<ast::IfExpr> Parser::parse_if_statement() {
    Location start = this->current.start;
    this->expect(TokenKind::LParen, "(");

    auto condition = this->expr(false);
    this->expect(TokenKind::RParen, ")");

    utils::Ref<ast::Expr> body;
    if (this->current != TokenKind::LBrace) {
        body = this->statement();
    } else {
        this->next();
        body = this->parse_block();
    }

    Location end = body->end;
    utils::Ref<ast::Expr> else_body;
    if (this->current == TokenKind::Keyword && this->current.value == "else") {
        this->next();
        if (this->current != TokenKind::LBrace) {
            else_body = this->statement();
        } else {
            this->next();
            else_body = this->parse_block();
        }

        end = else_body->end;
    }

    return utils::make_ref<ast::IfExpr>(start, end, std::move(condition), std::move(body), std::move(else_body));
}

utils::Ref<ast::StructExpr> Parser::parse_struct() {
    Location start = this->current.start;
    std::string name = this->expect(TokenKind::Identifier, "struct name").value;

    std::map<std::string, ast::StructField> fields;
    std::vector<utils::Ref<ast::Expr>> methods;
    std::vector<utils::Ref<ast::Expr>> parents;

    if (this->current == TokenKind::SemiColon) {
        this->next();
        return utils::make_ref<ast::StructExpr>(
            start, this->current.end, name, true, std::move(parents), std::move(fields), std::move(methods)
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

        if (this->current.match(TokenKind::Keyword, "private")) {
            is_private = true;
            this->next();
        } else if (this->current.match(TokenKind::Keyword, "readonly")) {
            is_readonly = true;
            this->next();
        } else if (this->current.match(TokenKind::Keyword, "operator")) {
            is_operator = true;
            this->next();
        }

        if (this->current != TokenKind::Identifier && !this->current.match(TokenKind::Keyword, STRUCT_ALLOWED_KEYWORDS)) {
            ERROR(this->current.start, "Expected field name or function definition");
        }

        if (this->current.value == "func") {
            this->next();
            auto definition = this->parse_function_definition(
                ast::ExternLinkageSpecifier::None, is_operator
            );

            if (is_private) {
                definition->attributes.add("private");
            }

            definition->attributes.update(attributes);
            methods.push_back(std::move(definition));
        } else if (this->current.value == "const") {
            this->next();
            methods.push_back(this->parse_variable_definition(true));
        } else if (this->current.value == "type") {
            this->next();
            methods.push_back(this->parse_type_alias());
        } else {
            if (index == UINT32_MAX) {
                ERROR(this->current.start, "Cannot define more than {0} fields inside a structure", UINT32_MAX);
            }

            std::string name = this->current.value;
            this->next();

            this->expect(TokenKind::Colon, ":");
            fields[name] = {
                name, 
                this->parse_type(), 
                index,
                is_private, 
                is_readonly
            };

            this->expect(TokenKind::SemiColon, ";");
            index++;
        }
    }

    Location end = this->expect(TokenKind::RBrace, "}").end;
    this->is_inside_struct = false;

    return utils::make_ref<ast::StructExpr>(
        start, end, name, false, std::move(parents), std::move(fields), std::move(methods)
    );    
}

utils::Ref<ast::Expr> Parser::parse_variable_definition(bool is_const) {
    Location start = this->current.start;
    utils::Ref<ast::TypeExpr> type = nullptr;

    std::vector<std::string> names;
    bool is_multiple_variables = false;
    bool has_consume_rest = false;
    bool is_immutable = false;

    if (!is_const && this->current.match(TokenKind::Keyword, "immutable")) {
        is_immutable = true;
        this->next();
    }

    std::string consume_rest;
    if (this->current == TokenKind::LParen) {
        this->next();

        while (this->current != TokenKind::RParen) {
            if (this->current == TokenKind::Mul) {
                if (has_consume_rest) {
                    ERROR(this->current.start, "Cannot consume rest of the arguments more than once");
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
    
    utils::Ref<ast::Expr> expr = nullptr;
    Location end;

    if (this->current != TokenKind::Assign) {
        end = this->expect(TokenKind::SemiColon, ";").end;

        if (!type) {
            ERROR(this->current.start, "Un-initialized variables must have an inferred type");
        }
    } else {
        this->next();
        expr = this->expr(false);
        
        this->expect(TokenKind::SemiColon, ";");
        end = expr->end;
    };

    if (names.size() > 1 && !expr) {
        ERROR(expr->start, "Expected an expression when using multiple named assignments");
    }
    
    if (is_const) {
        if (!expr) {
            ERROR(this->current.start, "Constants must have an initializer");
        }

        return utils::make_ref<ast::ConstExpr>(start, end, names[0], std::move(type), std::move(expr));
    } else {
        return utils::make_ref<ast::VariableAssignmentExpr>(
            start, end, names, std::move(type), std::move(expr), consume_rest, false, is_multiple_variables, is_immutable
        );
    }
}

utils::Ref<ast::NamespaceExpr> Parser::parse_namespace() {
    Location start = this->current.start;

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
    
    std::vector<utils::Ref<ast::Expr>> members;
    while (this->current != TokenKind::RBrace) {
        ast::Attributes attrs = this->parse_attributes();
        if (!this->current.match(TokenKind::Keyword, NAMESPACE_ALLOWED_KEYWORDS)) {
            ERROR(this->current.start, "Expected function, extern, struct, const, type, or namespace definition");
        }

        utils::Ref<ast::Expr> member;
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

    Location end = this->expect(TokenKind::RBrace, "}").end;
    return utils::make_ref<ast::NamespaceExpr>(start, end, name, parents, std::move(members));
}

utils::Ref<ast::Expr> Parser::parse_extern(ast::ExternLinkageSpecifier linkage) {
    utils::Ref<ast::Expr> definition;
    Location start = this->current.start;
    
    ast::Attributes attrs = this->parse_attributes();
    if (this->current == TokenKind::Identifier) {
        std::string name = this->current.value;
        std::string consume_rest;

        this->next();

        this->expect(TokenKind::Colon, ":");
        auto type = this->parse_type();

        utils::Ref<ast::Expr> expr;
        if (this->current == TokenKind::Assign) {
            expr = this->expr(false);
        }

        Location end = this->expect(TokenKind::SemiColon, ";").end;

        std::vector<std::string> names = {name,};
        definition = utils::make_ref<ast::VariableAssignmentExpr>(start, end, names, std::move(type), std::move(expr), consume_rest, true, false);
    } else {
        if (this->current != TokenKind::Keyword && this->current.value != "func") {
            ERROR(this->current.start, "Expected function");
        }

        this->next();
        definition = this->parse_function_definition(linkage);
    }

    definition->attributes.update(attrs);
    return definition;
}

utils::Ref<ast::Expr> Parser::parse_extern_block() {
    Location start = this->current.start;
    ast::ExternLinkageSpecifier linkage = ast::ExternLinkageSpecifier::Unspecified;

    if (this->current == TokenKind::String) {
        if (this->current.value != "C") {
            ERROR(this->current.start, "Unknown extern linkage specifier");
        }

        this->next();
        linkage = ast::ExternLinkageSpecifier::C;
    }

    // TODO: Allow const/let defitnitions
    if (this->current == TokenKind::LBrace) {
        std::vector<utils::Ref<ast::Expr>> definitions;
        this->next();

        while (this->current != TokenKind::RBrace) {
            auto definition = this->parse_extern(linkage);
            definitions.push_back(std::move(definition));
        }

        Location end = this->expect(TokenKind::RBrace, "}").end;
        return utils::make_ref<ast::BlockExpr>(start, end, std::move(definitions));
    }

    return this->parse_extern(linkage);
}

utils::Ref<ast::EnumExpr> Parser::parse_enum() {
    Location start = this->current.start;
    std::string name = this->expect(TokenKind::Identifier, "enum name").value;

    utils::Ref<ast::TypeExpr> type;
    if (this->current == TokenKind::Colon) {
        this->next(); type = this->parse_type();
    }

    this->expect(TokenKind::LBrace, "{");

    std::vector<ast::EnumField> fields;
    while (this->current != TokenKind::RBrace) {
        std::string field = this->expect(TokenKind::Identifier, "enum field name").value;
        utils::Ref<ast::Expr> value = nullptr;

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

    Location end = this->expect(TokenKind::RBrace, "}").end;
    return utils::make_ref<ast::EnumExpr>(start, end, name, std::move(type), std::move(fields));

}

std::vector<utils::Ref<ast::Expr>> Parser::parse() {
    return this->statements();
}

std::vector<utils::Ref<ast::Expr>> Parser::statements() {
    std::vector<utils::Ref<ast::Expr>> statements;

    while (this->current != TokenKind::EOS) {
        ast::Attributes attrs = this->parse_attributes();
        auto expr = this->statement();

        if (expr) {
            expr->attributes.update(attrs);
            statements.push_back(std::move(expr));
        }
    }

    return statements;
}

utils::Ref<ast::Expr> Parser::statement() {
    switch (this->current.type) {
        case TokenKind::Keyword:
            if (this->current.value == "extern") {
                this->next();
                return this->parse_extern_block();
            } else if (this->current.value == "func") {
                this->next();
                return this->parse_function_definition();
            } else if (this->current.value == "return") {
                if (!this->is_inside_function) {
                    ERROR(this->current.start, "Return statement outside of function");
                }

                this->next();
                Location start = this->current.start;
                if (this->current == TokenKind::SemiColon) {
                    this->next();
                    return utils::make_ref<ast::ReturnExpr>(start, this->current.end, nullptr);
                }

                auto expr = this->expr(false);

                Location end = this->expect(TokenKind::SemiColon, ";").end;
                return utils::make_ref<ast::ReturnExpr>(start, end, std::move(expr));
            } else if (this->current.value == "if") {
                if (!this->is_inside_function) {
                    ERROR(this->current.start, "If statement outside of function");
                }

                this->next();
                return this->parse_if_statement();
            } else if (this->current.value == "let") {
                this->next();
                return this->parse_variable_definition(false);
            }  else if (this->current.value == "const") {
                this->next();
                return this->parse_variable_definition(true);
            } else if (this->current.value == "struct") {
                if (this->is_inside_function) {
                    ERROR(this->current.start, "Struct definition inside function");
                }

                this->next();
                return this->parse_struct();
            } else if (this->current.value == "namespace") {
                this->next();
                return this->parse_namespace();
            } else if (this->current.value == "type") {
                this->next();
                return this->parse_type_alias();
            } else if (this->current.value == "while") {
                bool has_outer_loop = this->is_inside_loop;
                Location start = this->current.start;

                this->next();
                this->expect(TokenKind::LParen, "(");
                
                auto condition = this->expr(false);
                this->expect(TokenKind::RParen, ")");

                this->expect(TokenKind::LBrace, "{");

                this->is_inside_loop = true;
                auto body = this->parse_block();

                this->is_inside_loop = has_outer_loop;
                return utils::make_ref<ast::WhileExpr>(start, body->end, std::move(condition), std::move(body));
            } else if (this->current.value == "for") {
                bool has_outer_loop = this->is_inside_loop;
                Location start_ = this->current.start;

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
                return utils::make_ref<ast::ForExpr>(
                    start_, body->end, std::move(start), std::move(end), std::move(step), std::move(body)
                );
            } else if (this->current.value == "break") {
                if (!this->is_inside_loop) {
                    ERROR(this->current.start, "Break statement outside of loop");
                }

                Location start = this->current.start;
                this->next();

                Location end = this->expect(TokenKind::SemiColon, ";").end;
                return utils::make_ref<ast::BreakExpr>(start, end);
            } else if (this->current.value == "continue") {
                if (!this->is_inside_loop) {
                    ERROR(this->current.start, "Continue statement outside of loop");
                }

                Location start = this->current.start;
                this->next();

                Location end = this->expect(TokenKind::SemiColon, ";").end;
                return utils::make_ref<ast::ContinueExpr>(start, end);
            } else if (this->current.value == "using") {
                Location start = this->current.start;
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

                if (this->current != TokenKind::Keyword && this->current.value != "from") {
                    ERROR(this->current.start, "Expected 'from' keyword.");
                }

                this->next();
                auto parent = this->expr();

                return utils::make_ref<ast::UsingExpr>(start, this->current.end, members, std::move(parent));
            } else if (this->current.value == "defer") {
                if (!this->is_inside_function) {
                    ERROR(this->current.start, "Defer statement outside of function");
                }

                Location start = this->current.start;
                this->next();

                auto expr = this->expr();
                return utils::make_ref<ast::DeferExpr>(start, this->current.end, std::move(expr));
            } else if (this->current.value == "enum") {
                this->next();
                return this->parse_enum();
            } else if (this->current.value == "import") {
                Location start = this->current.start;
                this->next();

                bool is_relative = false;
                if (this->current == TokenKind::DoubleColon) {
                    is_relative = true;
                    this->next();
                }

                std::string name = this->expect(TokenKind::Identifier, "module name").value;
                bool is_wildcard = false;

                std::deque<std::string> parents;
                while (this->current == TokenKind::DoubleColon) {
                    this->next();

                    if (this->current == TokenKind::Mul) {
                        is_wildcard = true;
                        this->next();

                        break;
                    }

                    std::string parent = this->expect(TokenKind::Identifier, "module name").value;
                    parents.push_back(parent);
                }

                if (!parents.empty()) {
                    std::string back = parents.back();
                    parents.pop_back();

                    parents.push_back(name);
                    name = back;
                }

                Location end = this->expect(TokenKind::SemiColon, ";").end;
                return utils::make_ref<ast::ImportExpr>(start, end, name, parents, is_wildcard, is_relative);
            } else if (this->current.value == "foreach") {
                Location start = this->current.start;
                this->next(); this->expect(TokenKind::LParen, "(");

                // TODO: foreach ((x, y) in foo)
                auto variable = this->expect(TokenKind::Identifier, "identifier").value;
                this->expect(TokenKind::Keyword, "in");

                auto expr = this->expr(false);
                this->expect(TokenKind::RParen, ")");

                auto body = this->expr(false);
                return utils::make_ref<ast::ForeachExpr>(start, body->end, variable, std::move(expr), std::move(body));
            } else if (this->current.value == "static_assert") {
                Location start = this->current.start;
                this->next(); this->expect(TokenKind::LParen, "(");

                auto expr = this->expr(false);
                std::string message;
                if (this->current == TokenKind::Comma) {
                    this->next(); message = this->expect(TokenKind::String, "string").value;
                }

                this->expect(TokenKind::RParen, ")"); 
                Location end = this->expect(TokenKind::SemiColon, ";").end;
                    
                return utils::make_ref<ast::StaticAssertExpr>(start, end, std::move(expr), message);
            } else {
                ERROR(this->current.start, "Expected an expression");
            }

            break;
        case TokenKind::SemiColon:
            this->next();
            return nullptr;
        default:
            return this->expr();
    }

    ERROR(this->current.start, "Unreachable");
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
                            ERROR(this->current.start, "Expected a string or integer.");
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

utils::Ref<ast::Expr> Parser::expr(bool semicolon) {
    auto left = this->unary();
    auto expr = this->binary(0, std::move(left));
    
    if (semicolon) {
        this->end();
    }

    return expr;
}

utils::Ref<ast::Expr> Parser::binary(int prec, utils::Ref<ast::Expr> left) {
    while (true) {
        Location start = this->current.start;
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
            left = utils::make_ref<ast::InplaceBinaryOpExpr>(
                start, this->current.end, INPLACE_OPERATORS[op], std::move(left), std::move(right)
            );
        } else {
            left = utils::make_ref<ast::BinaryOpExpr>(start, this->current.end, op, std::move(left), std::move(right));
        }
    }
}

utils::Ref<ast::Expr> Parser::unary() {
    if (std::find(UNARY_OPERATORS.begin(), UNARY_OPERATORS.end(), this->current.type) == UNARY_OPERATORS.end()) {
        return this->call();
    }

    Location start = this->current.start;

    TokenKind op = this->current.type;
    this->next();

    auto value = this->call();
    return utils::make_ref<ast::UnaryOpExpr>(start, value->end, op, std::move(value));
}

utils::Ref<ast::Expr> Parser::call() {
    Location start = this->current.start;

    auto expr = this->factor();
    while (this->current == TokenKind::LParen) {
        this->next();

        std::vector<utils::Ref<ast::Expr>> args;
        std::map<std::string, utils::Ref<ast::Expr>> kwargs;

        bool has_kwargs = false;
        if (this->current != TokenKind::RParen) {
            while (true) {
                if (this->current == TokenKind::Identifier && this->peek() == TokenKind::Assign) {
                    std::string name = this->current.value;
                    this->next(); this->next();

                    auto value = this->expr(false);
                    kwargs[name] = std::move(value);

                    has_kwargs = true;
                } else {
                    if (has_kwargs) {
                        ERROR(this->current.start, "Positional arguments must come before keyword arguments");
                    }

                    auto value = this->expr(false);
                    args.push_back(std::move(value));
                }

                if (this->current != TokenKind::Comma) {
                    break;
                }

                this->next();
            }
        }
        
        this->expect(TokenKind::RParen, ")");
        expr = utils::make_ref<ast::CallExpr>(
            start, this->current.end, std::move(expr), std::move(args), std::move(kwargs)
        );
    }

    if (this->current == TokenKind::Inc || this->current == TokenKind::Dec) {
        TokenKind op = this->current.type;
        expr = utils::make_ref<ast::UnaryOpExpr>(start, this->current.end, op, std::move(expr));

        this->next();
    } else if (this->current == TokenKind::LBrace) {
        this->next();
        std::vector<ast::ConstructorField> fields;

        bool previous_is_named_field = true;
        while (this->current != TokenKind::RBrace) {
            std::string name;
            if (this->current == TokenKind::Identifier && this->peek() == TokenKind::Colon) {
                if (!previous_is_named_field) {
                    ERROR(this->current.start, "Expected a field name.");
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
        expr = utils::make_ref<ast::ConstructorExpr>(start, this->current.end, std::move(expr), std::move(fields));
    } else if (this->current == TokenKind::Maybe) {
        this->next();
        expr = utils::make_ref<ast::MaybeExpr>(start, this->current.end, std::move(expr));
    }

    if (this->current == TokenKind::Dot) {
        expr = this->attr(start, std::move(expr));
    } else if (this->current == TokenKind::LBracket) {
        expr = this->element(start, std::move(expr));
    }

    if (this->current.match(TokenKind::Keyword, "as")) {
        this->next(); Location end = this->current.end;
        expr = utils::make_ref<ast::CastExpr>(start, end, std::move(expr), this->parse_type());   
    }

    if (this->current.match(TokenKind::Keyword, "if")) {
        this->next();
        auto condition = this->expr(false);

        if (!this->current.match(TokenKind::Keyword, "else")) {
            ERROR(this->current.start, "Expected 'else' after 'if' in ternary expression");
        }

        this->next();
        auto else_expr = this->expr(false);

        expr = utils::make_ref<ast::TernaryExpr>(
            start, this->current.end, std::move(condition), std::move(expr), std::move(else_expr)
        );
    }

    return expr;
}

utils::Ref<ast::Expr> Parser::attr(Location start, utils::Ref<ast::Expr> expr) {
    while (this->current == TokenKind::Dot) {
        this->next();
        
        std::string value = this->expect(TokenKind::Identifier, "attribute name").value;
        expr = utils::make_ref<ast::AttributeExpr>(start, this->current.end, value, std::move(expr));

        if (this->current == TokenKind::Maybe) {
            this->next();
            expr = utils::make_ref<ast::MaybeExpr>(start, this->current.end, std::move(expr));
        }
    }

    if (this->current == TokenKind::LBracket) {
        return this->element(start, std::move(expr));
    }
    
    return expr;
}

utils::Ref<ast::Expr> Parser::element(Location start, utils::Ref<ast::Expr> expr) {
    while (this->current == TokenKind::LBracket) {
        this->next();
        auto index = this->expr(false);

        this->expect(TokenKind::RBracket, "]");
        expr = utils::make_ref<ast::ElementExpr>(start, this->current.end, std::move(expr), std::move(index));

        if (this->current == TokenKind::Maybe) {
            this->next();
            expr = utils::make_ref<ast::MaybeExpr>(start, this->current.end, std::move(expr));
        }
    }

    if (this->current == TokenKind::Dot) {
        return this->attr(start, std::move(expr));
    }

    return expr;
}

utils::Ref<ast::Expr> Parser::factor() {
    utils::Ref<ast::Expr> expr;

    Location start = this->current.start;
    switch (this->current.type) {
        case TokenKind::Integer: {
            std::string value = this->current.value;
            this->next();

            int bits = 32;
            bool is_float = false;

            if (this->current.match(TokenKind::Identifier, "i8")) {
                bits = 8; this->next();
            } else if (this->current.match(TokenKind::Identifier, "i16")) {
                bits = 16; this->next();
            } else if (this->current.match(TokenKind::Identifier, "i32")) {
                bits = 32; this->next();
            } else if (this->current.match(TokenKind::Identifier, "i64")) {
                bits = 64; this->next();
            } else if (this->current.match(TokenKind::Identifier, "i128")) {
                bits = 128; this->next();
            } else if (this->current.match(TokenKind::Identifier, "f32")) {
                bits = 32; is_float = true; this->next();
            } else if (this->current.match(TokenKind::Identifier, "f64")) {
                bits = 64; is_float = true; this->next();
            }

            // Integers are not callable nor indexable so we can safely return from this function.
            return utils::make_ref<ast::IntegerExpr>(start, this->current.end, value, bits, is_float);
        }
        case TokenKind::Char: {
            std::string value = this->current.value; this->next();
            return utils::make_ref<ast::CharExpr>(start, this->current.start, value[0]);
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

            return utils::make_ref<ast::FloatExpr>(start, this->current.start, result, is_double);
        }
        case TokenKind::String: {
            std::string value = this->current.value;
            this->next();

            expr = utils::make_ref<ast::StringExpr>(start, this->current.start, value);
            break;
        }
        case TokenKind::Identifier: {
            std::string name = this->current.value;
            Location end = this->current.end;

            this->next();
            if (name == "true") {
                return utils::make_ref<ast::IntegerExpr>(start, end, "1", 1);
            } else if (name == "false") {
                return utils::make_ref<ast::IntegerExpr>(start, end, "0", 1);
            }

            expr = utils::make_ref<ast::VariableExpr>(start, this->current.start, name);
            break;
        }
        case TokenKind::Keyword: {
            if (this->current.value == "sizeof") {
                this->next();
                
                this->expect(TokenKind::LParen, "(");
                utils::Ref<ast::Expr> expr = this->expr(false);

                Location end = this->expect(TokenKind::RParen, ")").end;

                // TODO: sizeof for types
                return utils::make_ref<ast::SizeofExpr>(start, end, std::move(expr));
            } else if (this->current.value == "offsetof") {
                Location start = this->current.start;
                this->next();

                this->expect(TokenKind::LParen, "(");
                auto value = this->expr(false);

                this->expect(TokenKind::Comma, ",");
                std::string field = this->expect(TokenKind::Identifier, "identifier").value;

                Location end = this->expect(TokenKind::RParen, ")").end;
                return utils::make_ref<ast::OffsetofExpr>(start, end, std::move(value), field);
            } else if (this->current.value == "where") {
                Location start = this->current.start;
                this->next();

                this->expect(TokenKind::LParen, "(");
                auto expr = this->expr(false);
                Location end = this->expect(TokenKind::RParen, ")").end;

                return utils::make_ref<ast::WhereExpr>(start, end, std::move(expr));
            } else if (this->current.value == "func") {
                bool outer = this->is_inside_function;
                this->is_inside_function = true;

                this->next();

                auto prototype = this->parse_prototype(ast::ExternLinkageSpecifier::None, false, false);
                auto body = this->expr(false);

                this->is_inside_function = outer;
                return utils::make_ref<ast::FunctionExpr>(start, this->current.start, std::move(prototype), std::move(body));
            } else {
                ERROR(this->current.start, "Unexpected keyword."); exit(1);
            }
        }
        case TokenKind::LParen: {
            Location start = this->current.start;
            this->next();

            expr = this->expr(false);
            if (this->current == TokenKind::Comma) {
                std::vector<utils::Ref<ast::Expr>> elements;
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

                Location end = this->expect(TokenKind::RParen, ")").end;
                expr = utils::make_ref<ast::TupleExpr>(start, end, std::move(elements));
            } else {
                this->expect(TokenKind::RParen, ")");
            }
            break;
        }
        case TokenKind::LBracket: {
            this->next();
            std::vector<utils::Ref<ast::Expr>> elements;

            if (this->current != TokenKind::RBracket) {
                auto element = this->expr(false);
                if (this->current == TokenKind::SemiColon) {
                    this->next();
                    auto size = this->expr(false);

                    this->expect(TokenKind::RBracket, "]");
                    return utils::make_ref<ast::ArrayFillExpr>(start, this->current.start, std::move(element), std::move(size));
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

            Location end = this->expect(TokenKind::RBracket, "]").end;
            expr = utils::make_ref<ast::ArrayExpr>(start, end, std::move(elements));
            break;
        }
        case TokenKind::LBrace: {
            this->next();
            expr = this->parse_block();
        }
    }

    while (this->current == TokenKind::DoubleColon) {
        this->next();

        std::string value = this->expect(TokenKind::Identifier, "identifier").value;
        expr = utils::make_ref<ast::NamespaceAttributeExpr>(start, this->current.end, value, std::move(expr));
    }

    if (this->current == TokenKind::Dot) {
        expr = this->attr(start, std::move(expr));
    } else if (this->current == TokenKind::LBracket) {
        expr = this->element(start, std::move(expr));
    }

    return expr;
}