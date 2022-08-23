#include "parser/parser.h"

#include <string>
#include <cstring>

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

Parser::Parser(std::vector<Token> tokens) : tokens(tokens) {
    this->index = 0;
    this->current = this->tokens.front();

    // Populate the hash map with already defined precedences.
    for (auto pair : PRECEDENCES) {
        this->precedences[pair.first] = pair.second;
    }

    this->types = {
        {"void", VoidType},
        {"short", ShortType},
        {"int", IntegerType},
        {"long", LongType},
        {"double", DoubleType},
        {"float", FloatType},
        {"char", CharType},
        {"bool", BooleanType},
    };
}

void Parser::free() {
    for (auto pair : this->types) {
        if (pair.second) {
            delete pair.second;
        }
    }

    for (auto type : this->_allocated_types) {
        if (type) {
            delete type;
        }
    }
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
    if (this->current != type) {
        ERROR(this->current.start, "Expected {s}", value); exit(1);
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

Type* Parser::parse_type(std::string name, bool should_error) {
    Type* type;
    if (this->current == TokenKind::Keyword && this->current.value == "func") {
        // func(int, int) -> int
        this->next();
        this->expect(TokenKind::LParen, "(");

        std::vector<Type*> args;
        while (this->current != TokenKind::RParen) {
            Type* type = this->parse_type(this->current.value);
            args.push_back(type);

            if (this->current != TokenKind::Comma) {
                break;
            }
            
            this->next();
        }

        this->expect(TokenKind::RParen, ")");
        Type* return_type = VoidType;
        
        if (this->current == TokenKind::Arrow) {
            this->next();
            return_type = this->parse_type(this->current.value);
        }

        type = FunctionType::create(args, return_type, false)->getPointerTo();
        this->_allocated_types.push_back(type);
    } else if (this->current == TokenKind::LBracket) {
        this->next();
        Type* element = this->parse_type(this->current.value);

        this->expect(TokenKind::SemiColon, ";");
        int result = this->itoa<int>(this->expect(TokenKind::Integer, "integer").value, "int");

        this->expect(TokenKind::RBracket, "]");
        type = ArrayType::create(result, element);

        this->_allocated_types.push_back(type);
    } else {
        bool skip_token = true;
        if (this->peek() == TokenKind::DoubleColon) {
            // A really silly solution
            this->next(); this->next();
            name += "::" + this->expect(TokenKind::Identifier, "identifier").value;

            while (this->current == TokenKind::DoubleColon) {
                this->next();
                name += "::" + this->expect(TokenKind::Identifier, "identifier").value;
            }

            skip_token = false;
        }

        if (this->types.find(name) == this->types.end()) {
            if (this->current_namespace.hasValue()) {
                name = utils::fmt::format("{s}::{s}", this->current_namespace.getValue(), name);
            }

            if (this->types.find(name) == this->types.end()) {
                if (should_error) {
                    ERROR(this->current.start, "Unknown type {s}", name); exit(1);
                } else {
                    return nullptr;
                }
            }
        }

        type = this->types[name];
        if (skip_token) {
            this->next();
        }
    }

    while (this->current == TokenKind::Mul) {
        type = type->getPointerTo();
        this->_allocated_types.push_back(type);

        this->next();
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

utils::Ref<ast::PrototypeExpr> Parser::parse_prototype(ast::ExternLinkageSpecifier linkage) {
    Location start = this->current.start;

    std::string name = this->expect(TokenKind::Identifier, "function name").value;
    this->expect(TokenKind::LParen, "(");

    std::vector<ast::Argument> args;
    bool has_varargs = false;

    while (this->current != TokenKind::RParen) {
        std::string value = this->current.value;
        if (this->current != TokenKind::Identifier && this->current != TokenKind::Ellipsis) {
            ERROR(this->current.start, "Expected identifier"); exit(1);
        }

        if (this->current == TokenKind::Ellipsis) {
            if (has_varargs) {
                ERROR(this->current.start, "Cannot have multiple varargs"); exit(1);
            }

            has_varargs = true;
            this->next();

            break;
        }


        Type* type;
        std::string argument;
        if (value == "self") {
            if (this->current_struct) {
                argument = "self";
                type = this->current_struct;

                this->next();
            } else {
                this->next();
                this->expect(TokenKind::Colon, ":");

                type = this->parse_type(this->current.value);
                argument = value;
            }
        } else {
            type = this->parse_type(value, false);
            if (!type) {
                this->next();
                this->expect(TokenKind::Colon, ":");

                type = this->parse_type(this->current.value);
                argument = value;
            }
        }

        args.push_back({argument, type});
        if (this->current != TokenKind::Comma) {
            break;
        }

        this->next();
    }

    Location end = this->expect(TokenKind::RParen, ")").end;
    Type* ret = VoidType;
    if (this->current == TokenKind::Arrow) {
        this->next();

        end = this->current.end;
        ret = this->parse_type(this->current.value);
        
    }

    auto expr = utils::make_ref<ast::PrototypeExpr>(start, end, name, ret, std::move(args), has_varargs);
    expr->linkage_specifier = linkage;

    return expr;
}

utils::Ref<ast::Expr> Parser::parse_function_definition(ast::ExternLinkageSpecifier linkage) {
    auto prototype = this->parse_prototype(linkage);

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
    std::string formatted = name;

    if (this->current_namespace.hasValue()) {
        formatted = utils::fmt::format("{s}::{s}", this->current_namespace.getValue(), name);
    }

    if (this->current == TokenKind::SemiColon) {
        this->next();

        this->types[formatted] = StructType::create(name, {});
        return utils::make_ref<ast::StructExpr>(start, this->current.end, name, true);
    }

    std::vector<utils::Ref<ast::Expr>> parents;

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

    std::map<std::string, ast::StructField> fields;
    std::vector<utils::Ref<ast::Expr>> methods;
    std::vector<Type*> types;

    StructType* structure = StructType::create(name, {});
    this->current_struct = structure;

    this->types[formatted] = structure;
    uint32_t index = 0;

    while (this->current != TokenKind::RBrace) {
        bool is_private = false;

        if (this->current.match(TokenKind::Keyword, "private")) {
            is_private = true;
            this->next();
        }

        if (this->current != TokenKind::Identifier && this->current.match(TokenKind::Keyword, "func")) {
            ERROR(this->current.start, "Expected field name or function definition"); exit(1);
        }

        if (this->current.value == "func") {
            this->next();

            auto definition = this->parse_function_definition();
            if (is_private) {
                definition->attributes.add("private");
            }

            methods.push_back(std::move(definition));
        } else {
            if (index == UINT32_MAX) {
                ERROR(this->current.start, "Cannot define more than {i} fields inside a structure", UINT32_MAX); exit(1);
            }

            std::string name = this->current.value;
            this->next();

            this->expect(TokenKind::Colon, ":");
            types.push_back(this->parse_type(this->current.value));

            fields[name] = {name, types.back(), index, is_private};

            this->expect(TokenKind::SemiColon, ";");
            index++;
        }
    }

    Location end = this->expect(TokenKind::RBrace, "}").end;
    structure->setFields(types);

    this->current_struct = nullptr;
    return utils::make_ref<ast::StructExpr>(
        start, end, name, false, std::move(parents), std::move(fields), std::move(methods)
    );    
}

utils::Ref<ast::Expr> Parser::parse_variable_definition(bool is_const) {
    Location start = this->current.start;
    Type* type = nullptr;

    std::string name = this->expect(TokenKind::Identifier, "variable name").value;
    if (this->current == TokenKind::Colon) {
        this->next();
        type = this->parse_type(this->current.value);
    }
    
    utils::Ref<ast::Expr> expr = nullptr;
    Location end;

    if (this->current != TokenKind::Assign) {
        end = this->expect(TokenKind::SemiColon, ";").end;

        if (!type) {
            ERROR(this->current.start, "Un-initialized variables must have an inferred type"); exit(1);
        }
    } else {
        this->next();
        expr = this->expr(false);
        
        this->expect(TokenKind::SemiColon, ";");
        end = expr->end;
    };
    
    if (is_const) {
        if (!expr) {
            ERROR(this->current.start, "Constants must have an initializer"); exit(1);
        }

        return utils::make_ref<ast::ConstExpr>(start, end, name, type, std::move(expr));
    } else {
        return utils::make_ref<ast::VariableAssignmentExpr>(start, end, name, type, std::move(expr));
    }
}

utils::Ref<ast::NamespaceExpr> Parser::parse_namespace() {
    Location start = this->current.start;
    std::string name = this->expect(TokenKind::Identifier, "namespace name").value;

    if (this->current_namespace.hasValue()) {
        this->current_namespace = utils::fmt::format("{s}::{s}", this->current_namespace.getValue(), name);
    } else {
        this->current_namespace = name;
    }

    this->expect(TokenKind::LBrace, "{");
    
    std::vector<utils::Ref<ast::Expr>> members;
    while (this->current != TokenKind::RBrace) {
        ast::Attributes attrs = this->parse_attributes();

        if (!this->current.match(TokenKind::Keyword, NAMESPACE_ALLOWED_KEYWORDS)) {
            ERROR(this->current.start, "Expected function, extern, struct, const, type, or namespace"); exit(1);
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
            std::string name = this->current.value;
            if (this->current_namespace.hasValue()) {
                name = utils::fmt::format("{s}::{s}", this->current_namespace.getValue(), name);
            }

            this->next();
            this->expect(TokenKind::Assign, "=");

            Type* type = this->parse_type(this->current.value);
            this->expect(TokenKind::SemiColon, ";");

            this->types[name] = type;
            continue;
        }

        member->attributes.update(attrs);
        members.push_back(std::move(member));
    }

    Location end = this->expect(TokenKind::RBrace, "}").end;
    this->current_namespace.reset();
    
    return utils::make_ref<ast::NamespaceExpr>(start, end, name, std::move(members));
}

utils::Ref<ast::Expr> Parser::parse_extern(ast::ExternLinkageSpecifier linkage) {
    utils::Ref<ast::Expr> definition;
    Location start = this->current.start;
    
    ast::Attributes attrs = this->parse_attributes();
    if (this->current == TokenKind::Identifier) {
        std::string name = this->current.value;
        this->next();

        this->expect(TokenKind::Colon, ":");
        Type* type = this->parse_type(this->current.value);

        if (this->current == TokenKind::Assign) {
            ERROR(this->current.start, "External variables cannot have an initializer"); exit(1);
        }

        Location end = this->expect(TokenKind::SemiColon, ";").end;
        definition = utils::make_ref<ast::VariableAssignmentExpr>(start, end, name, type, nullptr, true);
    } else {
        if (this->current != TokenKind::Keyword && this->current.value != "func") {
            ERROR(this->current.start, "Expected function"); exit(1);
        }

        this->next();
        definition = this->parse_function_definition(linkage);
    }

    definition->attributes.update(attrs);
    return definition;
}

utils::Ref<ast::Expr> Parser::parse_extern_block() {
    Location start = this->current.start;
    ast::ExternLinkageSpecifier linkage = ast::ExternLinkageSpecifier::None;

    if (this->current == TokenKind::String) {
        if (this->current.value != "C") {
            ERROR(this->current.start, "Unsupported extern linkage specifier"); exit(1);
        }

        // TODO: Name mangling.

        // For now just ignore if "C" is used because from what I know if it's used like 
        // `extern "C" void foo();` in C++, the name of the function doesn't get mangled unlike if used without the "C".
        // Right now, I don't do name mangling so I'll just ignore it.
        this->next();
        linkage = ast::ExternLinkageSpecifier::C;
    }

    std::vector<utils::Ref<ast::Expr>> definitions;
    if (this->current == TokenKind::LBrace) {
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
                    ERROR(this->current.start, "Return statement outside of function"); exit(1);
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
                    ERROR(this->current.start, "If statement outside of function"); exit(1);
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
                    ERROR(this->current.start, "Struct definition inside function"); exit(1);
                }

                this->next();
                return this->parse_struct();
            } else if (this->current.value == "namespace") {
                this->next();
                return this->parse_namespace();
            } else if (this->current.value == "type") {
                this->next();
                std::string name = this->current.value;
                if (this->current_namespace.hasValue()) {
                    std::string current = this->current_namespace.getValue();
                    name = utils::fmt::format("{s}::{s}", current, name);
                }

                this->next();
                this->expect(TokenKind::Assign, "=");
    
                Type* type = this->parse_type(this->current.value);
                this->expect(TokenKind::SemiColon, ";");
    
                this->types[name] = type;
                return nullptr;
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
                    ERROR(this->current.start, "Break statement outside of loop"); exit(1);
                }

                Location start = this->current.start;
                this->next();

                Location end = this->expect(TokenKind::SemiColon, ";").end;
                return utils::make_ref<ast::BreakExpr>(start, end);
            } else if (this->current.value == "continue") {
                if (!this->is_inside_loop) {
                    ERROR(this->current.start, "Continue statement outside of loop"); exit(1);
                }

                Location start = this->current.start;
                this->next();

                Location end = this->expect(TokenKind::SemiColon, ";").end;
                return utils::make_ref<ast::ContinueExpr>(start, end);
            } else if (this->current.value == "sizeof") {
                Location start = this->current.start;
                this->next();
                
                this->expect(TokenKind::LParen, "(");
                Type* type = this->parse_type(this->current.value, false);

                utils::Ref<ast::Expr> expr = nullptr;
                if (!type) {
                    expr = this->expr(false);
                }

                Location end = this->expect(TokenKind::RParen, ")").end;
                return utils::make_ref<ast::SizeofExpr>(start, end, type, std::move(expr));
            } else if (this->current.value == "offsetof") {
                Location start = this->current.start;
                this->next();

                this->expect(TokenKind::LParen, "(");
                auto value = this->expr(false);

                this->expect(TokenKind::Comma, ",");
                std::string field = this->expect(TokenKind::Identifier, "identifier").value;

                Location end = this->expect(TokenKind::RParen, ")").end;
                return utils::make_ref<ast::OffsetofExpr>(start, end, std::move(value), field);
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
                    ERROR(this->current.start, "Expected 'from' keyword."); exit(1);
                }

                this->next();
                auto parent = this->expr();

                return utils::make_ref<ast::UsingExpr>(start, this->current.end, members, std::move(parent));
            } else if (this->current.value == "defer") {
                if (!this->is_inside_function) {
                    ERROR(this->current.start, "Defer statement outside of function"); exit(1);
                }

                Location start = this->current.start;
                this->next();

                auto expr = this->expr();
                return utils::make_ref<ast::DeferExpr>(start, this->current.end, std::move(expr));
            } else {
                ERROR(this->current.start, "Expected an expression"); exit(1);
            }

            break;
        case TokenKind::SemiColon:
            this->next();
            return nullptr;
        default:
            return this->expr();
    }

    ERROR(this->current.start, "Unreachable"); exit(1);
    return nullptr;
}

utils::Ref<ast::Expr> Parser::parse_immediate_binary_op(utils::Ref<ast::Expr> right, utils::Ref<ast::Expr> left, TokenKind op) {
    if (left->kind == ast::ExprKind::String) {
        if (right->kind != ast::ExprKind::String) {
            ERROR(this->current.start, "Expected string.");
        }

        ast::StringExpr* lhs = left->cast<ast::StringExpr>();
        ast::StringExpr* rhs = right->cast<ast::StringExpr>();

        std::string result;

        switch (op) {
            case TokenKind::Add:
                result = lhs->value + rhs->value; break;
            case TokenKind::Eq: 
                return utils::make_ref<ast::IntegerExpr>(
                    this->current.start, this->current.end, lhs->value == rhs->value, 1
                );
            case TokenKind::Neq: {
                result = lhs->value != rhs->value;
                return utils::make_ref<ast::IntegerExpr>(
                    this->current.start, this->current.end, lhs->value != rhs->value, 1
                );
            }
            default:
                ERROR(
                    this->current.start, 
                    "Unsupported binary operator '{s}' for types 'char*' and 'char*'.",
                    Token::getTokenTypeValue(op)
                ); exit(1);
        }

        return utils::make_ref<ast::StringExpr>(this->current.start, this->current.end, result);
    }
    
    ast::IntegerExpr* lhs = left->cast<ast::IntegerExpr>();
    ast::IntegerExpr* rhs = right->cast<ast::IntegerExpr>();

    long result;
    switch (op) {
        case TokenKind::Add:
            result = lhs->value + rhs->value; break;
        case TokenKind::Minus:
            result = lhs->value - rhs->value; break;
        case TokenKind::Mul:
            result = lhs->value * rhs->value; break;
        case TokenKind::Div:
            result = lhs->value / rhs->value; break;
        case TokenKind::And:
            result = lhs->value || rhs->value; break;
        case TokenKind::Or:
            result = lhs->value && rhs->value; break;
        case TokenKind::BinaryAnd:
            result = lhs->value | rhs->value; break;
        case TokenKind::BinaryOr:
            result = lhs->value & rhs->value; break;
        case TokenKind::Xor:
            result = lhs->value ^ rhs->value; break;
        case TokenKind::Lsh:
            result = lhs->value << rhs->value; break;
        case TokenKind::Rsh:
            result = lhs->value >> rhs->value; break;
        case TokenKind::Eq:
            result = lhs->value == rhs->value; break;
        case TokenKind::Neq:
            result = lhs->value != rhs->value; break;
        case TokenKind::Gt:
            result = lhs->value > rhs->value; break;
        case TokenKind::Lt:
            result = lhs->value < rhs->value; break;
        case TokenKind::Gte:
            result = lhs->value >= rhs->value; break;
        case TokenKind::Lte:
            result = lhs->value <= rhs->value; break;
        default:
            ERROR(
                this->current.start, 
                "Unsupported binary operator '{s}' for types 'int' and 'int'", 
                Token::getTokenTypeValue(op)
            );
    }

    return utils::make_ref<ast::IntegerExpr>(this->current.start, this->current.end, result);
}

utils::Ref<ast::Expr> Parser::parse_immediate_unary_op(utils::Ref<ast::Expr> expr, TokenKind op) {
    if (op == TokenKind::BinaryAnd) {
        // We look for cases like `&(*ptr)`
        if (expr->kind == ast::ExprKind::UnaryOp) {
            ast::UnaryOpExpr* unary = expr->cast<ast::UnaryOpExpr>();
            if (unary->op == TokenKind::Mul) {
                return std::move(unary->value);
            }
        }

        return utils::make_ref<ast::UnaryOpExpr>(this->current.start, this->current.end, op, std::move(expr));
    } else if (op == TokenKind::Mul) {
        // Similar to the case above, now we look for `*(&value)`
        if (expr->kind == ast::ExprKind::UnaryOp) {
            ast::UnaryOpExpr* unary = expr->cast<ast::UnaryOpExpr>();
            if (unary->op == TokenKind::And) {
                return std::move(unary->value);
            }
        }

        return utils::make_ref<ast::UnaryOpExpr>(this->current.start, this->current.end, op, std::move(expr));
    }

    ast::IntegerExpr* lhs = (ast::IntegerExpr*)expr.get();
    long result = 0;
    
    switch (op) {
        case TokenKind::Not:
            result = !lhs->value; break;
        case TokenKind::BinaryNot:
            result = ~lhs->value; break;
        case TokenKind::Minus:
            result = -lhs->value; break;
        case TokenKind::Add:
            result = lhs->value; break;
        case TokenKind::Mul:
            ERROR(expr->start, "Unsupported unary operator '*' for type 'int'");
        case TokenKind::And:
            ERROR(expr->start, "Unsupported unary operator '&' for type 'int'");
    }

    return utils::make_ref<ast::IntegerExpr>(this->current.start, this->current.end, result);
}

ast::Attributes Parser::parse_attributes() {
    ast::Attributes attrs;
    if (this->current == TokenKind::LBracket) {
        this->next();
        if (this->current == TokenKind::LBracket) {
            this->next();

            while (this->current != TokenKind::RBracket) {
                std::string name = this->expect(TokenKind::Identifier, "attribute name").value;
                attrs.add(name);

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

        if (right->kind.in(NUMERIC_KINDS) && left->kind.in(NUMERIC_KINDS)) {
            left = this->parse_immediate_binary_op(std::move(right), std::move(left), op);
        } else {
            if (INPLACE_OPERATORS.find(op) != INPLACE_OPERATORS.end()) {
                left = utils::make_ref<ast::InplaceBinaryOpExpr>(
                    start, this->current.end, INPLACE_OPERATORS[op], std::move(left), std::move(right)
                );
            } else {
                left = utils::make_ref<ast::BinaryOpExpr>(start, this->current.end, op, std::move(left), std::move(right));
            }
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
    if (value->kind.in(NUMERIC_KINDS) || op == TokenKind::Mul || op == TokenKind::BinaryAnd) {
        return this->parse_immediate_unary_op(std::move(value), op);
    }

    return utils::make_ref<ast::UnaryOpExpr>(start, this->current.end, op, std::move(value));
}

utils::Ref<ast::Expr> Parser::call() {
    Location start = this->current.start;

    auto expr = this->factor();
    while (this->current == TokenKind::LParen) {
        this->next();
        std::vector<utils::Ref<ast::Expr>> args;

        if (this->current != TokenKind::RParen) {
            while (true) {
                auto arg = this->expr(false);
                args.push_back(std::move(arg));

                if (this->current != TokenKind::Comma) {
                    break;
                }

                this->next();
            }
        }
        
        this->expect(TokenKind::RParen, ")");
        expr = utils::make_ref<ast::CallExpr>(start, this->current.end, std::move(expr), std::move(args));
    }

    if (this->current == TokenKind::Inc || this->current == TokenKind::Dec) {
        TokenKind op = this->current.type;
        expr = utils::make_ref<ast::UnaryOpExpr>(start, this->current.end, op, std::move(expr));

        this->next();
    } else if (this->current == TokenKind::LBrace) {
        this->next();
        std::map<std::string, utils::Ref<ast::Expr>> fields;

        bool previous_is_named_field = true;
        while (this->current != TokenKind::RBrace) {

            std::string name;
            if (this->current == TokenKind::Identifier) {
                if (!previous_is_named_field) {
                    ERROR(this->current.start, "Expected a field name."); exit(1);
                }

                previous_is_named_field = true;
                name = this->current.value;

                this->next();
                this->expect(TokenKind::Colon, ":");
            } else {
                previous_is_named_field = false;
            }

            auto value = this->expr(false);
            fields[name] = std::move(value);

            if (this->current != TokenKind::Comma) {
                break;
            }

            this->next();
        }

        this->expect(TokenKind::RBrace, "}");
        expr = utils::make_ref<ast::ConstructorExpr>(start, this->current.end, std::move(expr), std::move(fields));
    }

    if (this->current == TokenKind::Dot) {
        expr = this->attr(start, std::move(expr));
    } else if (this->current == TokenKind::LBracket) {
        expr = this->element(start, std::move(expr));
    }

    if (this->current.value == "as") {
        this->next();
        Location end = this->current.end;

        Type* to = this->parse_type(this->current.value);
        expr = utils::make_ref<ast::CastExpr>(start, end, std::move(expr), to);   
    }

    return expr;
}

utils::Ref<ast::Expr> Parser::attr(Location start, utils::Ref<ast::Expr> expr) {
    while (this->current == TokenKind::Dot) {
        this->next();
        
        std::string value = this->expect(TokenKind::Identifier, "attribute name").value;
        expr = utils::make_ref<ast::AttributeExpr>(start, this->current.end, value, std::move(expr));
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

            std::string number = this->current.value;
            this->next();

            if (this->current.match(TokenKind::Identifier, "i8")) {
                this->next();
                return utils::make_ref<ast::IntegerExpr>(
                    start, this->current.start, this->itoa<char>(number, "char"), 8
                );

                this->next();
            }
            
            if (this->current.match(TokenKind::Identifier, "i16")) {
                this->next();
                return utils::make_ref<ast::IntegerExpr>(
                    start, this->current.start, this->itoa<short>(number, "short"), 16
                );
            } 
            
            if (this->current.match(TokenKind::Identifier, "i64")) {
                this->next();
                return utils::make_ref<ast::IntegerExpr>(
                    start, this->current.start, this->itoa<long>(number, "long"), 64
                );
            }


            if (this->current.match(TokenKind::Identifier, "i32")) {
                this->next();
            }
        
            // Integers are not callable nor indexable so we can safely return from this function.
            return utils::make_ref<ast::IntegerExpr>(start, this->current.start, this->itoa<int>(number, "int"), 32);
        }
        case TokenKind::Char: {
            char c = this->current.value[0];
            this->next();

            // Same case for integers.
            return utils::make_ref<ast::IntegerExpr>(start, this->current.start, c, 8);
        }
        case TokenKind::Float: {
            double number;
            bool error = llvm::StringRef(this->current.value).getAsDouble(number);
            if (error) {
                ERROR(this->current.start, "Invalid floating point number."); exit(1);
            }

            this->next();

            // Same case again.
            return utils::make_ref<ast::FloatExpr>(start, this->current.start, number);
        }
        case TokenKind::String: {
            std::string value = this->current.value;
            this->next();

            expr = utils::make_ref<ast::StringExpr>(start, this->current.start, value);
            break;
        }
        case TokenKind::Identifier: {
            std::string name = this->current.value;
            this->next();

            expr = utils::make_ref<ast::VariableExpr>(start, this->current.start, name);
            break;
        }
        case TokenKind::Keyword: {
            if (this->current.value == "sizeof") {
                this->next();
                
                this->expect(TokenKind::LParen, "(");
                Type* type = this->parse_type(this->current.value, false);
                utils::Ref<ast::Expr> expr = nullptr;

                if (!type) {
                    expr = this->expr(false);
                }

                Location end = this->expect(TokenKind::RParen, ")").end;
                return utils::make_ref<ast::SizeofExpr>(start, end, type, std::move(expr));
            } else if (this->current.value == "offsetof") {
                Location start = this->current.start;
                this->next();

                this->expect(TokenKind::LParen, "(");
                auto value = this->expr(false);

                this->expect(TokenKind::Comma, ",");
                std::string field = this->expect(TokenKind::Identifier, "identifier").value;

                Location end = this->expect(TokenKind::RParen, ")").end;
                return utils::make_ref<ast::OffsetofExpr>(start, end, std::move(value), field);
            } else {
                ERROR(this->current.start, "Unexpected keyword."); exit(1);
            }
        }
        case TokenKind::LParen: {
            this->next();
            expr = this->expr(false);

            this->expect(TokenKind::RParen, ")");
            break;
        }
        case TokenKind::LBracket: {
            this->next();

            std::vector<utils::Ref<ast::Expr>> elements;
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