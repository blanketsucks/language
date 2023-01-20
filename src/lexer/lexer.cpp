#include "lexer/lexer.h"
#include "utils/log.h"

#include <cctype>
#include <string>

Lexer::Lexer(const std::string& source, std::string filename) {
    this->source = source;
    this->filename = filename;

    this->reset();
    this->next();
}

Lexer::Lexer(utils::filesystem::Path path) {
    this->filename = path.str();
    this->source = path.read(true).str();

    this->reset();
    this->next();
}

char Lexer::next() {
    if (this->index == UINT32_MAX) {
        ERROR(this->make_span(), "Lexer index overflow.");
    }

    this->current = this->source[this->index];
    this->index++;

    if (this->current == 0) {
        this->eof = true;
        return this->current;
    }

    this->column++;
    if (this->column == UINT32_MAX) {
        ERROR(this->make_span(), "Lexer column overflow.");
    }

    return this->current;
}

char Lexer::peek(uint32_t offset) { 
    return this->source[this->index + offset]; 
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
    return KEYWORDS.find(word) != KEYWORDS.end();
}

TokenKind Lexer::get_keyword_kind(std::string word) {
    return KEYWORDS.at(word);
}

Token Lexer::create_token(TokenKind type, std::string value) {
    return {type, value, this->make_span()};
}

Token Lexer::create_token(TokenKind type, Location start, std::string value) {
    return {type, value, this->make_span(start, this->loc())};
}

Location Lexer::loc() {
    return Location {
        this->line,
        this->column,
        this->index
    };
}

Span Lexer::make_span() {
    return this->make_span(this->loc(), this->loc());
}

Span Lexer::make_span(const Location& start, const Location& end) {
    uint32_t offset = start.index - start.column;
    std::string line;

    if (offset < this->source.size()) {
        line = this->source.substr(offset);
        line = line.substr(0, line.find('\n'));
    }

    return Span {
        start,
        end,
        this->filename.c_str(),
        line
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
        while (hex[i]) {
            if (hex[i] >= '0' && hex[i] <= '9') {
                hex[i] -= '0';
            } else if (hex[i] >= 'a' && hex[i] <= 'f') {
                hex[i] -= 'a' - 10;
            } else if (hex[i] >= 'A' && hex[i] <= 'F') {
                hex[i] -= 'A' - 10;
            } else {
                ERROR(this->make_span(), "Invalid hex escape sequence");
            }

            i++;
        }

        return (char)(hex[0] * 16 + hex[1]);
    }

    ERROR(this->make_span(), "Invalid escape sequence"); exit(1);
}

bool Lexer::is_valid_identifier(uint8_t c) {
    if (std::isalnum(c) || c == '_') {
        return true;
    } else if (c < 128) {
        return false;
    }

    uint32_t expected = 0;
    if (c < 0x80) {
        return true;
    } else if (c < 0xE0) {
        expected = 1;
    } else if (c < 0xF0) {
        expected = 2;
    } else if (c < 0xF8) {
        expected = 3;
    } else {
        return false;
    }

    for (uint32_t i = 0; i < expected; i++) {
        uint8_t next = this->peek(i);
        if ((next & 0xC0) != 0x80) {
            return false;
        }
    }

    return true;
}

std::string Lexer::parse_unicode(uint8_t current) {  
    uint32_t expected = 0;

    if (current < 0x80) {
        return std::string(1, current);
    } else if (current < 0xE0) {
        expected = 1;
    } else if (current < 0xF0) {
        expected = 2;
    } else if (current < 0xF8) {
        expected = 3;
    }

    std::string unicode;
    unicode.reserve(expected + 1); // I'm pretty sure this doesn't improve performance that much in this case but meh

    unicode += this->current;
    for (uint32_t i = 0; i < expected; i++) {
        unicode += this->next();
    }

    return unicode;
}

void Lexer::skip_comment() {
    while (this->current != '\n' && this->current != 0) {
        this->next();
    }
}

Token Lexer::parse_identifier() {
    std::string value;
    Location start = this->loc();

    value += this->parse_unicode(this->current);

    char next = this->next();
    while (this->is_valid_identifier(next)) {
        value += this->parse_unicode(next);
        next = this->next();
    }

    if (Lexer::is_keyword(value)) {
        return this->create_token(Lexer::get_keyword_kind(value), start, value);
    } else {
        if (value[0] == '$') {
            ERROR(this->make_span(start, this->loc()), "Identifiers starting with '$' are reserved for keywords.");
        }

        return this->create_token(TokenKind::Identifier, start, value);
    }
}

Token Lexer::parse_string() {
    std::string value;
    Location start = this->loc();

    if (this->current == '\'') {
        char character = this->escape(this->next());
        this->next(); this->next();

        std::string value; value.push_back(character);
        return this->create_token(TokenKind::Char, start, value);
    }

    char next = this->next();
    while (next && next != '"') {
        value += this->escape(this->current);
        next = this->next();
    }

    if (this->current != '"') {
        NOTE(this->make_span(start, start), "Unterminated string literal.");
        ERROR(this->make_span(this->loc(), this->loc()), "Expected end of string.");
    }
    
    Token token = this->create_token(TokenKind::String, start, value); this->next();
    return token;
}

Token Lexer::parse_number() {
    std::string value;
    Location start = this->loc();

    value += this->current;

    char next = this->next();
    bool dot = false;

    if (value == "0") {
        if (next == 'x' || next == 'b') {
            value += next; this->next();
            if (next == 'x') {
                while (std::isxdigit(this->current)) {
                    value += this->current;
                    this->next();
                }
            } else {
                while (this->current == '0' || this->current == '1') {
                    value += this->current;
                    this->next();
                }
            }

            return this->create_token(TokenKind::Integer, start, value);
        }
    
        if (this->current != '.') {
            if (std::isdigit(this->peek()) ) {
                ERROR(this->make_span(start, this->loc()), "Leading zeros on integer constants are not allowed");
            }

            return this->create_token(TokenKind::Integer, start, "0");
        }

        dot = true;
        next = this->next();
    }

    while (std::isdigit(next) || next == '.') {
        if (next == '.') {
            if (dot) {
                break;
            }

            dot = true;
        }

        value += next;
        next = this->next();
    }

    if (dot) {
        return this->create_token(TokenKind::Float, start, value);
    } else {
        return this->create_token(TokenKind::Integer, start, value);
    }
}

std::vector<Token> Lexer::lex() {
    std::vector<Token> tokens;

    while (true) {
        if (this->eof) break;

        if (this->current == '\n') {
            if (this->line == UINT32_MAX) {
                ERROR(this->make_span(), "Lexer line overflow. Too many lines in file.");
            }

            this->line++;
            this->column = 0;

            tokens.push_back(this->create_token(TokenKind::Newline, "\n"));
            this->next();

            continue;
        }

        if (std::isspace(this->current)) {
            this->next();
            if (this->current == '\t') {
                this->column += 3;
            }

            continue;
        } else if (std::isdigit(this->current)) {
            tokens.push_back(this->parse_number());
        } else if (this->is_valid_identifier(this->current)) {
            tokens.push_back(this->parse_identifier());
        } else if (this->current == '#') {
            this->skip_comment();
        } else if (this->current == '+') {
            Location start = this->loc();
            Token token;

            char next = this->next();
            if (next == '+') {
                this->next();
                token = this->create_token(TokenKind::Inc, start, "++");
            } else if (next == '=') {
                this->next();
                token = this->create_token(TokenKind::IAdd, start, "+=");
            } else {
                token = this->create_token(TokenKind::Add, start, "+");
            }

            tokens.push_back(token);
        } else if (this->current == '-') {
            Location start = this->loc();
            Token token;

            char next = this->next();
            if (next == '>') {
                this->next();
                token = this->create_token(TokenKind::Arrow, start, "->");
            } else if (next == '-') {
                this->next();
                token = this->create_token(TokenKind::Dec, start, "--");
            } else if (next == '=') {
                this->next();
                token = this->create_token(TokenKind::IMinus, start, "-=");
            } else {
                token = this->create_token(TokenKind::Minus, "-");
            }

            tokens.push_back(token);
        } else if (this->current == '*') {
            Location start = this->loc();
            Token token;

            char next = this->next();
            if (next == '=') {
                this->next();
                token = this->create_token(TokenKind::IMul, start, "*=");
            } else {
                token = this->create_token(TokenKind::Mul, "*");
            }

            tokens.push_back(token);
        } else if (this->current == '/') {
            Location start = this->loc();
            Token token;

            char next = this->next();
            if (next == '=') {
                this->next();
                token = this->create_token(TokenKind::IDiv, start, "/=");
            } else {
                token = this->create_token(TokenKind::Div, "/");
            }

            tokens.push_back(token);
        } else if (this->current == '%') {
            tokens.push_back(this->create_token(TokenKind::Mod, "%"));
            this->next();
        } else if (this->current == '=') {
            Location start = this->loc();
            Token token;

            char next = this->next();
            if (next == '=') {
                this->next();
                token = this->create_token(TokenKind::Eq, start, "==");
            } else if (next == '>') {
                this->next();
                token = this->create_token(TokenKind::DoubleArrow, start, "=>");
            } else {
                token = this->create_token(TokenKind::Assign, "=");
            }

            tokens.push_back(token);
        } else if (this->current == '>') {
            Location start = this->loc();
            Token token;

            char next = this->next();
            if (next == '=') {
                this->next();
                token = this->create_token(TokenKind::Gte, start, ">=");
            } else if (next == '>') {
                this->next();
                token = this->create_token(TokenKind::Rsh, start, ">>");
            } else {
                token = this->create_token(TokenKind::Gt, start, ">");
            }

            tokens.push_back(token);
        } else if (this->current == '<') {
            Location start = this->loc();
            Token token;

            char next = this->next();
            if (next == '=') {
                this->next();
                token = this->create_token(TokenKind::Lte, start, "<=");
            } else if (next == '<') {
                this->next();
                token = this->create_token(TokenKind::Lsh, start, "<<");
            } else {
                token = this->create_token(TokenKind::Lt, start, "<");
            }

            tokens.push_back(token);
        } else if (this->current == '!') {
            Location start = this->loc();
            Token token;

            char next = this->next();
            if (next == '=') {
                this->next();
                token = this->create_token(TokenKind::Neq, start, "!=");
            } else {
                token = this->create_token(TokenKind::Not, start, "!");
            }

            tokens.push_back(token);
        } else if (this->current == '|') {
            Location start = this->loc();
            Token token;

            char next = this->next();
            if (next == '|') {
                this->next();
                token = this->create_token(TokenKind::Or, start, "||");
            } else {
                token = this->create_token(TokenKind::BinaryOr, start, "|");
            }
            
            tokens.push_back(token);
        } else if (this->current == '&') {
            Location start = this->loc();
            Token token;

            char next = this->next();
            if (next == '&') {
                this->next();
                token = this->create_token(TokenKind::And, start, "&&");
            } else {
                token = this->create_token(TokenKind::BinaryAnd, start, "&");
            }

            tokens.push_back(token);
        } else if (this->current == '~') {
            tokens.push_back(this->create_token(TokenKind::BinaryNot, "~"));
            this->next();
        } else if (this->current == '^') {
            tokens.push_back(this->create_token(TokenKind::Xor, "^"));
            this->next();
        } else if (this->current == '(') {
            tokens.push_back(this->create_token(TokenKind::LParen, "("));
            this->next();
        } else if (this->current == ')') {
            tokens.push_back(this->create_token(TokenKind::RParen, ")"));
            this->next();
        } else if (this->current == '{') {
            tokens.push_back(this->create_token(TokenKind::LBrace, "{"));
            this->next();
        } else if (this->current == '}') {
            tokens.push_back(this->create_token(TokenKind::RBrace, "}"));
            this->next();
        } else if (this->current == '[') {
            tokens.push_back(this->create_token(TokenKind::LBracket, "["));
            this->next();
        } else if (this->current == ']') {
            tokens.push_back(this->create_token(TokenKind::RBracket, "]"));
            this->next();
        } else if (this->current == ',') {
            tokens.push_back(this->create_token(TokenKind::Comma, ","));
            this->next();
        } else if (this->current == '.') {
            Location start = this->loc();
            Token token;

            char next = this->next();
            if (next == '.' && this->peek() == '.') {
                this->next(); this->next();
                token = this->create_token(TokenKind::Ellipsis, start, "...");
            } else {
                token = this->create_token(TokenKind::Dot, ".");
            }

            tokens.push_back(token);
        } else if (this->current == ';') {
            tokens.push_back(this->create_token(TokenKind::SemiColon, ";"));
            this->next();
        } else if (this->current == ':') {
            Location start = this->loc();
            Token token;

            char next = this->next();
            if (next == ':') {
                this->next();
                token = this->create_token(TokenKind::DoubleColon, start, "::");
            } else {
                token = this->create_token(TokenKind::Colon, ":");
            }

            tokens.push_back(token);
        } else if (this->current == '"' || this->current == '\'') {
            tokens.push_back(this->parse_string());
        } else if (this->current == '?') {
            tokens.push_back(this->create_token(TokenKind::Maybe, "?"));
            this->next();
        } else {
            ERROR(this->make_span(), "Unexpected character '{0}'", this->current);
        }
    }

    Token eof = {TokenKind::EOS, "\0", this->make_span()};
    tokens.push_back(eof);

    return tokens;
}