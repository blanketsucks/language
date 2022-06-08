#include "lexer.h"

Lexer::Lexer(std::string source, std::string filename) {
    this->source = source;
    this->filename = filename;

    this->reset();
    this->next();
}

Lexer::Lexer(std::ifstream& file, std::string filename) {
    std::stringstream buffer;
    buffer << file.rdbuf();

    this->source = buffer.str();
    this->filename = filename;

    this->reset();
    this->next();
}

Lexer::Lexer(FILE* file, std::string filename) {
    std::stringstream buffer;
    buffer << file;

    this->source = buffer.str();
    this->filename = filename;

    this->reset();
    this->next();
}

char Lexer::next(bool check_newline) {
    this->current = this->source[this->index];
    this->index++;

    if (check_newline) {
        while (this->current == '\n' || this->current == '\r') {
            if (this->current == '\n') {
                this->line++;
                this->column = 0;
            }

            this->next();
        }
    }

    if (this->current == 0) {
        this->eof = true;
        return this->current;
    }

    this->column++;
    return this->current;
}

char Lexer::peek() { 
    return this->source[this->index + 1]; 
}

char Lexer::prev() { 
    return this->source[this->index - 1]; 
}

void Lexer::reset() {
    this->index = 0;
    this->line = 1;
    this->column = 0;
    this->eof = false;
}

bool Lexer::is_keyword(std::string word) {
    return std::find(KEYWORDS.begin(), KEYWORDS.end(), word) != KEYWORDS.end();
}

Token Lexer::create_token(TokenType type, std::string value) {
    return {type, this->loc(), this->loc(), value};
}

Token Lexer::create_token(TokenType type, Location* start, std::string value) {
    return {type, start, this->loc(), value};
}

Location* Lexer::loc() {
    return new Location(this->line, this->column, this->index, this->filename, this->source);
}

void Lexer::skip_comment() {
    while (this->current != '\n' && this->current != 0) {
        this->next(false);
    }

    this->next();
}

Token Lexer::parse_identifier() {
    std::string value;
    Location* start = this->loc();

    value += this->current;

    char next = this->next();
    while (std::isalnum(next) || next == '_') {
        value += this->current;
        next = this->next();
    }

    if (this->is_keyword(value)) {
        return this->create_token(TokenType::KEYWORD, start, value);
    } else {
        return this->create_token(TokenType::IDENTIFIER, start, value);
    }
}

Token Lexer::parse_string() {
    std::string value;
    Location* start = this->loc();

    char opening = this->current;

    char next = this->next();
    while (next && next != opening) {
        value += this->current;
        next = this->next();
    }

    if (this->current != opening) {
        std::cerr << "[ERROR] Expected " << opening << " but got " << this->current << std::endl;
        exit(1);
    }
    
    Token token = this->create_token(TokenType::STRING, start, value);
    this->next();

    return token;
}

Token Lexer::parse_number() {
    std::string value;
    Location* start = this->loc();

    value += this->current;

    char next = this->next();
    while (std::isdigit(next) || next == '.') {
        value += this->current;
        next = this->next();
    }

    return this->create_token(TokenType::INTEGER, start, value);
}

std::vector<Token> Lexer::lex() {
    std::vector<Token> tokens;

    while (true) {
        if (this->eof) break;

        if (std::isspace(this->current)) {
            this->next();
            continue;
        } else if (std::isalpha(this->current) || this->current == '_') {
            tokens.push_back(this->parse_identifier());
        } else if (std::isdigit(this->current)) {
            tokens.push_back(this->parse_number());
        } else if (this->current == '#') {
            this->skip_comment();
        } else if (this->current == '+') {
            tokens.push_back(this->create_token(TokenType::PLUS, "+"));    
            this->next();
        } else if (this->current == '-') {
            Location* start = this->loc();
            Token token;

            char next = this->next();
            if (next == '>') {
                this->next();
                token = this->create_token(TokenType::ARROW, start, "->");
            } else {
                token = this->create_token(TokenType::MINUS, "-");
            }

            tokens.push_back(token);
        } else if (this->current == '*') {
            tokens.push_back(this->create_token(TokenType::MUL, "*"));
            this->next();
        } else if (this->current == '/') {
            tokens.push_back(this->create_token(TokenType::DIV, "/"));
            this->next();
        } else if (this->current == '=') {
            tokens.push_back(this->create_token(TokenType::ASSIGN, "="));
            this->next();
        } else if (this->current == '>') {
            Location* start = this->loc();
            Token token;

            char next = this->next();
            if (next == '=') {
                this->next();
                token = this->create_token(TokenType::GTE, start, ">=");
            } else if (next == '>') {
                this->next();
                token = this->create_token(TokenType::GT, start, ">>");
            } else {
                token = this->create_token(TokenType::GT, start, ">");
            }

            tokens.push_back(token);
        } else if (this->current == '<') {
            Location* start = this->loc();
            Token token;

            char next = this->next();
            if (next == '=') {
                this->next();
                token = this->create_token(TokenType::LTE, start, "<=");
            } else if (next == '<') {
                this->next();
                token = this->create_token(TokenType::LT, start, "<<");
            } else {
                token = this->create_token(TokenType::LT, start, "<");
            }

            tokens.push_back(token);
        } else if (this->current == '!') {
            Location* start = this->loc();
            Token token;

            char next = this->next();
            if (next == '=') {
                this->next();
                token = this->create_token(TokenType::NEQ, start, "!=");
            } else {
                token = this->create_token(TokenType::NOT, start, "!");
            }

            tokens.push_back(token);
        } else if (this->current == '|') {
            Location* start = this->loc();
            Token token;

            char next = this->next();
            if (next == '|') {
                this->next();
                token = this->create_token(TokenType::OR, start, "||");
            } else {
                token = this->create_token(TokenType::BINARY_OR, start, "|");
            }
            
            tokens.push_back(token);
        } else if (this->current == '&') {
            Location* start = this->loc();
            Token token;

            char next = this->next();
            if (next == '&') {
                this->next();
                token = this->create_token(TokenType::AND, start, "&&");
            } else {
                token = this->create_token(TokenType::BINARY_AND, start, "&");
            }

            tokens.push_back(token);
        } else if (this->current == '~') {
            tokens.push_back(this->create_token(TokenType::BINARY_NOT, "~"));
            this->next();
        } else if (this->current == '^') {
            tokens.push_back(this->create_token(TokenType::XOR, "^"));
            this->next();
        } else if (this->current == '(') {
            tokens.push_back(this->create_token(TokenType::LPAREN, "("));
            this->next();
        } else if (this->current == ')') {
            tokens.push_back(this->create_token(TokenType::RPAREN, ")"));
            this->next();
        } else if (this->current == '{') {
            tokens.push_back(this->create_token(TokenType::LBRACE, "{"));
            this->next();
        } else if (this->current == '}') {
            tokens.push_back(this->create_token(TokenType::RBRACE, "}"));
            this->next();
        } else if (this->current == '[') {
            tokens.push_back(this->create_token(TokenType::LBRACKET, "["));
            this->next();
        } else if (this->current == ']') {
            tokens.push_back(this->create_token(TokenType::RBRACKET, "]"));
            this->next();
        } else if (this->current == ',') {
            tokens.push_back(this->create_token(TokenType::COMMA, ","));
            this->next();
        } else if (this->current == '.') {
            tokens.push_back(this->create_token(TokenType::DOT, "."));
            this->next();
        } else if (this->current == ';') {
            tokens.push_back(this->create_token(TokenType::SEMICOLON, ";"));
            this->next();
        } else if (this->current == ':') {
            tokens.push_back(this->create_token(TokenType::COLON, ":"));
            this->next();
        } else if (this->current == '"' || this->current == '\'') {
            tokens.push_back(this->parse_string());
        } else {
            std::cerr << "[ERROR] Unknown character: " << this->current << std::endl;
            exit(1);
        }
    }

    Token eof = {TokenType::EOS, this->loc(), this->loc(), "\0"};
    tokens.push_back(eof);

    return tokens;
}