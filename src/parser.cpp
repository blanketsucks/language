#include "parser.h"

#include <string>
#include <cstring>

static std::vector<ast::ExprKind> NUMERIC_KINDS = {
    ast::ExprKind::String,
    ast::ExprKind::Integer,
    ast::ExprKind::Float
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
        {"str", StringType},
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
    this->expect(TokenType::SemiColon, ";");
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

Token Parser::expect(TokenType type, std::string value) {
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
    if (this->current == TokenType::LParen) {
        // (int, int) -> int
        this->next();
        std::vector<Type*> args;

        while (this->current != TokenType::RParen) {
            Type* type = this->parse_type(this->current.value);
            args.push_back(type);

            if (this->current != TokenType::Comma) {
                break;
            }
            
            this->next();
        }

        this->expect(TokenType::RParen, ")");
        Type* return_type = VoidType;
        
        if (this->current == TokenType::Arrow) {
            this->next();
            return_type = this->parse_type(this->current.value);
        }

        type = FunctionType::create(args, return_type, false);
        this->_allocated_types.push_back(type);
    } else if (this->current == TokenType::LBracket) {
        this->next();
        Type* element = this->parse_type(this->current.value);

        this->expect(TokenType::SemiColon, ";");
        int result = this->itoa<int>(this->expect(TokenType::Integer, "integer").value);

        this->expect(TokenType::RBracket, "]");
        type = ArrayType::create(result, element);

        this->_allocated_types.push_back(type);
    } else {
        if (this->types.find(name) == this->types.end()) {
            if (should_error) {
                ERROR(this->current.start, "Unrecognized type '{s}'", name);
            }
            
            return nullptr;
        }

        type = this->types[name];
        this->next();
    }

    while (this->current == TokenType::Mul) {
        type = type->getPointerTo();
        this->_allocated_types.push_back(type);

        this->next();
    }

    return type;
} 

std::unique_ptr<ast::BlockExpr> Parser::parse_block() {
    std::vector<std::unique_ptr<ast::Expr>> body;
    Location start = this->current.start;

    while (this->current != TokenType::RBrace) {
        ast::Attributes attrs = this->parse_attributes();
        auto expr = this->statement();

        if (!expr) {
            continue;
        }

        expr->attributes = attrs;
        body.push_back(std::move(expr));
    }

    Location end = this->expect(TokenType::RBrace, "}").end;
    return std::make_unique<ast::BlockExpr>(start, end, std::move(body));
}

std::unique_ptr<ast::PrototypeExpr> Parser::parse_prototype(ast::ExternLinkageSpecifier linkage) {
    Location start = this->current.start;

    std::string name = this->expect(TokenType::Identifier, "function name").value;
    this->expect(TokenType::LParen, "(");

    std::vector<ast::Argument> args;
    bool has_varargs = false;

    while (this->current != TokenType::RParen) {
        std::string value = this->current.value;
        if (this->current != TokenType::Identifier && this->current != TokenType::Ellipsis) {
            ERROR(this->current.start, "Expected identifier"); exit(1);
        }

        if (this->current == TokenType::Ellipsis) {
            if (has_varargs) {
                ERROR(this->current.start, "Cannot have multiple varargs"); exit(1);
            }

            has_varargs = true;
            this->next();

            break;
        }

        Type* type = this->parse_type(value, false);
        std::string argument;

        if (!type) {
            this->next();
            this->expect(TokenType::Colon, ":");

            type = this->parse_type(this->current.value);
            argument = value;
        }

        args.push_back({argument, type});
        if (this->current != TokenType::Comma) {
            break;
        }

        this->next();
    }

    Location end = this->expect(TokenType::RParen, ")").end;
    Type* ret = VoidType;
    if (this->current == TokenType::Arrow) {
        this->next();

        end = this->current.end;
        ret = this->parse_type(this->current.value);
        
    }

    auto expr = std::make_unique<ast::PrototypeExpr>(start, end, name, ret, std::move(args), has_varargs);
    expr->linkage_specifier = linkage;

    return expr;
}

std::unique_ptr<ast::Expr> Parser::parse_function_definition(ast::ExternLinkageSpecifier linkage) {
    auto prototype = this->parse_prototype(linkage);

    if (this->current == TokenType::SemiColon) {
        this->next();
        return prototype;
    }

    this->expect(TokenType::LBrace, "{");
    this->context.is_inside_function = true;

    auto body = this->parse_block();
    this->context.is_inside_function = false;

    return std::make_unique<ast::FunctionExpr>(prototype->start, body->end, std::move(prototype), std::move(body));
}

std::unique_ptr<ast::IfExpr> Parser::parse_if_statement() {
    Location start = this->current.start;
    std::unique_ptr<ast::Expr> condition = this->expr(false);

    this->expect(TokenType::LBrace, "{");
    auto body = this->parse_block();
    
    Location end = body->end;
    this->next();

    std::unique_ptr<ast::BlockExpr> else_body = nullptr;
    if (this->current == TokenType::Keyword && this->current.value == "else") {
        this->next();
        this->expect(TokenType::LBrace, "{");

        else_body = this->parse_block();
        end = else_body->end;
    }

    return std::make_unique<ast::IfExpr>(start, end, std::move(condition), std::move(body), std::move(else_body));
}

std::unique_ptr<ast::StructExpr> Parser::parse_struct() {
    Location start = this->current.start;
    bool packed = false;
    if (this->current == TokenType::Keyword && this->current.value == "packed") {
        packed = true;
        this->next();
    }
    
    if (this->current != TokenType::Identifier) {
        ERROR(this->current.start, "Expected struct name"); exit(1);
    }

    std::string name = this->current.value;
    this->next();

    if (this->current == TokenType::SemiColon) {
        this->next();

        this->types[name] = StructType::create(name, {});
        return std::make_unique<ast::StructExpr>(start, this->current.end, name, packed, true);
    }

    if (this->current != TokenType::LBrace) {
        ERROR(this->current.start, "Expected {"); exit(1);
    }

    this->next();
    std::map<std::string, ast::Argument> fields;
    std::vector<std::unique_ptr<ast::Expr>> methods;
    std::vector<Type*> types;

    StructType* structure = StructType::create(name, {});
    this->context.current_struct = structure;

    this->types[name] = structure;
    while (this->current != TokenType::RBrace) {
        if (this->current != TokenType::Identifier && (this->current != TokenType::Keyword && this->current.value != "func")) {
            ERROR(this->current.start, "Expected field name or function definition"); exit(1);
        }

        if (this->current.value == "func") {
            this->next();

            auto definition = this->parse_function_definition();
            methods.push_back(std::move(definition));
        } else {
            std::string name = this->current.value;
            this->next();

            if (this->current != TokenType::Colon) {
                ERROR(this->current.start, "Expected :"); exit(1);
            }

            this->next();
            types.push_back(this->parse_type(this->current.value));

            fields[name] = {name, types.back()};
            if (this->current != TokenType::SemiColon) {
                ERROR(this->current.start, "Expected ;"); exit(1);
            }

            this->next();
        }
    }

    if (this->current != TokenType::RBrace) {
        ERROR(this->current.start, "Expected }"); exit(1);
    }

    Location end = this->current.end;
    structure->setFields(types);

    this->next();
    return std::make_unique<ast::StructExpr>(start, end, name, packed, false, std::move(fields), std::move(methods));    
}

std::unique_ptr<ast::Expr> Parser::parse_variable_definition(bool is_const) {
    Location start = this->current.start;

    if (this->current != TokenType::Identifier) {
        ERROR(this->current.start, "Expected variable name"); exit(1);
    }

    Type* type = nullptr;
    std::string name = this->current.value;
    this->next();

    if (this->current == TokenType::Colon) {
        this->next();
        type = this->parse_type(this->current.value);
    }
    
    std::unique_ptr<ast::Expr> expr = nullptr;
    if (this->current != TokenType::Assign) {
        if (this->current != TokenType::SemiColon) {
            ERROR(this->current.start, "Expected = or ;"); exit(1);
        }

        if (!type) {
            ERROR(this->current.start, "Un-initialized variables must have an inferred type"); exit(1);
        }
    } else {
        this->next();
        expr = this->expr(false);
        
        if (this->current != TokenType::SemiColon) {
            ERROR(this->current.start, "Expected ;"); exit(1);
        }
    }

    Location end = this->current.end;
    this->next();

    if (is_const) {
        if (!expr) {
            ERROR(this->current.start, "Constants must have an initializer"); exit(1);
        }

        return std::make_unique<ast::ConstExpr>(start, end, name, type, std::move(expr));
    } else {
        return std::make_unique<ast::VariableAssignmentExpr>(start, end, name, type, std::move(expr));
    }
}

std::unique_ptr<ast::NamespaceExpr> Parser::parse_namespace() {
    Location start = this->current.start;
    if (this->current != TokenType::Identifier) {
        ERROR(this->current.start, "Expected namespace name"); exit(1);
    }

    std::string name = this->current.value;
    std::cout << "Parsing namespace " << name << std::endl;

    this->next();
    if (this->current != TokenType::LBrace) {
        ERROR(this->current.start, "Expected {"); exit(1);
    }

    this->next();

    std::vector<std::unique_ptr<ast::Expr>> members;

    while (this->current != TokenType::RBrace) {
        ast::Attributes attrs = this->parse_attributes();
        if (
            this->current != TokenType::Keyword || (this->current.value != "func" && this->current.value != "extern" && this->current.value != "struct" && this->current.value != "const" && this->current.value != "namespace")
        ) {
            ERROR(this->current.start, "Expected function, extern, struct, const, or namespace"); exit(1);
        }

        std::unique_ptr<ast::Expr> member;
        std::cout << "Parsing member " << this->current.value << std::endl;
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
        }

        std::cout << "Parsed member fiwifew" << std::endl;

        member->attributes = attrs;
        members.push_back(std::move(member));
    }

    if (this->current != TokenType::RBrace) {
        ERROR(this->current.start, "Expected }"); exit(1);
    }

    Location end = this->current.end;
    this->next();

    std::cout << "Parsed namespace " << name << std::endl;
    return std::make_unique<ast::NamespaceExpr>(start, end, name, std::move(members));
}

std::unique_ptr<ast::Expr> Parser::parse_extern(ast::ExternLinkageSpecifier linkage) {
    std::unique_ptr<ast::Expr> definition;
    Location start = this->current.start;
    
    ast::Attributes attrs = this->parse_attributes();
    if (this->current == TokenType::Identifier) {
        std::string name = this->current.value;
        this->next();

        if (this->current != TokenType::Colon) {
            ERROR(this->current.start, "Expected :"); exit(1);
        }

        this->next();
        Type* type = this->parse_type(this->current.value);

        if (this->current == TokenType::Assign) {
            ERROR(this->current.start, "External variables cannot have an initializer"); exit(1);
        }

        if (this->current != TokenType::SemiColon) {
            ERROR(this->current.start, "Expected ;"); exit(1);
        }

        this->next();
        definition = std::make_unique<ast::VariableAssignmentExpr>(start, this->current.end, name, type, nullptr, true);
    } else {
        if (this->current != TokenType::Keyword && this->current.value != "func") {
            ERROR(this->current.start, "Expected function"); exit(1);
        }

        this->next();
        definition = this->parse_function_definition(linkage);
    }

    definition->attributes = attrs;
    return definition;
}

std::unique_ptr<ast::BlockExpr> Parser::parse_extern_block() {
    Location start = this->current.start;
    ast::ExternLinkageSpecifier linkage = ast::ExternLinkageSpecifier::None;

    if (this->current == TokenType::String) {
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

    std::vector<std::unique_ptr<ast::Expr>> definitions;
    if (this->current == TokenType::LBrace) {
        this->next();

        while (this->current != TokenType::RBrace) {
            auto definition = this->parse_extern(linkage);
            definitions.push_back(std::move(definition));
        }

        Location end = this->current.end;
        this->next();

        return std::make_unique<ast::BlockExpr>(start, end, std::move(definitions));
    }

    auto definition = this->parse_extern(linkage);
    definitions.push_back(std::move(definition));

    return std::make_unique<ast::BlockExpr>(start, this->current.end, std::move(definitions));
}

std::unique_ptr<ast::InlineAssemblyExpr> Parser::parse_inline_assembly() {
    Location start = this->current.start;
    if (this->current != TokenType::LParen) {
        ERROR(this->current.start, "Expected ("); exit(1);
    }

    this->next();
    if (this->current != TokenType::String) {
        ERROR(this->current.start, "Expected string"); exit(1);
    }

    std::string assembly = this->current.value;
    this->next();

    ast::InlineAssemblyConstraint inputs;
    ast::InlineAssemblyConstraint outputs;
    std::vector<std::string> clobbers;

    if (this->current == TokenType::Colon) {
        this->next();
        auto parse_constraint = [this](
            ast::InlineAssemblyConstraint& constraints
        ) {
            while (this->current != TokenType::Colon && this->current != TokenType::RParen) {
                if (this->current != TokenType::String) {
                    ERROR(this->current.start, "Expected string"); exit(1);
                }

                std::string value = this->current.value;
                this->next();

                if (this->current != TokenType::LParen) {
                    ERROR(this->current.start, "Expected ("); exit(1);
                }

                this->next();
                auto expr = this->expr(false);

                if (this->current != TokenType::RParen) {
                    ERROR(this->current.start, "Expected )"); exit(1);
                }

                this->next();

                auto pair = std::make_pair(value, expr.release());
                constraints.push_back(pair);

                if (this->current != TokenType::Comma) {
                    break;
                }

                this->next();
            }

            if (this->current != TokenType::Colon && this->current != TokenType::RParen) {
                ERROR(this->current.start, "Expected : or )"); exit(1);
            }
        };

        parse_constraint(inputs);
        if (this->current == TokenType::RParen) {
            this->next();
            if (this->current != TokenType::SemiColon) {
                ERROR(this->current.start, "Expected ;"); exit(1);
            }

            return std::make_unique<ast::InlineAssemblyExpr>(start, this->current.end, assembly, std::move(inputs), std::move(outputs), clobbers);
        }

        this->next();
        parse_constraint(outputs);
        if (this->current == TokenType::RParen) {
            this->next();

            this->expect(TokenType::SemiColon, ";");
            return std::make_unique<ast::InlineAssemblyExpr>(start, this->current.end, assembly, std::move(inputs), std::move(outputs), clobbers);
        }

        while (this->current != TokenType::RParen) {
            std::string value = this->expect(TokenType::String, "string").value;
            this->next();

            clobbers.push_back(value);

            if (this->current != TokenType::Comma) {
                break;
            }
        }
    }
    
    this->expect(TokenType::RParen, ")");
    this->expect(TokenType::SemiColon, ";");

    return std::make_unique<ast::InlineAssemblyExpr>(start, this->current.end, assembly, std::move(inputs), std::move(outputs), clobbers);

}

std::unique_ptr<ast::Program> Parser::statements() {
    std::vector<std::unique_ptr<ast::Expr>> statements;

    while (this->current != TokenType::EOS) {
        ast::Attributes attrs = this->parse_attributes();
        auto expr = this->statement();

        if (!expr) {
            continue;
        }

        expr->attributes = attrs;
        statements.push_back(std::move(expr));
    }

    return std::make_unique<ast::Program>(std::move(statements));
}

std::unique_ptr<ast::Expr> Parser::statement() {
    switch (this->current.type) {
        case TokenType::Keyword:
            if (this->current.value == "extern") {
                this->next();
                return this->parse_extern_block();
            } else if (this->current.value == "func") {
                this->next();
                return this->parse_function_definition();
            } else if (this->current.value == "return") {
                if (!this->context.is_inside_function) {
                    ERROR(this->current.start, "Return statement outside of function"); exit(1);
                }

                this->next();
                Location start = this->current.start;
                if (this->current == TokenType::SemiColon) {
                    this->next();
                    return std::make_unique<ast::ReturnExpr>(start, this->current.end, nullptr);
                }

                auto expr = this->expr(false);
                Location end = this->current.end;

                this->expect(TokenType::SemiColon, ";");
                return std::make_unique<ast::ReturnExpr>(start, end, std::move(expr));
            } else if (this->current.value == "if") {
                if (!this->context.is_inside_function) {
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
                if (this->context.is_inside_function) {
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

                this->next();
                this->expect(TokenType::LParen, ")");
    
                Type* type = this->parse_type(this->current.value);
                this->expect(TokenType::SemiColon, ";");
    
                this->types[name] = type;
                return nullptr;
            } else if (this->current.value == "while") {
                this->next();
                this->expect(TokenType::LParen, "(");
                
                auto condition = this->expr(false);
                this->expect(TokenType::RParen, ")");

                this->expect(TokenType::LBrace, "{");
                auto body = this->parse_block();

                return std::make_unique<ast::WhileExpr>(this->current.start, this->current.end, std::move(condition), std::move(body));
            } else if (this->current.value == "sizeof") {
                Location start = this->current.start;
                this->next();
                
                this->expect(TokenType::LParen, "(");
                Type* type = this->parse_type(this->current.value);

                Location end = this->expect(TokenType::RParen, ")").end;
                return std::make_unique<ast::SizeofExpr>(start, end, type);
            } else if (this->current.value == "static_assert") {
                Location start = this->current.start;
                this->next();

                this->expect(TokenType::LParen, "(");
                auto condition = this->expr(false);

                std::string message;
                if (this->current == TokenType::Comma) {
                    this->next();
                    std::cout << this->current.value << '\n';
                    message = this->expect(TokenType::String, "string").value;
                }

                this->expect(TokenType::RParen, ")");
                this->expect(TokenType::SemiColon, ";");

                if (condition->kind != ast::ExprKind::Integer) {
                    ERROR(start, "Static assert condition must be an integeral constant.");
                }

                ast::IntegerExpr* expr = condition->cast<ast::IntegerExpr>();
                if (expr->value == 0) {
                    if (message.empty()) {
                        ERROR(start, "Static assertion failed.");
                    } else {
                        ERROR(start, "Static assertion failed. {s}", message);
                    }
                }

                return nullptr;
            } else if (this->current.value == "asm") {
                this->next();
                return this->parse_inline_assembly();
            } else {
                ERROR(this->current.start, "Unknown keyword '{s}'", this->current.value); exit(1);
            }

            break;
        default:
            return this->expr();
    }

    ERROR(this->current.start, "Unreachable"); exit(1);
    return nullptr;
}

std::unique_ptr<ast::Expr> Parser::parse_immediate_binary_op(std::unique_ptr<ast::Expr> right, std::unique_ptr<ast::Expr> left, TokenType op) {
    if (left->kind == ast::ExprKind::String) {
        if (right->kind != ast::ExprKind::String) {
            ERROR(this->current.start, "Expected string.");
        }

        ast::StringExpr* lhs = left->cast<ast::StringExpr>();
        ast::StringExpr* rhs = right->cast<ast::StringExpr>();

        std::string result;

        switch (op) {
            case TokenType::Add:
                result = lhs->value + rhs->value; break;
            case TokenType::Eq:
                result = lhs->value == rhs->value ? "true" : "false";
                return std::make_unique<ast::VariableExpr>(this->current.start, this->current.end, result);
            case TokenType::Neq:
                result = lhs->value != rhs->value ? "true" : "false";
                return std::make_unique<ast::VariableExpr>(this->current.start, this->current.end, result);
            default:
                ERROR(this->current.start, "Unimplemented binary operator."); exit(1);
        }

        return std::make_unique<ast::StringExpr>(this->current.start, this->current.end, result);
    }
    
    ast::IntegerExpr* lhs = left->cast<ast::IntegerExpr>();
    ast::IntegerExpr* rhs = right->cast<ast::IntegerExpr>();

    long result;
    switch (op) {
        case TokenType::Add:
            result = lhs->value + rhs->value; break;
        case TokenType::Minus:
            result = lhs->value - rhs->value; break;
        case TokenType::Mul:
            result = lhs->value * rhs->value; break;
        case TokenType::Div:
            result = lhs->value / rhs->value; break;
        case TokenType::And:
            result = lhs->value || rhs->value; break;
        case TokenType::Or:
            result = lhs->value && rhs->value; break;
        case TokenType::BinaryAnd:
            result = lhs->value | rhs->value; break;
        case TokenType::BinaryOr:
            result = lhs->value & rhs->value; break;
        case TokenType::Xor:
            result = lhs->value ^ rhs->value; break;
        case TokenType::Lsh:
            result = lhs->value << rhs->value; break;
        case TokenType::Rsh:
            result = lhs->value >> rhs->value; break;
        case TokenType::Eq:
            result = lhs->value == rhs->value; break;
        case TokenType::Neq:
            result = lhs->value != rhs->value; break;
        case TokenType::Gt:
            result = lhs->value > rhs->value; break;
        case TokenType::Lt:
            result = lhs->value < rhs->value; break;
        case TokenType::Gte:
            result = lhs->value >= rhs->value; break;
        case TokenType::Lte:
            result = lhs->value <= rhs->value; break;
        default:
            ERROR(this->current.start, "Unimplemented binary operator."); exit(1);
    }

    return std::make_unique<ast::IntegerExpr>(this->current.start, this->current.end, result);
}

std::unique_ptr<ast::Expr> Parser::parse_immediate_unary_op(std::unique_ptr<ast::Expr> expr, TokenType op) {
    if (op == TokenType::BinaryAnd) {
        // We look for cases like `&(*ptr)`
        if (expr->kind == ast::ExprKind::UnaryOp) {
            ast::UnaryOpExpr* unary = expr->cast<ast::UnaryOpExpr>();
            if (unary->op == TokenType::Mul) {
                return std::move(unary->value);
            }
        }

        return std::make_unique<ast::UnaryOpExpr>(this->current.start, this->current.end, op, std::move(expr));
    } else if (op == TokenType::Mul) {
        // Similar to the case above, now we look for `*(&value)`
        if (expr->kind == ast::ExprKind::UnaryOp) {
            ast::UnaryOpExpr* unary = expr->cast<ast::UnaryOpExpr>();
            if (unary->op == TokenType::And) {
                return std::move(unary->value);
            }
        }

        return std::make_unique<ast::UnaryOpExpr>(this->current.start, this->current.end, op, std::move(expr));
    }


    ERROR(this->current.start, "Unimplemented unary operator."); exit(1);
    return nullptr;
}

ast::Attributes Parser::parse_attributes() {
    ast::Attributes attrs;
    if (this->current == TokenType::LBracket) {
        this->next();
        if (this->current == TokenType::LBracket) {
            this->next();

            while (this->current != TokenType::RBracket) {
                std::string name = this->expect(TokenType::Identifier, "attribute name").value;
                attrs.add(name);

                if (this->current != TokenType::Comma) {
                    break;
                }

                this->next();
            }

            this->expect(TokenType::RBracket, "]");
            this->expect(TokenType::RBracket, "]");
        }
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
        Location start = this->current.start;
        int precedence = this->get_token_precendence();

        if (precedence < prec) {
            return left;
        }

        TokenType op = this->current.type;
        this->next();

        auto right = this->unary();

        int next = this->get_token_precendence();
        if (precedence < next) {
            right = this->binary(precedence + 1, std::move(right));
        }

        if (right->kind.in(NUMERIC_KINDS) && left->kind.in(NUMERIC_KINDS)) {
            left = this->parse_immediate_binary_op(std::move(right), std::move(left), op);
        } else {
            left = std::make_unique<ast::BinaryOpExpr>(start, this->current.end, op, std::move(left), std::move(right));
        }
    }
}

std::unique_ptr<ast::Expr> Parser::unary() {
    if (std::find(UNARY_OPERATORS.begin(), UNARY_OPERATORS.end(), this->current.type) == UNARY_OPERATORS.end()) {
        return this->call();
    }

    Location start = this->current.start;

    TokenType op = this->current.type;
    this->next();

    auto value = this->call();
    if (value->kind.in(NUMERIC_KINDS) || op == TokenType::Mul || op == TokenType::BinaryAnd) {
        return this->parse_immediate_unary_op(std::move(value), op);
    }

    return std::make_unique<ast::UnaryOpExpr>(start, this->current.end, op, std::move(value));
}

std::unique_ptr<ast::Expr> Parser::call() {
    Location start = this->current.start;
    auto expr = this->factor();

    if (this->current == TokenType::LParen) {
        this->next();
        std::vector<std::unique_ptr<ast::Expr>> args;

        if (this->current != TokenType::RParen) {
            while (true) {
                auto arg = this->expr(false);
                args.push_back(std::move(arg));

                if (this->current != TokenType::Comma) {
                    break;
                }

                this->next();
            }
        }
        
        this->expect(TokenType::RParen, ")");
        expr = std::make_unique<ast::CallExpr>(start, this->current.end, std::move(expr), std::move(args));
    }

    if (this->current == TokenType::Inc || this->current == TokenType::Dec) {
        TokenType op = this->current.type;
        expr = std::make_unique<ast::UnaryOpExpr>(start, this->current.end, op, std::move(expr));

        this->next();
    } else if (this->current == TokenType::LBrace) {
        this->next();
        std::map<std::string, std::unique_ptr<ast::Expr>> fields;

        bool previous_is_named_field = true;
        while (this->current != TokenType::RBrace) {

            std::string name;
            if (this->current == TokenType::Identifier) {
                if (!previous_is_named_field) {
                    ERROR(this->current.start, "Expected a field name."); exit(1);
                }

                previous_is_named_field = true;
                name = this->current.value;

                this->next();
                this->expect(TokenType::Colon, ":");
            } else {
                previous_is_named_field = false;
            }

            auto value = this->expr(false);
            fields[name] = std::move(value);

            if (this->current != TokenType::Comma) {
                break;
            }

            this->next();
        }

        this->expect(TokenType::RBrace, "}");
        expr = std::make_unique<ast::ConstructorExpr>(start, this->current.end, std::move(expr), std::move(fields));
    }

    if (this->current == TokenType::Dot) {
        expr = this->attr(start, std::move(expr));
    } else if (this->current == TokenType::LBracket) {
        expr = this->element(start, std::move(expr));
    }

    if (this->current.value == "as") {
        this->next();
        Location end = this->current.end;

        Type* to = this->parse_type(this->current.value);
        expr = std::make_unique<ast::CastExpr>(start, end, std::move(expr), to);   
    }

    return expr;
}

std::unique_ptr<ast::Expr> Parser::attr(Location start, std::unique_ptr<ast::Expr> expr) {
    while (this->current == TokenType::Dot) {
        this->next();
        std::string value = this->expect(TokenType::Identifier, "attribute name").value;

        expr = std::make_unique<ast::AttributeExpr>(start, this->current.end, value, std::move(expr));
    }

    if (this->current == TokenType::LBracket) {
        return this->element(start, std::move(expr));
    }

    return expr;
}

std::unique_ptr<ast::Expr> Parser::element(Location start, std::unique_ptr<ast::Expr> expr) {
    while (this->current == TokenType::LBracket) {
        this->next();
        auto index = this->expr(false);

        this->expect(TokenType::RBracket, "]");
        expr = std::make_unique<ast::ElementExpr>(start, this->current.end, std::move(expr), std::move(index));
    }

    if (this->current == TokenType::Dot) {
        return this->attr(start, std::move(expr));
    }

    return expr;
}

std::unique_ptr<ast::Expr> Parser::factor() {
    std::unique_ptr<ast::Expr> expr;

    Location start = this->current.start;
    switch (this->current.type) {
        case TokenType::Integer: {
            int number = this->itoa<int>(this->current.value);
            this->next();

            expr = std::make_unique<ast::IntegerExpr>(start, this->current.start, number);
            break;
        }
        case TokenType::Char: {
            char c = this->current.value[0];
            this->next();

            expr = std::make_unique<ast::IntegerExpr>(start, this->current.start, c, 8);
            break;
        }
        case TokenType::Float: {
            double number;
            bool error = llvm::StringRef(this->current.value).getAsDouble(number);
            if (error) {
                ERROR(this->current.start, "Invalid floating point number."); exit(1);
            }

            this->next();

            expr = std::make_unique<ast::FloatExpr>(start, this->current.start, number);
            break;
        }
        case TokenType::String: {
            std::string value = this->current.value;
            this->next();

            expr = std::make_unique<ast::StringExpr>(start, this->current.start, value);
            break;
        }
        case TokenType::Identifier: {
            std::string name = this->current.value;
            this->next();

            expr = std::make_unique<ast::VariableExpr>(start, this->current.start, name);
            break;
        }
        case TokenType::Keyword: {
            if (this->current.value == "sizeof") {
                Location start = this->current.start;
                this->next();
                
                this->expect(TokenType::LParen, "(");
                Type* type = this->parse_type(this->current.value);

                Location end = this->expect(TokenType::LParen, ")").end;
                return std::make_unique<ast::SizeofExpr>(start, end, type);
            } else {
                ERROR(this->current.start, "Unexpected keyword."); exit(1);
            }
        }
        case TokenType::LParen: {
            this->next();
            expr = this->expr(false);

            this->expect(TokenType::RParen, ")");
            break;
        }
        case TokenType::LBracket: {
            this->next();

            std::vector<std::unique_ptr<ast::Expr>> elements;
            while (this->current != TokenType::RBracket) {
                auto element = this->expr(false);
                elements.push_back(std::move(element));

                if (this->current != TokenType::Comma) {
                    break;
                }

                this->next();
            }

            Location end = this->expect(TokenType::RBracket, "]").end;
            expr = std::make_unique<ast::ArrayExpr>(start, end, std::move(elements));
            break;
        }
        case TokenType::LBrace: {
            this->next();
            expr = this->parse_block();
        }
    }

    while (this->current == TokenType::DoubleColon) {
        this->next();

        std::string value = this->expect(TokenType::Identifier, "identifier").value;
        expr = std::make_unique<ast::NamespaceAttributeExpr>(start, this->current.end, value, std::move(expr));
    }

    if (this->current == TokenType::Dot) {
        expr = this->attr(start, std::move(expr));
    } else if (this->current == TokenType::LBracket) {
        expr = this->element(start, std::move(expr));
    }

    return expr;
}