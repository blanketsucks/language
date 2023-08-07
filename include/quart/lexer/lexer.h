#pragma once

#include <quart/lexer/tokens.h>
#include <quart/utils/filesystem.h>

#include <cstdint>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <functional>

class Lexer {
public:
    typedef bool (*Predicate)(char);

    Lexer(const std::string& source, const std::string& filename);
    Lexer(utils::fs::Path path);

    static bool is_keyword(const std::string& word);
    static TokenKind get_keyword_kind(const std::string& word);

    char next();
    char peek(uint32_t offset = 0);
    char prev();
    char rewind(uint32_t offset = 1);

    void reset();

    Token create_token(TokenKind type, const std::string& value);
    Token create_token(TokenKind type, Location start, const std::string& value);

    Location loc();

    Span make_span();
    Span make_span(const Location& start, const Location& end);

    char validate_hex_escape();
    char escape(char current);

    std::string parse_while(
        std::string& buffer,
        const Predicate predicate
    );

    bool is_valid_identifier(uint8_t current);

    uint8_t parse_unicode(std::string& buffer, uint8_t current);

    void skip_comment();
    Token parse_identifier(bool accept_keywords = true);
    Token parse_string();
    Token parse_number();

    Token once();
    std::vector<Token> lex();

    uint32_t line;
    uint32_t column;
    size_t index;
    
    bool eof;
    char current;

    std::string filename;
    std::string source;
};
