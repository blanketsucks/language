#pragma once

#include <quart/lexer/tokens.h>
#include <quart/source_code.h>
#include <quart/common.h>
#include <quart/errors.h>

#include <cstdint>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <functional>

namespace quart {

struct ExpectedToken {
    TokenKind kind;
    char c;
};

class Lexer {
public:
    Lexer(RefPtr<SourceCode>);

    char next();
    void skip(size_t n = 1);

    char prev();

    char peek(u32 offset = 0);
    char rewind(u32 offset = 1);

    Optional<Token> expect(char prev, ExpectedToken);

    template<typename ...Args> requires(of_type_v<ExpectedToken, Args...>)
    Optional<Token> expect(char prev, ExpectedToken expected, Args... args) {
        auto option = this->expect(prev, expected);
        if (!option.has_value()) {
            return this->expect(prev, std::forward<Args>(args)...);
        }

        return option.value();
    }

    size_t lex_while(
        String& buffer,
        const std::function<bool(char)>& predicate
    );

    ErrorOr<char> escape(char current);
    ErrorOr<char> espace_next();

    bool is_valid_identifier(char current);

    Token lex_identifier(bool allow_keywords = true);
    ErrorOr<Token> lex_string();
    ErrorOr<Token> lex_number();

    void single_line_comment();
    void multi_line_comment();

    ErrorOr<Token> once();
    ErrorOr<Vector<Token>> lex();

private:
    size_t m_offset = 0;
    
    bool m_eof = false;
    char m_current = 0;

    RefPtr<SourceCode> m_source_code;
    StringView m_code;
};

}