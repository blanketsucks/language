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

bool isxdigit(char c) {
    return std::isxdigit(c);
}

Lexer::Lexer(const std::string& source, std::string filename) {
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

std::string Lexer::parse_while(
    std::string& buffer,
    const Predicate predicate
) {
    while (predicate(this->current)) {
        buffer += this->current;
        this->next();
    }

    return buffer;
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

        value += next;
        next = this->next();
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
                return this->create_token(TokenKind::Range, start, "..");
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