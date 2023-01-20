#ifndef _LEXER_H
#define _LEXER_H

#include "lexer/tokens.h"
#include "lexer/location.h"
#include "utils/filesystem.h"

#include <cstdint>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>

class Lexer {
public:
    Lexer(const std::string& source, std::string filename);
    Lexer(utils::filesystem::Path path);

    static bool is_keyword(std::string word);
    static TokenKind get_keyword_kind(std::string word);

    char next();
    char peek(uint32_t offset = 0);
    char prev();

    void reset();

    Token create_token(TokenKind type, std::string value);
    Token create_token(TokenKind type, Location start, std::string value);

    Location loc();

    Span make_span();
    Span make_span(const Location& start, const Location& end);

    char escape(char current);

    std::string get_identifier_token(uint8_t current, bool );
    bool is_valid_identifier(uint8_t current);
    std::string parse_unicode(uint8_t current);

    void skip_comment();
    Token parse_identifier();
    Token parse_string();
    Token parse_number();

    std::vector<Token> lex();

    uint32_t line;
    uint32_t column;
    uint32_t index;
    
    bool eof;
    char current;

    std::string filename;
    std::string source;
};

#endif