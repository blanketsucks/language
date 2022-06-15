#include "parser.h"

#include <string>

Parser::Parser(std::vector<Token> tokens) : tokens(tokens) {
    this->index = -1;
    this->next();

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
        {"byte", ByteType},
        {"str", StringType},
        {"bool", BooleanType},
        {"array", ArrayType},
    };
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

Type* Parser::get_type(std::string name) {
    if (!this->types.count(name)) {
        this->error("Unknown type: " + name);
    }

    Type* type = this->types[name]->copy();

    // tested piece of garbage. There probably is a better to do this.
    if (type->is_generic()) {
        this->next();

        if (this->current != TokenType::LT) {
            this->error("Missing generic type arguments for " + name);
        }

        this->next();
        std::vector<llvm::Any> values;

        std::vector<TypeVar> vars = type->get_type_vars();
        int count = 0;
        while (this->current != TokenType::GT) {
            if (count > vars.size()) {
                this->error("Too many generic type arguments passed in for " + name);
            }

            if (vars[count].type == TypeVarType::Typename) {
                if (this->current != TokenType::IDENTIFIER) {
                    this->error("Expected type name for generic type argument");
                }

                values.push_back(this->get_type(this->current.value));
            } else if (vars[count].type == TypeVarType::Integer) {
                if (this->current != TokenType::INTEGER) {
                    this->error("Expected integer for generic type argument");
                }

                values.push_back(std::stoi(this->current.value));
                this->next();
            } 

            if (this->current.type != TokenType::COMMA) {
                break;
            }

            this->next();
            count++;
        }

        if (values.size() != vars.size()) {
            this->error("Too few generic type arguments passed in for " + name);
        }

        if (this->current.type != TokenType::GT) {
            this->error("Expected >");
        }

        type->set_type_var_values(values);
    }

    this->next();
    if (this->current == TokenType::MUL) {
        type->is_pointer = true;
        this->next();
    }

    return type;
} 

std::unique_ptr<ast::BlockExpr> Parser::parse_block() {
    std::vector<std::unique_ptr<ast::Expr>> body;
    Location* start = this->current.start;

    while (this->current != TokenType::RBRACE) {
        auto expr = this->statement();
        body.push_back(std::move(expr));
    }

    if (this->current != TokenType::RBRACE) {
        this->error("Expected }");
    }

    Location* end = this->current.end;
    this->next();

    return std::make_unique<ast::BlockExpr>(start, end, std::move(body));
}

std::unique_ptr<ast::PrototypeExpr> Parser::parse_prototype() {
    Location* start = this->current.start;

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
    bool has_varargs = false;

    while (this->current == TokenType::IDENTIFIER || this->current == TokenType::ELLIPSIS) {
        std::string name = this->current.value;
        if (this->current == TokenType::ELLIPSIS) {
            has_varargs = true;
            this->next();

            if (this->current != TokenType::RPAREN) {
                this->error("Expected ) after varargs");
            }

            break;
        }

        // Special case for self if inside a structure. Type of the parameter doesn't need to be specified.
        if (this->context.current_struct) {
            if (name == "self") {
                args.push_back({name, this->context.current_struct});
                this->next();
                
                if (this->current != TokenType::COMMA) {
                    break;
                }

                this->next();
                continue;
            }
        }

        this->next();

        if (this->current != TokenType::COLON) {
            this->error("Expected :");
        }

        this->next();
        if (this->current != TokenType::IDENTIFIER) {
            this->error("Expected type.");
        }

        args.push_back({name, this->get_type(this->current.value)});
        if (this->current != TokenType::COMMA) {
            break;
        }
    
        this->next();
    }

    if (this->current != TokenType::RPAREN) {
        this->error("Expected )");
    }
    
    Location* end = this->current.end;
    this->next();

    Type* ret = VoidType;
    if (this->current == TokenType::ARROW) {
        this->next();
        if (this->current != TokenType::IDENTIFIER) {
            this->error("Expected type.");
        }

        ret = this->get_type(this->current.value);
        end = this->current.end;
    }

    return std::make_unique<ast::PrototypeExpr>(start, end, name, ret, std::move(args), has_varargs);
}

std::unique_ptr<ast::FunctionExpr> Parser::parse_function() {
    Location* start = this->current.start;
    auto prototype = this->parse_prototype();

    if (this->current != TokenType::LBRACE) {
        this->error("Expected {");
    }

    this->next();
    this->context.is_inside_function = true;

    auto body = this->parse_block();
    Location* end = this->current.end;

    return std::make_unique<ast::FunctionExpr>(start, end, std::move(prototype), std::move(body));
}

std::unique_ptr<ast::IfExpr> Parser::parse_if_statement() {
    Location* start = this->current.start;
    std::unique_ptr<ast::Expr> condition = this->expr(false);

    if (this->current != TokenType::LBRACE) {
        this->error("Expected {");
    }

    this->next();
    auto body = this->parse_block();
    
    Location* end = this->current.end;
    this->next();

    std::unique_ptr<ast::BlockExpr> else_body = nullptr;
    if (this->current == TokenType::KEYWORD && this->current.value == "else") {
        this->next();
        if (this->current != TokenType::LBRACE) {
            this->error("Expected {");
        }

        this->next();

        else_body = this->parse_block();
        end = this->current.end;
    }

    return std::make_unique<ast::IfExpr>(start, end, std::move(condition), std::move(body), std::move(else_body));
}

std::unique_ptr<ast::StructExpr> Parser::parse_struct() {
    Location* start = this->current.start;
    bool packed = false;
    if (this->current == TokenType::KEYWORD && this->current.value == "packed") {
        packed = true;
        this->next();
    }
    
    if (this->current != TokenType::IDENTIFIER) {
        this->error("Expected identifer or `packed` keyword.");
    }

    std::string name = this->current.value;
    this->next();

    if (this->current != TokenType::LBRACE) {
        this->error("Expected {");
    }

    this->next();
    std::map<std::string, ast::Argument> fields;
    std::vector<std::unique_ptr<ast::FunctionExpr>> methods;
    std::vector<Type*> types;

    StructType* structure = StructType::create(name, {});
    this->context.current_struct = structure;

    this->types[name] = structure;
    while (this->current != TokenType::RBRACE) {
        if (this->current != TokenType::IDENTIFIER && this->current.value != "def") {
            this->error("Expected identifier or function definition.");
        }

        if (this->current.value == "def") {
            this->next();

            auto function = this->parse_function();
            methods.push_back(std::move(function));
        } else {
            std::string name = this->current.value;
            this->next();

            if (this->current != TokenType::COLON) {
                this->error("Expected :");
            }

            this->next();
            if (this->current != TokenType::IDENTIFIER) {
                this->error("Expected type.");
            }

            types.push_back(this->get_type(this->current.value));
            fields[name] = {name, types.back()};

            if (this->current != TokenType::SEMICOLON) {
                this->error("Expected ;");
            }

            this->next();
        }
    }

    if (this->current != TokenType::RBRACE) {
        this->error("Expected }");
    }

    Location* end = this->current.end;
    structure->set_fields(types);

    this->next();
    return std::make_unique<ast::StructExpr>(start, end, name, packed, std::move(fields), std::move(methods));    
}

std::unique_ptr<ast::Expr> Parser::parse_variable_definition(bool is_const) {
    Location* start = this->current.start;

    if (this->current != TokenType::IDENTIFIER) {
        this->error("Expected identifer.");
    }

    Type* type = nullptr;
    std::string name = this->current.value;
    this->next();

    if (this->current == TokenType::COLON) {
        this->next();
        if (this->current != TokenType::IDENTIFIER) {
            this->error("Expected type.");
        }

        type = this->get_type(this->current.value);
    }

    if (this->current != TokenType::ASSIGN) {
        this->error("Expected =");
    }

    this->next();
        
    auto expr = this->expr(false);
    Location* end = this->current.end;
    
    if (this->current != TokenType::SEMICOLON) {
        this->error("Expected ;");
    }

    if (is_const) {
        return std::make_unique<ast::ConstExpr>(start, end, name, type, std::move(expr));
    } else {
        return std::make_unique<ast::VariableAssignmentExpr>(start, end, name, type, std::move(expr));
    }
}

std::unique_ptr<ast::NamespaceExpr> Parser::parse_namespace() {
    Location* start = this->current.start;
    std::string name = this->current.value;

    this->next();
    if (this->current != TokenType::LBRACE) {
        this->error("Expected {");
    }

    this->next();

    std::vector<std::unique_ptr<ast::FunctionExpr>> functions;
    std::vector<std::unique_ptr<ast::StructExpr>> structs;
    std::vector<std::unique_ptr<ast::ConstExpr>> constants;
    std::vector<std::unique_ptr<ast::NamespaceExpr>> namespaces;

    while (this->current != TokenType::RBRACE) {
        if (
            this->current != TokenType::KEYWORD || (this->current.value != "def" && this->current.value != "struct" && this->current.value != "const" && this->current.value != "namespace")
        ) {
            this->error("Expected function, struct, const or namespace definition.");
        }

        if (this->current.value == "def") {
            this->next();
            functions.push_back(std::move(this->parse_function()));
        } else if (this->current.value == "struct") {
            this->next();
            structs.push_back(std::move(this->parse_struct()));
        } else if (this->current.value == "namespace") {
            this->next();
            namespaces.push_back(std::move(this->parse_namespace()));
        }
    }

    if (this->current != TokenType::RBRACE) {
        this->error("Expected }");
    }

    Location* end = this->current.end;
    this->next();

    return std::make_unique<ast::NamespaceExpr>(start, end, name, std::move(functions), std::move(structs), std::move(constants), std::move(namespaces));
}

std::unique_ptr<ast::Program> Parser::statements() {
    std::vector<std::unique_ptr<ast::Expr>> statements;

    while (this->current != TokenType::EOS) {
        auto expr = this->statement();
        if (!expr) {
            continue;
        }

        statements.push_back(std::move(expr));
    }

    return std::make_unique<ast::Program>(std::move(statements));
}

std::unique_ptr<ast::Expr> Parser::statement() {
    switch (this->current.type) {
        case TokenType::KEYWORD:
            if (this->current.value == "extern") {
                this->next();
                if (this->current == TokenType::STRING) {
                    if (this->current.value != "C") {
                        this->error("Unknown extern linkage specification");
                    }

                    // For now just ignore if "C" is used because from what I know if it's used like 
                    // `extern "C" void foo();` in C++, the name of the function doesn't get mangled unlike if used without the "C".
                    // Right now, I don't do name mangling so I'll just ignore it.
                    this->next();
                }

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
                Location* start = this->current.start;
                if (this->current == TokenType::SEMICOLON) {
                    this->next();
                    return std::make_unique<ast::ReturnExpr>(start, this->current.end, nullptr);
                }

                auto expr = this->expr(false);
                Location* end = this->current.end;

                if (this->current != TokenType::SEMICOLON) {
                    this->error("Expected ;");
                }

                return std::make_unique<ast::ReturnExpr>(start, end, std::move(expr));
            } else if (this->current.value == "if") {
                if (!this->context.is_inside_function) {
                    this->error("if is only allowed inside functions.");
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
                    this->error("struct is only allowed outside functions.");
                }

                this->next();
                return this->parse_struct();
            } else if (this->current.value == "include") {
                Location* start = this->current.start;
                this->next();

                if (this->current != TokenType::STRING) {
                    this->error("Expected file path string.");
                }

                std::string path = this->current.value;
                Location* end = this->current.end;

                this->next();
                return std::make_unique<ast::IncludeExpr>(start, end, path);
            } else if (this->current.value == "namespace") {
                this->next();
                return this->parse_namespace();
            } else if (this->current.value == "type") {
                this->next();
                std::string name = this->current.value;

                this->next();
                if (this->current != TokenType::ASSIGN) {
                    this->error("Expected =");
                }

                this->next();
                if (this->current != TokenType::IDENTIFIER) {
                    this->error("Expected type name.");
                }

                Type* type = this->get_type(this->current.value);
                if (this->current != TokenType::SEMICOLON) {
                    this->error("Expected ;");
                }

                this->next();
                this->types[name] = type;

                return nullptr;
            } else if (this->current.value == "while") {
                this->next();
                auto condition = this->expr(false);

                if (this->current != TokenType::LBRACE) {
                    this->error("Expected {");
                }

                this->next();
                auto body = this->parse_block();

                return std::make_unique<ast::WhileExpr>(this->current.start, this->current.end, std::move(condition), std::move(body));
            } else {
                this->error("Unimplemented keyword.");
            }

            break;
        default:
            return this->expr();
    }

    this->error("Unexpected token.");
    return nullptr;
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
        Location* start = this->current.start;
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

        left = std::make_unique<ast::BinaryOpExpr>(start, this->current.end, op, std::move(left), std::move(right));
    }
}

std::unique_ptr<ast::Expr> Parser::unary() {
    if (std::find(UNARY_OPERATORS.begin(), UNARY_OPERATORS.end(), this->current.type) == UNARY_OPERATORS.end()) {
        return this->call();
    }

    Location* start = this->current.start;

    TokenType op = this->current.type;
    this->next();

    auto value = this->call();
    return std::make_unique<ast::UnaryOpExpr>(start, this->current.end, op, std::move(value));
}

std::unique_ptr<ast::Expr> Parser::call() {
    Location* start = this->current.start;
    auto expr = this->factor();

    if (this->current == TokenType::LPAREN) {
        this->next();
        std::vector<std::unique_ptr<ast::Expr>> args;

        if (this->current != TokenType::RPAREN) {
            while (true) {
                auto arg = this->expr(false);
                args.push_back(std::move(arg));

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
        return std::make_unique<ast::CallExpr>(start, this->current.end, std::move(expr), std::move(args));
    }

    return expr;
}

std::unique_ptr<ast::Expr> Parser::factor() {
    std::unique_ptr<ast::Expr> expr;

    Location* start = this->current.start;
    switch (this->current.type) {
        case TokenType::INTEGER: {
            int number = std::stol(this->current.value);
            this->next();

            expr = std::make_unique<ast::IntegerExpr>(start, this->current.start, number);
            break;
        }
        case TokenType::FLOAT: {
            float number = std::stof(this->current.value);
            this->next();

            expr = std::make_unique<ast::FloatExpr>(start, this->current.start, number);
            break;
        }
        case TokenType::STRING: {
            std::string value = this->current.value;
            this->next();

            expr = std::make_unique<ast::StringExpr>(start, this->current.start, value);
            break;
        }
        case TokenType::IDENTIFIER: {
            std::string name = this->current.value;
            this->next();

            expr = std::make_unique<ast::VariableExpr>(start, this->current.start, name);
            break;
        }
        case TokenType::LPAREN: {
            this->next();
            expr = this->expr(false);

            if (this->current != TokenType::RPAREN) {
                this->error("Expected )");
            }

            this->next();
            break;
        }
        case TokenType::LBRACKET: {
            this->next();

            std::vector<std::unique_ptr<ast::Expr>> elements;
            while (this->current != TokenType::RBRACKET) {
                auto element = this->expr(false);
                elements.push_back(std::move(element));

                if (this->current != TokenType::COMMA) {
                    break;
                }

                this->next();
            }

            if (this->current != TokenType::RBRACKET) {
                this->error("Expected ]");
            }

            Location* end = this->current.end;
            this->next();

            expr = std::make_unique<ast::ArrayExpr>(start, end, std::move(elements));
            break;
        }
        case TokenType::LBRACE: {
            this->next();
            expr = this->parse_block();
        }
    }

    while (this->current == TokenType::DOUBLECOLON) {
        this->next();
        if (this->current != TokenType::IDENTIFIER) {
            this->error("Expected identifier.");
        }

        expr = std::make_unique<ast::NamespaceAttributeExpr>(start, this->current.end, this->current.value, std::move(expr));
        this->next();
    }

    if (this->current == TokenType::LBRACKET) {
        this->next();
        auto index = this->expr(false);
        if (this->current != TokenType::RBRACKET) {
            this->error("Expected ]");
        }

        this->next();
        expr = std::make_unique<ast::ElementExpr>(start, this->current.end, std::move(expr), std::move(index));
    }

    while (this->current == TokenType::DOT) {
        this->next();
        if (this->current != TokenType::IDENTIFIER) {
            this->error("Expected identifier.");
        }

        expr = std::make_unique<ast::AttributeExpr>(start, this->current.end, this->current.value, std::move(expr));
        this->next();
    }

    return expr;
}