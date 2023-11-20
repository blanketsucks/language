#include <quart/lexer/lexer.h>
#include <quart/logging.h>

#include <cctype>
#include <ios>
#include <iterator>
#include <string>

using namespace quart;

static const std::map<char, TokenKind> SINGLE_CHAR_TOKENS = {
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

Token Lexer::create_token(TokenKind type, const std::string& value) {
    return {type, value, this->make_span()};
}

Token Lexer::create_token(TokenKind type, const Location& start, const std::string& value) {
    return {type, value, this->make_span(start, this->current_location())};
}

Location Lexer::current_location() {
    return Location {
        this->line,
        this->column,
        this->index
    };
}

Span Lexer::make_span() {
    return this->make_span(this->current_location(), this->current_location());
}

Span Lexer::make_span(const Location& start, const Location& end) {
    return Span {
        start,
        end,
        this->filename,
        this->get_line_for(start)
    };
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
        default:
            ERROR(this->make_span(), "Invalid escape sequence");
    }
}

bool Lexer::is_valid_identifier(u8 c) {
    if (std::isalnum(c) || c == '_') {
        return true;
    } else if (c < 128) {
        return false;
    }

    u32 expected = 0;
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

    for (u32 i = 0; i < expected; i++) {
        u8 next = this->peek(i);
        if ((next & 0xC0) != 0x80) {
            return false;
        }
    }

    return true;
}

u8 Lexer::parse_unicode_identifier(std::string& buffer, u8 current) {  
    u8 expected = 0;
    buffer.push_back(static_cast<i8>(current));

    if (current < 0x80) {
        return 0;
    } else if (current < 0xE0) {
        expected = 1;
    } else if (current < 0xF0) {
        expected = 2;
    } else if (current < 0xF8) {
        expected = 3;
    }

    for (u8 i = 0; i < expected; i++) {
        buffer.push_back(this->next());
    }

    return expected;
}

size_t Lexer::lex_while(
    std::string& buffer,
    const std::function<bool(char)>& predicate
) {
    size_t n = 0;
    while (predicate(this->current)) {
        buffer.push_back(this->current);
        this->next();

        n++;
    }

    return n;
}

Token Lexer::lex_identifier(bool accept_keywords) {
    std::string value;
    Location start = this->current_location();

    this->parse_unicode_identifier(value, this->current);
    char next = this->next();

    while (this->is_valid_identifier(next)) {
        this->parse_unicode_identifier(value, next);
        next = this->next();
    }

    if (quart::is_keyword(value) && accept_keywords) {
        return this->create_token(quart::get_keyword_kind(value), start, value);
    } else {
        return this->create_token(TokenKind::Identifier, start, value);
    }
}

Token Lexer::lex_string() {
    std::string value;
    Location start = this->current_location();

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
        ERROR(this->make_span(), "Expected end of string.");
    }
    
    Token token = this->create_token(TokenKind::String, start, value); this->next();
    return token;
}

Token Lexer::lex_number() {
    std::string value;
    Location start = this->current_location();

    value.push_back(this->current);

    char next = this->next();
    bool dot = false;

    if (value == "0") {
        if (next == 'x' || next == 'b') {
            value.push_back(next);
            this->next();

            if (next == 'x') {
                this->lex_while(value, isxdigit);
            } else {
                this->lex_while(value, [](char c) { return c == '0' || c == '1'; });
            }

            return this->create_token(TokenKind::Integer, start, value);
        }
    
        if (this->current != '.') {
            if (std::isdigit(this->peek()) ) {
                ERROR(this->make_span(start, this->current_location()), "Leading zeros on integer constants are not allowed");
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
                ERROR(this->make_span(start, this->current_location()), "Invalid number literal");
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
    Location start = this->current_location();

    if (std::isdigit(current)) {
        return this->lex_number();
    } else if (this->is_valid_identifier(current)) {
        return this->lex_identifier();
    } else if (SINGLE_CHAR_TOKENS.find(current) != SINGLE_CHAR_TOKENS.end()) {
        TokenKind kind = SINGLE_CHAR_TOKENS.at(current);
        this->next();

        return this->create_token(kind, start, std::string(1, current));
    }

    switch (current) {
        case '`': {
            this->next();
            Token token = this->lex_identifier(false);

            if (this->current != '`') {
                ERROR(this->make_span(start, this->current_location()), "Expected end of identifier.");
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
        case '"': case '\'': return this->lex_string();
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
            while (this->current != '\n' && this->current != 0) {
                this->next();
            }

            continue;
        }

        tokens.push_back(this->once());
    }

    Token eof = {TokenKind::EOS, "\0", this->make_span()};
    tokens.push_back(eof);

    return tokens;
}


MemoryLexer::MemoryLexer(std::string source, const std::string& filename) : source(std::move(source)) {
    this->filename = filename;

    this->reset();
    this->next();
}

MemoryLexer::MemoryLexer(fs::Path path) {
    this->filename = path;
    this->source = path.read().str();
    
    this->reset();
    this->next();
}

char MemoryLexer::next() {
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

char MemoryLexer::peek(u32 offset) { 
    return this->source[this->index + offset]; 
}

char MemoryLexer::prev() { 
    return this->source[this->index - 1]; 
}

char MemoryLexer::rewind(u32 offset) {
    this->index -= offset - 1;
    this->column -= offset;

    this->current = this->source[this->index - 1];
    return this->current;
}

void MemoryLexer::reset() {
    this->index = 0;
    this->line = 1;
    this->column = 0;
    this->eof = false;
}

llvm::StringRef MemoryLexer::get_line_for(const Location& location) {
    if (this->lines.find(location.line) == this->lines.end()) {
        size_t offset = location.index - location.column;
        std::string line;

        if (offset < this->source.size()) {
            line = this->source.substr(offset);
            line = line.substr(0, line.find('\n'));
        }

        this->lines[location.line] = line;
    }

    return this->lines[location.line];
}

StreamLexer::StreamLexer(std::ifstream& stream, const std::string& filename) : stream(stream) {
    this->filename = filename;

    this->reset();
    this->next();
}

char StreamLexer::next() {
    if (this->index == UINT32_MAX) {
        ERROR(this->make_span(), "Lexer index overflow.");
    }

    this->current = static_cast<i8>(this->stream.get());
    this->index++;

    if (!this->stream.good()) {
        this->eof = true;
        return this->current;
    }

    this->column++;
    if (this->column == UINT32_MAX) {
        ERROR(this->make_span(), "Lexer column overflow.");
    }

    return this->current;
}

char StreamLexer::peek(u32 offset) { 
    return static_cast<i8>(this->stream.get()); 
}

char StreamLexer::prev() {
    i64 index = static_cast<i64>(this->index);

    this->stream.seekg(index - 1, std::ifstream::beg);
    char c = static_cast<i8>(this->stream.get());

    this->stream.seekg(index, std::ifstream::beg);
    return c;
}

char StreamLexer::rewind(u32 offset) {
    this->index -= offset - 1;
    this->column -= offset;

    i64 index = static_cast<i64>(this->index);

    this->stream.seekg(index - 1, std::ifstream::beg);
    this->current = static_cast<i8>(this->stream.get());

    this->stream.seekg(index, std::ifstream::beg);
    return this->current;
}

void StreamLexer::reset() {
    this->index = 0;
    this->line = 1;
    this->column = 0;
    this->eof = false;
}

llvm::StringRef StreamLexer::get_line_for(const Location& location) {
    auto iterator = this->lines.find(location.line);
    if (iterator == this->lines.end()) {
        i64 offset = static_cast<i64>(location.index - location.column);
        this->stream.seekg(offset, std::ifstream::beg);

        std::string line;
        std::getline(this->stream, line);

        this->stream.seekg(static_cast<i64>(this->index), std::ifstream::beg);
        this->lines[location.line] = line;
    }

    return iterator->second;
}
