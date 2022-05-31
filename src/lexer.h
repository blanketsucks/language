#ifndef _LEXER_H
#define _LEXER_H

#include "tokens.hpp"

#include <cstdint>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>

class Lexer {
public:
    Lexer(std::string source, std::string filename);
    Lexer(std::ifstream& file, std::string filename);
    Lexer(FILE* file, std::string filename);

    char next();
    char peek();
    char prev();

    void reset();

    bool is_keyword(std::string word);

    Token create_token(TokenType type, std::string value);
    Token create_token(TokenType type, Location start, std::string value);

    Location loc();

    Token parse_identifier();
    Token parse_string();
    Token parse_number();

    std::vector<Token> lex();

private:
    int line;
    int column;
    int index;
    bool eof;
    char current;
    std::string filename;
    std::string source;
};

#endif