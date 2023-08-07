#include <quart/lexer/lexer.h>
#include <quart/utils/log.h>

#include <cctype>
#include <ios>
#include <iterator>
#include <string>

static const std::map<char, TokenKind> CHAR_TO_TOKENKIND = {
    {'~', TokenKind::BinaryNot},
    {'(', TokenKind::LParen},
    {')', TokenKind::RParen},
    {'{', TokenKind::LBrace},
    {'}', TokenKind::RBrace},
    {'[', TokenKind::LBracket},
    {']', TokenKind::RBracket},
    {';', TokenKind::SemiColon},
    {',', TokenKind::Comma},
    {'?', TokenKind::Maybe},
    {'%', TokenKind::Mod}
};

bool isxdigit(char c) { return std::isxdigit(c); }

Lexer::Lexer(const std::string& source, const std::string& filename) {
    this->source = source;
    this->filename = filename;

    this->reset();
    this->next();
}

Lexer::Lexer(utils::fs::Path path) {
    this->filename = path.str();
    this->source = path.read().str();
    
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

char Lexer::rewind(uint32_t offset) {
    this->index -= offset - 1;
    this->column -= offset;

    this->current = this->source[this->index - 1];
    return this->current;
}

void Lexer::reset() {
    this->index = 0;
    this->line = 1;
    this->column = 0;
    this->eof = false;
}

bool Lexer::is_keyword(const std::string& word) {
    return KEYWORDS.find(word) != KEYWORDS.end();
}

TokenKind Lexer::get_keyword_kind(const std::string& word) {
    return KEYWORDS.at(word);
}

Token Lexer::create_token(TokenKind type, const std::string& value) {
    return {type, value, this->make_span()};
}

Token Lexer::create_token(TokenKind type, Location start, const std::string& value) {
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
        this->filename,
        line
    };
}

char Lexer::validate_hex_escape() {
    char hex[3] = {
        this->next(),
        this->next(),
        0
    };

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

char Lexer::escape(char current) {
    if (current != '\\') {
        return current;
    }

    switch (this->next()) {
        case 'n':
            return '\n';
        case 't':
            return '\t';
        case 'r':
            return '\r';
        case '\\':
            return '\\';
        case '\'':
            return '\'';
        case '0':
            return '\0';
        case '"':
            return '"';
        case 'x':
            return this->validate_hex_escape();
        case 'u': case 'U': {
            uint32_t expected = current == 'u' ? 4 : 8;
            uint32_t codepoint = 0;

            for (uint32_t i = 0; i < expected; i++) {
                char c = this->next();
                if (!std::isxdigit(c)) {
                    ERROR(this->make_span(), "Invalid unicode escape sequence");
                }

                codepoint *= 16;
                if (c >= '0' && c <= '9') {
                    codepoint += c - '0';
                } else if (c >= 'a' && c <= 'f') {
                    codepoint += c - 'a' + 10;
                } else if (c >= 'A' && c <= 'F') {
                    codepoint += c - 'A' + 10;
                }
            }

            if (codepoint > 0x10FFFF) {
                ERROR(this->make_span(), "Invalid unicode escape sequence");
            }

            return (uint8_t)codepoint;
        }
        default:
            ERROR(this->make_span(), "Invalid escape sequence");
    }
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

uint8_t Lexer::parse_unicode(std::string& buffer, uint8_t current) {  
    uint8_t expected = 0;

    buffer.push_back(current); 
    if (current < 0x80) {
        return 0;
    } else if (current < 0xE0) {
        expected = 1;
    } else if (current < 0xF0) {
        expected = 2;
    } else if (current < 0xF8) {
        expected = 3;
    }

    for (uint8_t i = 0; i < expected; i++) {
        buffer.push_back(this->next());
    }

    return expected;
}

std::string Lexer::parse_while(
    std::string& buffer,
    const Predicate predicate
) {
    while (predicate(this->current)) {
        buffer.push_back(this->current);
        this->next();
    }

    return buffer;
}

void Lexer::skip_comment() {
    while (this->current != '\n' && this->current != 0) {
        this->next();
    }
}

Token Lexer::parse_identifier(bool accept_keywords) {
    std::string value;
    Location start = this->loc();

    this->parse_unicode(value, this->current);
    char next = this->next();

    while (this->is_valid_identifier(next)) {
        this->parse_unicode(value, next);
        next = this->next();
    }

    if (Lexer::is_keyword(value) && accept_keywords) {
        return this->create_token(Lexer::get_keyword_kind(value), start, value);
    } else {
        return this->create_token(TokenKind::Identifier, start, value);
    }
}

Token Lexer::parse_string() {
    std::string value;
    Location start = this->loc();

    if (this->current == '\'') {
        char character = this->escape(this->next());
        this->next(); this->next();

        std::string value(1, character);
        return this->create_token(TokenKind::Char, start, value);
    }

    char next = this->next();
    while (next && next != '"') {
        char escaped = this->escape(this->current);
        value.push_back(escaped);

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

    value.push_back(this->current);

    char next = this->next();
    bool dot = false;

    if (value == "0") {
        if (next == 'x' || next == 'b') {
            value.push_back(next);
            this->next();

            if (next == 'x') {
                this->parse_while(value, isxdigit);
            } else {
                this->parse_while(value, [](char c) { return c == '0' || c == '1'; });
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

        if (this->current == '.') {
            this->rewind(2);
            return this->create_token(TokenKind::Integer, start, "0");
        }
    }

    while (std::isdigit(next) || next == '.' || next == '_') {
        if (next == '.') {
            if (dot) {
                break;
            }

            dot = true;
        } else if (next == '_') {
            if (this->peek() == '_') {
                ERROR(this->make_span(start, this->loc()), "Invalid number literal");
            }

            next = this->next();
            continue;
        }

        value.push_back(next);
        next = this->next();
    }

    if (value.back() == '.' && this->current == '.') {
        this->rewind(2);
        value.pop_back();

        dot = false;
    }

    if (dot) {
        return this->create_token(TokenKind::Float, start, value);
    } else {
        return this->create_token(TokenKind::Integer, start, value);
    }
}

Token Lexer::once() {
    char current = this->current;
    Location start = this->loc();

    if (std::isdigit(current)) {
        return this->parse_number();
    } else if (this->is_valid_identifier(current)) {
        return this->parse_identifier();
    } else if (CHAR_TO_TOKENKIND.find(current) != CHAR_TO_TOKENKIND.end()) {
        TokenKind kind = CHAR_TO_TOKENKIND.at(current);
        this->next();

        return this->create_token(kind, start, std::string(1, current));
    }

    switch (current) {
        case '`': {
            this->next();
            Token token = this->parse_identifier(false);

            if (this->current != '`') {
                ERROR(this->make_span(start, this->loc()), "Expected end of identifier.");
            }

            this->next();
            return token;
        }
        case '+': {
            char next = this->next();
            if (next == '+') {
                this->next();
                return this->create_token(TokenKind::Inc, start, "++");
            } else if (next == '=') {
                this->next();
                return this->create_token(TokenKind::IAdd, start, "+=");
            }

            return this->create_token(TokenKind::Add, start, "+");
        } case '-': {
            char next = this->next();
            if (next == '>') {
                this->next();
                return this->create_token(TokenKind::Arrow, start, "->");
            } else if (next == '-') {
                this->next();
                return this->create_token(TokenKind::Dec, start, "--");
            } else if (next == '=') {
                this->next();
                return this->create_token(TokenKind::IMinus, start, "-=");
            }
            
            return this->create_token(TokenKind::Minus, "-");
        } case '*': {
            char next = this->next();
            if (next == '=') {
                this->next();
                return this->create_token(TokenKind::IMul, start, "*=");
            }
                
            return this->create_token(TokenKind::Mul, "*");
        } case '/': {
            char next = this->next();
            if (next == '=') {
                this->next();
                return this->create_token(TokenKind::IDiv, start, "/=");
            }
                
            return this->create_token(TokenKind::Div, "/");
        } case '=': {
            char next = this->next();
            if (next == '=') {
                this->next();
                return this->create_token(TokenKind::Eq, start, "==");
            } else if (next == '>') {
                this->next();
                return this->create_token(TokenKind::DoubleArrow, start, "=>");
            }

            return this->create_token(TokenKind::Assign, "=");
        } case '>': {
            char next = this->next();
            if (next == '=') {
                this->next();
                return this->create_token(TokenKind::Gte, start, ">=");
            } else if (next == '>') {
                this->next();
                return this->create_token(TokenKind::Rsh, start, ">>");
            }

            return this->create_token(TokenKind::Gt, start, ">");
        } case '<': {
            char next = this->next();
            if (next == '=') {
                this->next();
                return this->create_token(TokenKind::Lte, start, "<=");
            } else if (next == '<') {
                this->next();
                return this->create_token(TokenKind::Lsh, start, "<<");
            }
                
            return this->create_token(TokenKind::Lt, start, "<");
        } case '!': {
            char next = this->next();
            if (next == '=') {
                this->next();
                return this->create_token(TokenKind::Neq, start, "!=");
            }
            
            return this->create_token(TokenKind::Not, start, "!");
        } case '|': {
            char next = this->next();
            if (next == '|') {
                this->next();
                return this->create_token(TokenKind::Or, start, "||");
            }
                
            return this->create_token(TokenKind::BinaryOr, start, "|");
        } case '&': {
            char next = this->next();
            if (next == '&') {
                this->next();
                return this->create_token(TokenKind::And, start, "&&");
            }
            
            return this->create_token(TokenKind::BinaryAnd, start, "&");
        } case '.': {
            char next = this->next();
            if (next == '.' && this->peek() == '.') {
                this->next(); this->next();
                return this->create_token(TokenKind::Ellipsis, start, "...");
            } else if (next == '.') {
                this->next();
                return this->create_token(TokenKind::DoubleDot, start, "..");
            }
            
            return this->create_token(TokenKind::Dot, ".");
        } case ':': {
            char next = this->next();
            if (next == ':') {
                this->next();
                return this->create_token(TokenKind::DoubleColon, start, "::");
            }
            
            return this->create_token(TokenKind::Colon, ":");
        } 
        case '"':
        case '\'':
            return this->parse_string();
        default:
            ERROR(this->make_span(), "Unexpected character '{0}'", this->current);
    }
}

std::vector<Token> Lexer::lex() {
    std::vector<Token> tokens;

    while (!this->eof) {
        if (this->current == '\n') {
            if (this->line == UINT32_MAX) {
                ERROR(this->make_span(), "Lexer line overflow. Too many lines in file.");
            }

            this->line++;
            this->column = 0;

            this->next();
            continue;
        } else if (std::isspace(this->current)) {
            this->next();
            if (this->current == '\t') {
                this->column += 3;
            }

            continue;
        } else if (this->current == '#') {
            this->skip_comment();
            continue;
        }

        tokens.push_back(this->once());
    }

    Token eof = {TokenKind::EOS, "\0", this->make_span()};
    tokens.push_back(eof);

    return tokens;
}