#include "lexer.h"

#include "utils.h"

std::string escape_str(std::string str) {
    std::string result;
    for (char c : str) {
        if (!isprint((unsigned char)c)) {
            std::stringstream stream;
            stream << std::hex << (unsigned int)(unsigned char)c;
            
            std::string code = stream.str();
            result += std::string("\\x") + (code.size() < 2 ? "0" : "") + code;
        } else {
            result += c;
        }
    }

    return result;
}

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

char Lexer::next() {
    this->current = this->source[this->index];
    this->index++;

    if (this->current == 0) {
        this->eof = true;
        return this->current;
    }

    this->column++;
    return this->current;
}

char Lexer::peek() { 
    return this->source[this->index]; 
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

Token Lexer::create_token(TokenType type, Location start, std::string value) {
    return {type, start, this->loc(), value};
}

Location Lexer::loc() {
    return Location {
        this->line,
        this->column,
        this->index,
        this->filename,
    };
}

char Lexer::escape(char current) {
    if (current != '\\') {
        return current;
    }

    char next = this->next();
    if (next == 'n') {
        return '\n';
    } else if (next == 't') {
        return '\t';
    } else if (next == 'r') {
        return '\r';
    } else if (next == '\\') {
        return '\\';
    } else if (next == '\'') {
        return '\'';
    } else if (next == '0') {
        return '\0';
    } else if (next == '"') {
        return '"';
    } else if (next == 'x') {
        char hex[3];
        hex[0] = this->next();
        hex[1] = this->next();
        hex[2] = 0;

        int i = 0;
        while (hex[i] != 0) {
            if (hex[i] >= '0' && hex[i] <= '9') {
                hex[i] -= '0';
            } else if (hex[i] >= 'a' && hex[i] <= 'f') {
                hex[i] -= 'a' - 10;
            } else if (hex[i] >= 'A' && hex[i] <= 'F') {
                hex[i] -= 'A' - 10;
            } else {
                std::cerr << "Invalid hexadecimal character '" << hex[i] << "'." << std::endl;
                exit(1);
            }

            i++;
        }

        return (char)(hex[0] * 16 + hex[1]);
    } else {
        utils::error(this->loc(), "Invalid escape sequence");
        return '\0';
    }
}

void Lexer::skip_comment() {
    while (this->current != '\n' && this->current != 0) {
        this->next();
    }

    this->next();
}

Token Lexer::parse_identifier() {
    std::string value;
    Location start = this->loc();

    value += this->current;

    char next = this->next();
    while (std::isalnum(next) || next == '_') {
        value += this->current;
        next = this->next();
    }

    if (this->is_keyword(value)) {
        return this->create_token(TokenType::Keyword, start, value);
    } else {
        if (value[0] == '$') {
            utils::error(start, "Identifiers starting with '$' are reserved for keywords.");
        }

        return this->create_token(TokenType::Identifier, start, value);
    }
}

Token Lexer::parse_string() {
    std::string value;
    Location start = this->loc();

    if (this->current == '\'') {
        char character = this->escape(this->next());

        this->next();
        return this->create_token(TokenType::Char, start, std::to_string(character));
    }

    char next = this->next();
    while (next && next != '"') {
        value += this->escape(this->current);
        next = this->next();
    }

    if (this->current != '"') {
        utils::error(this->loc(), "Expected end of string.");
    }
    
    Token token = this->create_token(TokenType::String, start, value);
    this->next();

    return token;
}

Token Lexer::parse_number() {
    std::string value;
    Location start = this->loc();

    value += this->current;

    char next = this->next();
    if (value == "0") {
        if (next == 'x' || next == 'b') {
            value += this->current;
            if (next == 'x') {
                while (std::isxdigit(this->current)) {
                    value += this->current;
                    this->next();
                }

                // Kind of funky, but eh
                long val = std::stol(value, nullptr, 16);
                value = std::to_string(val);
            } else {
                while (this->current == '0' || this->current == '1') {
                    value += this->current;
                    this->next();
                }

                long val = std::stol(value, nullptr, 2);
                value = std::to_string(val);
            }

            return this->create_token(TokenType::Integer, start, value);
        }
    
        if (std::isdigit(this->peek()) ) {
            utils::error(start, "Leading zeros on integer constants are not allowed");
        }
    }

    bool dot = false;
    while (std::isdigit(next) || next == '.') {
        if (next == '.') {
            if (dot) {
                break;
            }

            dot = true;
        }

        value += this->current;
        next = this->next();
    }

    if (dot) {
        return this->create_token(TokenType::Float, start, value);
    } else {
        return this->create_token(TokenType::Integer, start, value);
    }
}

std::vector<Token> Lexer::lex() {
    std::vector<Token> tokens;

    while (true) {
        if (this->eof) break;

        if (this->current == '\n') {
            this->line++;
            this->column = 0;

            tokens.push_back(this->create_token(TokenType::Newline, "\n"));
            this->next();

            continue;
        }

        if (std::isspace(this->current)) {
            this->next();
            continue;
        } else if (std::isalpha(this->current) || this->current == '_' || this->current == '$') {
            tokens.push_back(this->parse_identifier());
        } else if (std::isdigit(this->current)) {
            tokens.push_back(this->parse_number());
        } else if (this->current == '#') {
            this->skip_comment();
        } else if (this->current == '+') {
            Location start = this->loc();
            Token token;

            char next = this->next();
            if (next == '+') {
                this->next();
                token = this->create_token(TokenType::Inc, start, "++");
            } else {
                token = this->create_token(TokenType::Add, start, "+");
            }

            tokens.push_back(token);
        } else if (this->current == '-') {
            Location start = this->loc();
            Token token;

            char next = this->next();
            if (next == '>') {
                this->next();
                token = this->create_token(TokenType::Arrow, start, "->");
            } else if (next == '-') {
                this->next();
                token = this->create_token(TokenType::Dec, start, "--");
            } else {
                token = this->create_token(TokenType::Minus, "-");
            }

            tokens.push_back(token);
        } else if (this->current == '*') {
            tokens.push_back(this->create_token(TokenType::Mul, "*"));
            this->next();
        } else if (this->current == '/') {
            tokens.push_back(this->create_token(TokenType::Div, "/"));
            this->next();
        } else if (this->current == '=') {
            Location start = this->loc();
            Token token;

            char next = this->next();
            if (next == '=') {
                this->next();
                token = this->create_token(TokenType::Eq, start, "==");
            } else {
                token = this->create_token(TokenType::Assign, "=");
            }

            tokens.push_back(token);
        } else if (this->current == '>') {
            Location start = this->loc();
            Token token;

            char next = this->next();
            if (next == '=') {
                this->next();
                token = this->create_token(TokenType::Gte, start, ">=");
            } else if (next == '>') {
                this->next();
                token = this->create_token(TokenType::Rsh, start, ">>");
            } else {
                token = this->create_token(TokenType::Gt, start, ">");
            }

            tokens.push_back(token);
        } else if (this->current == '<') {
            Location start = this->loc();
            Token token;

            char next = this->next();
            if (next == '=') {
                this->next();
                token = this->create_token(TokenType::Lte, start, "<=");
            } else if (next == '<') {
                this->next();
                token = this->create_token(TokenType::Lsh, start, "<<");
            } else {
                token = this->create_token(TokenType::Lt, start, "<");
            }

            tokens.push_back(token);
        } else if (this->current == '!') {
            Location start = this->loc();
            Token token;

            char next = this->next();
            if (next == '=') {
                this->next();
                token = this->create_token(TokenType::Neq, start, "!=");
            } else {
                token = this->create_token(TokenType::Not, start, "!");
            }

            tokens.push_back(token);
        } else if (this->current == '|') {
            Location start = this->loc();
            Token token;

            char next = this->next();
            if (next == '|') {
                this->next();
                token = this->create_token(TokenType::Or, start, "||");
            } else {
                token = this->create_token(TokenType::BinaryOr, start, "|");
            }
            
            tokens.push_back(token);
        } else if (this->current == '&') {
            Location start = this->loc();
            Token token;

            char next = this->next();
            if (next == '&') {
                this->next();
                token = this->create_token(TokenType::And, start, "&&");
            } else {
                token = this->create_token(TokenType::BinaryAnd, start, "&");
            }

            tokens.push_back(token);
        } else if (this->current == '~') {
            tokens.push_back(this->create_token(TokenType::BinaryNot, "~"));
            this->next();
        } else if (this->current == '^') {
            tokens.push_back(this->create_token(TokenType::Xor, "^"));
            this->next();
        } else if (this->current == '(') {
            tokens.push_back(this->create_token(TokenType::LParen, "("));
            this->next();
        } else if (this->current == ')') {
            tokens.push_back(this->create_token(TokenType::RParen, ")"));
            this->next();
        } else if (this->current == '{') {
            tokens.push_back(this->create_token(TokenType::LBrace, "{"));
            this->next();
        } else if (this->current == '}') {
            tokens.push_back(this->create_token(TokenType::RBrace, "}"));
            this->next();
        } else if (this->current == '[') {
            tokens.push_back(this->create_token(TokenType::LBracket, "["));
            this->next();
        } else if (this->current == ']') {
            tokens.push_back(this->create_token(TokenType::RBracket, "]"));
            this->next();
        } else if (this->current == ',') {
            tokens.push_back(this->create_token(TokenType::Comma, ","));
            this->next();
        } else if (this->current == '.') {
            Location start = this->loc();
            Token token;

            char next = this->next();
            if (next == '.' && this->peek() == '.') {
                this->next(); this->next();
                token = this->create_token(TokenType::Ellipsis, start, "...");
            } else {
                token = this->create_token(TokenType::Dot, ".");
            }

            tokens.push_back(token);
        } else if (this->current == ';') {
            tokens.push_back(this->create_token(TokenType::SemiColon, ";"));
            this->next();
        } else if (this->current == ':') {
            Location start = this->loc();
            Token token;

            char next = this->next();
            if (next == ':') {
                this->next();
                token = this->create_token(TokenType::DoubleColon, start, "::");
            } else {
                token = this->create_token(TokenType::Colon, ":");
            }

            tokens.push_back(token);
        } else if (this->current == '"' || this->current == '\'') {
            tokens.push_back(this->parse_string());
        } else {
            std::string msg = "Unrecognized character '";
            utils::error(this->loc(), msg + this->current + "'");
        }
    }

    Token eof = {TokenType::EOS, this->loc(), this->loc(), "\0"};
    tokens.push_back(eof);

    return tokens;
}