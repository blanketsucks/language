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
    "return"
};

static std::vector<std::pair<TokenType, int>> PRECEDENCES = {
    // Comparison operators - 10 precedence (the lowest).
    std::pair<TokenType, int>(TokenType::LT, 10),
    std::pair<TokenType, int>(TokenType::GT, 10),
    std::pair<TokenType, int>(TokenType::LTE, 10),
    std::pair<TokenType, int>(TokenType::GTE, 10),
    std::pair<TokenType, int>(TokenType::EQ, 10),
    std::pair<TokenType, int>(TokenType::NEQ, 10),

    // Binary operators - 20 precedence.
    std::pair<TokenType, int>(TokenType::BINARY_AND, 20),
    std::pair<TokenType, int>(TokenType::BINARY_OR, 20),
    std::pair<TokenType, int>(TokenType::XOR, 20),
    std::pair<TokenType, int>(TokenType::RSH, 20),
    std::pair<TokenType, int>(TokenType::LSH, 20),

    // Arithmic operators - from 30 to 40.
    std::pair<TokenType, int>(TokenType::PLUS, 30),
    std::pair<TokenType, int>(TokenType::MINUS, 30),
    std::pair<TokenType, int>(TokenType::DIV, 40),
    std::pair<TokenType, int>(TokenType::MUL, 40)
};

#endif