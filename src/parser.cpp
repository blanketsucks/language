#include "parser.h"

#include <string>

Parser::Parser(std::vector<Token> tokens) : tokens(tokens) {
    this->index = -1;
    this->next();

    // Populate the hash map with already defined precedences.
    for (auto pair : PRECEDENCES) {
        this->precedences[pair.first] = pair.second;
    }

    for (auto type : TYPES) {
        this->types[type.name] = type;
    }
}

void Parser::error(const std::string& message) {
    // TODO: More descriptive errors.
    std::cerr << message << '\n';
    exit(1);
}

void Parser::end() {
    if (this->current != TokenType::SEMICOLON) {
        this->error("Expected ;");
    }

    this->next();
}

Token Parser::next() {
    this->index++;

    if (this->index >= this->tokens.size()) {
        this->current = this->tokens[-1];
    } else {
        this->current = this->tokens[this->index];
    }

    return this->current;
}

Token Parser::peek() {
    if (this->index >= this->tokens.size()) {
        return this->tokens[-1];
    }

    return this->tokens[this->index + 1];
}

int Parser::get_token_precendence() {
    int precedence = this->precedences[this->current.type];
    if (precedence <= 0) {
        return -1;
    }

    return precedence;
}

Type Parser::get_type(std::string name) {
    if (!this->types.count(name)) {
        this->error("Unknown type: " + name);
    }

    return this->types[name];
} 

std::unique_ptr<ast::PrototypeExpr> Parser::parse_prototype() {
    if (this->current != TokenType::IDENTIFIER) {
        this->error("Expected identifer.");
    }

    std::string name = this->current.value;
    this->next();

    if (this->current != TokenType::LPAREN) {
        this->error("Expected (");
    }

    this->next();
    std::vector<ast::Argument> args;

    while (this->current == TokenType::IDENTIFIER) {
        std::string name = this->current.value;
        this->next();

        if (this->current != TokenType::COLON) {
            this->error("Expected :");
        }

        this->next();
        if (this->current != TokenType::IDENTIFIER) {
            this->error("Expected type.");
        }

        args.push_back({name, this->get_type(this->current.value)});
        this->next();

        if (this->current != TokenType::COMMA) {
            break;
        }

        this->next();
    } 

    if (this->current != TokenType::RPAREN) {
        this->error("Expected )");
    }

    this->next();
    if (this->current != TokenType::ARROW) {
        this->error("Expected ->");
    }

    this->next();
    if (this->current != TokenType::IDENTIFIER) {
        this->error("Expected type.");
    }

    Type type = this->get_type(this->current.value);
    this->next();

    return std::make_unique<ast::PrototypeExpr>(name, type, std::move(args));
}

std::unique_ptr<ast::FunctionExpr> Parser::parse_function() {
    auto prototype = this->parse_prototype();
    if (this->current != TokenType::LBRACE) {
        this->error("Expected {");
    }

    this->next();
    this->context.is_inside_function = true;

    std::vector<std::unique_ptr<ast::Expr>> body;
    while (this->current != TokenType::RBRACE) {
        auto expr = this->statement();
        body.push_back(std::move(expr));
    }

    if (this->current != TokenType::RBRACE) {
        this->error("Expected }");
    }
    
    this->next();
    return std::make_unique<ast::FunctionExpr>(std::move(prototype), std::move(body));
}

std::unique_ptr<ast::Program> Parser::statements() {
    std::vector<std::unique_ptr<ast::Expr>> statements;

    while (this->current != TokenType::EOS) {
        auto expr = this->statement();
        statements.push_back(std::move(expr));
    }

    return std::make_unique<ast::Program>(std::move(statements));
}

std::unique_ptr<ast::Expr> Parser::statement() {
    switch (this->current.type) {
        case TokenType::KEYWORD:
            if (this->current.value == "extern") {
                this->next();
                auto expr = this->parse_prototype();

                this->end();
                return expr;
            } else if (this->current.value == "def") {
                this->next();
                return this->parse_function();
            } else if (this->current.value == "return") {
                if (!this->context.is_inside_function) {
                    this->error("return outside of function.");
                }

                this->next();
                if (this->current == TokenType::SEMICOLON) {
                    this->next();
                    return std::make_unique<ast::ReturnExpr>(nullptr);
                }

                return std::make_unique<ast::ReturnExpr>(this->expr());
            }
            break;
        default:
            return this->expr();
    }

    this->error("Unreachable.");
}

std::unique_ptr<ast::Expr> Parser::expr(bool expect_semicolon) {
    auto left = this->factor();
    auto expr = this->binary(0, std::move(left));
    
    if (expect_semicolon) {
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

        TokenType op = this->current.type;
        this->next();

        auto right = this->factor();

        int next = this->get_token_precendence();
        if (precedence < next) {
            right = this->binary(precedence + 1, std::move(right));
        }

        left = std::make_unique<ast::BinaryOpExpr>(op, std::move(left), std::move(right));
    }
}

std::unique_ptr<ast::Expr> Parser::factor() {
    switch (this->current.type) {
        case TokenType::INTEGER: {
            int number = std::stoi(this->current.value);
            this->next();

            return std::make_unique<ast::IntegerExpr>(number);
        }
        case TokenType::IDENTIFIER: {
            std::string name = this->current.value;
            this->next();

            if (this->current != TokenType::LPAREN) {
                return std::make_unique<ast::VariableExpr>(name);
            }

            this->next();
            std::vector<std::unique_ptr<ast::Expr>> args;

            if (this->current != TokenType::RPAREN) {
                while (true) {
                    args.push_back(this->expr(false));
                    if (this->current != TokenType::COMMA) {
                        break;
                    }

                    this->next();
                }
            }

            if (this->current != TokenType::RPAREN) {
                this->error("Expected )");
            }

            this->next();
            return std::make_unique<ast::CallExpr>(name, std::move(args));
        }
        case TokenType::LPAREN: {
            this->next();
            auto result = this->expr();

            if (this->current != TokenType::RPAREN) {
                this->error("Expected )");
            }
            
            return result;
        }
        default:
            this->error("Unimplmented.");
    }

    return nullptr;
}