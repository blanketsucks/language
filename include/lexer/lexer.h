#ifndef _LEXER_H
#define _LEXER_H

#include "lexer/tokens.h"

#include <cstdint>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>

class Lexer {
public:
    Lexer(std::string source, std::string filename);
    Lexer(std::fstream& file, std::string filename);
    Lexer(FILE* file, std::string filename);

    char next();
    char peek();
    char prev();

    void reset();

    bool is_keyword(std::string word);

    Token create_token(TokenKind type, std::string value);
    Token create_token(TokenKind type, Location start, std::string value);

    Location loc();

    char escape(char current);

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