#include <quart/lexer/lexer.h>

#include <cctype>

namespace quart {

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

Lexer::Lexer(SourceCode const& source_code) : m_source_code(source_code) {
    m_code = source_code.code();
    auto _ = this->next();
}

ErrorOr<char> Lexer::espace_next() {
    return this->escape(TRY(this->next()));
}

ErrorOr<char> Lexer::escape(char current) {
    if (current != '\\') {
        return current;
    }

    char c = TRY(this->next());
    switch (c) {
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
            return err({ m_offset, m_offset, m_source_code.index() }, "Invalid espace sequence");
    }
}

bool Lexer::is_valid_identifier(char c) {
    return std::isalnum(c) || c == '_';
}

ErrorOr<size_t> Lexer::lex_while(String& buffer, const std::function<bool(char)>& predicate) {
    size_t n = 0;
    while (predicate(m_current)) {
        buffer.push_back(m_current);
        TRY(this->next());

        n++;
    }

    return n;
}

ErrorOr<Token> Lexer::lex_identifier(bool allow_keywords) {
    String value;
    size_t start = m_offset;

    TRY(this->lex_while(value, [this](char c) {
        return this->is_valid_identifier(c);
    }));

    Span span = { start, m_offset, m_source_code.index() };
    if (is_keyword(value) && allow_keywords) {
        return Token { get_keyword_kind(value), move(value), span };
    }

    return Token { TokenKind::Identifier, move(value), span };
}

ErrorOr<Token> Lexer::lex_string() {
    String value;
    size_t start = m_offset;

    if (m_current == '\'') {
        char c = TRY(this->espace_next());
        TRY(this->skip(2));

        value.push_back(c);
        return Token { TokenKind::Char, move(value), { start, m_offset, m_source_code.index() } };
    }

    char next = TRY(this->next());
    while (next && next != '"') {
        value.push_back(TRY(this->escape(m_current)));
        next = TRY(this->next());
    }

    if (m_current != '"') {
        return err({ m_offset, m_offset, m_source_code.index() }, "Expected end of string");
    }

    TRY(this->next());
    return Token { TokenKind::String, move(value), { start, m_offset, m_source_code.index() } };
}

ErrorOr<Token> Lexer::lex_number() {
    String value;
    size_t start = m_offset;

    value.push_back(m_current);

    char next = TRY(this->next());
    bool is_float = false;

    if (value == "0") {
        if (next == 'x' || next == 'b') {
            value.push_back(next);
            TRY(this->next());

            if (next == 'x') {
                TRY(this->lex_while(value, isxdigit));
            } else {
                TRY(this->lex_while(value, [](char c) { return c == '0' || c == '1'; }));
            }

            return Token { TokenKind::Integer, move(value), { start, m_offset, m_source_code.index() } };
        }
    
        if (m_current != '.') {
            if (std::isdigit(this->peek()) ) {
                return err({ start, m_offset, m_source_code.index() }, "Leading zeros on integer literals are not allowed");
            }

            return Token { TokenKind::Integer, move(value), { start, m_offset, m_source_code.index() } };
        }

        is_float = true;
        next = TRY(this->next());

        if (m_current == '.') {
            this->rewind(2);
            return Token { TokenKind::Integer, move(value), { start, m_offset, m_source_code.index() } };
        }
    }

    while (std::isdigit(next) || next == '.' || next == '_') {
        if (next == '.') {
            if (is_float) {
                break;
            }

            is_float = true;
        } else if (next == '_') {
            if (this->peek() == '_') {
                return err({ start, m_offset, m_source_code.index() }, "Invalid integer literal");
            }

            next = TRY(this->next());
            continue;
        }

        value.push_back(next);
        next = TRY(this->next());
    }

    if (value.back() == '.' && m_current == '.') {
        this->rewind(2);
        value.pop_back();

        is_float = false;
    }

    Span span = { start, m_offset, m_source_code.index() };
    if (is_float) {
        return Token { TokenKind::Float, move(value), span }; 
    } else {
        return Token { TokenKind::Integer, move(value), span }; 
    }
}

Optional<Token> Lexer::expect(char prev, ExpectedToken expected) {
    if (m_current != expected.c) {
        return {};
    }

    auto _ = this->next(); // FIXME: Handle the Error

    String value;

    value.push_back(prev);
    value.push_back(expected.c);

    return Token { expected.kind, move(value), { m_offset, m_offset, m_source_code.index() } };
}

ErrorOr<Token> Lexer::once() {
    size_t start = m_offset;

    if (std::isdigit(m_current)) {
        return this->lex_number();
    } else if (this->is_valid_identifier(m_current)) {
        return this->lex_identifier();
    } else if (SINGLE_CHAR_TOKENS.find(m_current) != SINGLE_CHAR_TOKENS.end()) {
        TokenKind kind = SINGLE_CHAR_TOKENS.at(m_current);
        char c = m_current;
        
        TRY(this->next());
        return Token { kind, String(1, c), { start, start, m_source_code.index() } };
    }

    switch (m_current) {
        case '`': {
            TRY(this->next());
            auto token = TRY(this->lex_identifier(false));

            if (m_current != '`') {
                return err({ start, m_offset, m_source_code.index() }, "Expected end of identifier");
            }

            TRY(this->next());
            return token;
        }
        case '+': {
            TRY(this->next());
            Optional<Token> option = this->expect('+',
                ExpectedToken { TokenKind::Inc, '+'}, 
                ExpectedToken { TokenKind::IAdd, '='}
            );

            return option.value_or(Token { TokenKind::Add, "+", { start, m_offset, m_source_code.index() } });
        } case '-': {
            TRY(this->next());
            Optional<Token> option = this->expect('-',
                ExpectedToken { TokenKind::Arrow, '>' },
                ExpectedToken { TokenKind::Dec, '-' },
                ExpectedToken { TokenKind::ISub, '=' }
            );

            return option.value_or(Token { TokenKind::Sub, "-", { start, m_offset, m_source_code.index() } });
        } case '*': {
            TRY(this->next());

            Optional<Token> option = this->expect('*', { TokenKind::IMul, '=' });
            return option.value_or(Token { TokenKind::Mul, "*", { start, m_offset, m_source_code.index() } });
        } case '/': {
            TRY(this->next());

            Optional<Token> option = this->expect('/', { TokenKind::IDiv, '=' });
            return option.value_or(Token { TokenKind::Mul, "*", { start, m_offset, m_source_code.index() } });
        } case '=': {
            TRY(this->next());
            Optional<Token> option = this->expect('=',
                ExpectedToken { TokenKind::Eq, '=' },
                ExpectedToken { TokenKind::FatArrow, '>' }
            );

            return option.value_or(Token { TokenKind::Assign, "=", { start, m_offset, m_source_code.index() } });
        } case '>': {
            TRY(this->next());
            
            Optional<Token> option = this->expect('>', { TokenKind::Gte, '=' });
            return option.value_or(Token { TokenKind::Gt, ">", { start, m_offset, m_source_code.index() } });
        } case '<': {
            TRY(this->next());
            Optional<Token> option = this->expect('<',
                ExpectedToken { TokenKind::Lte, '=' },
                ExpectedToken { TokenKind::Lsh, '<' }
            );

            return option.value_or(Token { TokenKind::Lt, "<", { start, m_offset, m_source_code.index() } });
        } case '!': {
            TRY(this->next());
            
            Optional<Token> option = this->expect('!', { TokenKind::Neq, '=' });
            return option.value_or(Token { TokenKind::Not, "!", { start, m_offset, m_source_code.index() } });
        } case '|': {
            TRY(this->next());
            
            Optional<Token> option = this->expect('|', { TokenKind::LogicalOr, '|' });
            return option.value_or(Token { TokenKind::Or, "|", { start, m_offset, m_source_code.index() } });
        } case '&': {
            TRY(this->next());
            
            Optional<Token> option = this->expect('&', { TokenKind::LogicalAnd, '&' });
            return option.value_or(Token { TokenKind::And, "&", { start, m_offset, m_source_code.index() } });
        } case '.': {
            TRY(this->next());
            if (m_current == '.' && this->peek() == '.') {
                TRY(this->skip(2));
                return Token { TokenKind::Ellipsis, "...", { start, m_offset, m_source_code.index() } };
            }

            Optional<Token> option = this->expect('.', { TokenKind::DoubleDot, '.' });
            return option.value_or(Token { TokenKind::Dot, ".", { start, m_offset, m_source_code.index() } });
        } case ':': {
            TRY(this->next());

            Optional<Token> option = this->expect(':', { TokenKind::DoubleColon, ':' });
            return option.value_or(Token { TokenKind::Colon, ":", { start, m_offset, m_source_code.index() } });
        } 
        case '"': case '\'': {
            return this->lex_string();
        }
        default:
            return err({ start, m_offset, m_source_code.index() }, "Unexpected character '{0}'", m_current);
    }
}

ErrorOr<Vector<Token>> Lexer::lex() {
    Vector<Token> tokens;
    while (!m_eof) {
        if (m_current == '\0') {
            break;
        }

        if (std::isspace(m_current)) {
            TRY(this->next());
            continue;
        } else if (m_current == '#') {
            while (m_current != '\n' && m_current != 0) {
                TRY(this->next());
            }

            continue;
        }

        tokens.push_back(TRY(this->once()));
    }

    Token eof = { TokenKind::EOS, {}, { m_offset, m_offset, m_source_code.index() } };
    tokens.push_back(eof);

    return tokens;
}

ErrorOr<char> Lexer::next() {
    if (m_offset == SIZE_MAX) {
        return err({ m_offset, m_offset, m_source_code.index() }, "Lexer offset overflow");
    }

    if (m_offset > m_code.size()) {
        m_eof = true;
        return '\0';
    }

    m_current = m_code[m_offset];
    m_offset++;

    return m_current;
}

ErrorOr<void> Lexer::skip(size_t n) {
    for (size_t i = 0; i < n; i++) {
        TRY(this->next());
    }

    return {};
}

char Lexer::peek(u32 offset) { 
    return m_code[m_offset + offset]; 
}

char Lexer::prev() { 
    return m_code[m_offset - 1]; 
}

char Lexer::rewind(u32 offset) {
    m_offset -= offset - 1;

    m_current = m_code[m_offset - 1];
    return m_current;
}

}