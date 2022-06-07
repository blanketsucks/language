#ifndef _TOKENS_HPP
#define _TOKENS_HPP

#include <vector>
#include <iostream>
#include <map>

enum class TokenType {
    IDENTIFIER,
    KEYWORD,
    INTEGER,
    FLOAT,
    STRING,
    CHARACTER,

    // Operators
    PLUS,
    MINUS,
    MUL,
    DIV,
    NOT,
    OR,
    AND,

    // Binary Operators
    BINARY_OR,
    BINARY_AND,
    BINARY_NOT,
    XOR,
    RSH,
    LSH,

    // Comparison
    EQ,
    NEQ,
    GT,
    LT,
    GTE,
    LTE,

    // Assignment
    ASSIGN,

    // Other
    LPAREN,
    RPAREN,
    LBRACE,
    RBRACE,
    LBRACKET,
    RBRACKET,
    COMMA,
    SEMICOLON,
    COLON,
    DOT,
    ARROW,
    EOS
};

struct Location {
    int column;
    int line;
    int index;
    std::string filename;

    std::string format() {
        return filename + ":" + std::to_string(line) + ":" + std::to_string(column);
    }
};

struct Token {
    TokenType type;
    Location start;
    Location end;
    std::string value;

    bool operator==(TokenType type) {
        return this->type == type;
    }

    bool operator!=(TokenType type) {
        return this->type != type;
    }
};

static std::vector<std::string> KEYWORDS = {
    "extern",
    "def",
    "return",
    "if",
    "else",
    "let",
    "struct",
    "packed",
    "include"
};

static std::vector<std::pair<TokenType, int>> PRECEDENCES = {
    // Assignment operator - 5 precedence.
    std::make_pair(TokenType::ASSIGN, 5),

    // Comparison operators - 10 precedence.
    std::make_pair(TokenType::LT, 10),
    std::make_pair(TokenType::GT, 10),
    std::make_pair(TokenType::LTE, 10),
    std::make_pair(TokenType::GTE, 10),
    std::make_pair(TokenType::EQ, 10),
    std::make_pair(TokenType::NEQ, 10),

    // Binary operators - 20 precedence.
    std::make_pair(TokenType::BINARY_AND, 20),
    std::make_pair(TokenType::BINARY_OR, 20),
    std::make_pair(TokenType::XOR, 20),
    std::make_pair(TokenType::RSH, 20),
    std::make_pair(TokenType::LSH, 20),

    // Arithmic operators - from 30 to 40.
    std::make_pair(TokenType::PLUS, 30),
    std::make_pair(TokenType::MINUS, 30),
    std::make_pair(TokenType::DIV, 40),
    std::make_pair(TokenType::MUL, 40)
};

static std::vector<TokenType> UNARY_OPERATORS = {
    TokenType::NOT,
    TokenType::PLUS,
    TokenType::MINUS,
    TokenType::BINARY_NOT
};


#endif